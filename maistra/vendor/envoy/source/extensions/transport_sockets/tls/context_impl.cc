#include "extensions/transport_sockets/tls/context_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "envoy/admin/v3/certs.pb.h"
#include "envoy/common/exception.h"
#include "envoy/common/platform.h"
#include "envoy/ssl/ssl_socket_extended_info.h"
#include "envoy/stats/scope.h"
#include "envoy/type/matcher/v3/string.pb.h"

#include "common/common/assert.h"
#include "common/common/base64.h"
#include "common/common/fmt.h"
#include "common/common/hex.h"
#include "common/common/utility.h"
#include "common/network/address_impl.h"
#include "common/protobuf/utility.h"
#include "common/runtime/runtime_features.h"
#include "common/stats/utility.h"

#include "extensions/transport_sockets/tls/openssl_impl.h"
#include "extensions/transport_sockets/tls/utility.h"

#include "absl/container/node_hash_set.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/hmac.h"
#include "openssl/rand.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

int ContextImpl::sslExtendedSocketInfoIndex() {
  CONSTRUCT_ON_FIRST_USE(int, []() -> int {
    int ssl_context_index = SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
    RELEASE_ASSERT(ssl_context_index >= 0, "");
    return ssl_context_index;
  }());
}

ContextImpl::ContextImpl(Stats::Scope& scope, const Envoy::Ssl::ContextConfig& config,
                         TimeSource& time_source)
    : scope_(scope), stats_(generateStats(scope)), time_source_(time_source),
      tls_max_version_(config.maxProtocolVersion()),
      stat_name_set_(scope.symbolTable().makeSet("TransportSockets::Tls")),
      unknown_ssl_cipher_(stat_name_set_->add("unknown_ssl_cipher")),
      unknown_ssl_curve_(stat_name_set_->add("unknown_ssl_curve")),
      unknown_ssl_algorithm_(stat_name_set_->add("unknown_ssl_algorithm")),
      unknown_ssl_version_(stat_name_set_->add("unknown_ssl_version")),
      ssl_ciphers_(stat_name_set_->add("ssl.ciphers")),
      ssl_versions_(stat_name_set_->add("ssl.versions")),
      ssl_curves_(stat_name_set_->add("ssl.curves")),
      ssl_sigalgs_(stat_name_set_->add("ssl.sigalgs")), capabilities_(config.capabilities()) {
  const auto tls_certificates = config.tlsCertificates();

  tls_context_.ssl_ctx_.reset(SSL_CTX_new(TLS_method()));

  int rc = SSL_CTX_set_app_data(tls_context_.ssl_ctx_.get(), this);
  RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));

  rc = SSL_CTX_set_min_proto_version(tls_context_.ssl_ctx_.get(), config.minProtocolVersion());
  RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));

  rc = SSL_CTX_set_max_proto_version(tls_context_.ssl_ctx_.get(), config.maxProtocolVersion());
  RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));

  if (!Envoy::Extensions::TransportSockets::Tls::set_strict_cipher_list(tls_context_.ssl_ctx_.get(), config.cipherSuites().c_str())) {
    // Break up a set of ciphers into each individual cipher and try them each individually in
    // order to attempt to log which specific one failed. Example of config.cipherSuites():
    // "-ALL:[ECDHE-ECDSA-AES128-GCM-SHA256|ECDHE-ECDSA-CHACHA20-POLY1305]:ECDHE-ECDSA-AES128-SHA".
    //
    // "-" is both an operator when in the leading position of a token (-ALL: don't allow this
    // cipher), and the common separator in names (ECDHE-ECDSA-AES128-GCM-SHA256). Don't split on
    // it because it will separate pieces of the same cipher. When it is a leading character, it
    // is removed below.
    std::vector<absl::string_view> ciphers =
        StringUtil::splitToken(config.cipherSuites(), ":+![|]", false);
    std::vector<std::string> bad_ciphers;
    for (const auto& cipher : ciphers) {
      std::string cipher_str(cipher);

      if (absl::StartsWith(cipher_str, "-")) {
        cipher_str.erase(cipher_str.begin());
      }

      if (!Envoy::Extensions::TransportSockets::Tls::set_strict_cipher_list(tls_context_.ssl_ctx_.get(), cipher_str.c_str())) {
        bad_ciphers.push_back(cipher_str);
      }
    }
    throw EnvoyException(fmt::format("Failed to initialize cipher suites {}. The following "
                                     "ciphers were rejected when tried individually: {}",
                                     config.cipherSuites(), absl::StrJoin(bad_ciphers, ", ")));
  }

  if (!SSL_CTX_set1_curves_list(tls_context_.ssl_ctx_.get(), config.ecdhCurves().c_str())) {
    throw EnvoyException(absl::StrCat("Failed to initialize ECDH curves ", config.ecdhCurves()));
  }

  int verify_mode = SSL_VERIFY_NONE;
  int verify_mode_validation_context = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;

  if (config.certificateValidationContext() != nullptr) {
    envoy::extensions::transport_sockets::tls::v3::CertificateValidationContext::
        TrustChainVerification verification =
            config.certificateValidationContext()->trustChainVerification();
    if (verification == envoy::extensions::transport_sockets::tls::v3::
                            CertificateValidationContext::ACCEPT_UNTRUSTED) {
      verify_mode = SSL_VERIFY_PEER; // Ensure client-certs will be requested even if we have
                                     // nothing to verify against
      verify_mode_validation_context = SSL_VERIFY_PEER;
    }
  }

  if (config.certificateValidationContext() != nullptr &&
      !config.certificateValidationContext()->caCert().empty() &&
      !config.capabilities().provides_certificates) {
    ca_file_path_ = config.certificateValidationContext()->caCertPath();
    bssl::UniquePtr<BIO> bio(
        BIO_new_mem_buf(const_cast<char*>(config.certificateValidationContext()->caCert().data()),
                        config.certificateValidationContext()->caCert().size()));
    RELEASE_ASSERT(bio != nullptr, "");
    // Based on BoringSSL's X509_load_cert_crl_file().
    bssl::UniquePtr<STACK_OF(X509_INFO)> list(
        PEM_X509_INFO_read_bio(bio.get(), nullptr, nullptr, nullptr));
    if (list == nullptr) {
      throw EnvoyException(absl::StrCat("Failed to load trusted CA certificates from ",
                                        config.certificateValidationContext()->caCertPath()));
    }

    X509_STORE* store = SSL_CTX_get_cert_store(tls_context_.ssl_ctx_.get());
    bool has_crl = false;
    for (const X509_INFO* item : list.get()) {
      if (item->x509) {
        X509_STORE_add_cert(store, item->x509);
        if (ca_cert_ == nullptr) {
          X509_up_ref(item->x509);
          ca_cert_.reset(item->x509);
        }
      }
      if (item->crl) {
        X509_STORE_add_crl(store, item->crl);
        has_crl = true;
      }

      // TODO (dmitri-d) why do we do this here? Upstream doesn't
      Envoy::Extensions::TransportSockets::Tls::ssl_ctx_add_client_CA(tls_context_.ssl_ctx_.get(),
                                                                        item->x509);
    }
    if (ca_cert_ == nullptr) {
      throw EnvoyException(fmt::format("Failed to load trusted CA certificates from {}",
                                       config.certificateValidationContext()->caCertPath()));
    }
    if (has_crl) {
      X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
    }
    verify_mode = SSL_VERIFY_PEER;
    verify_trusted_ca_ = true;

    // NOTE: We're using SSL_CTX_set_cert_verify_callback() instead of X509_verify_cert()
    // directly. However, our new callback is still calling X509_verify_cert() under
    // the hood. Therefore, to ignore cert expiration, we need to set the callback
    // for X509_verify_cert to ignore that error.
    if (config.certificateValidationContext()->allowExpiredCertificate()) {
      X509_STORE_set_verify_cb(store, ContextImpl::ignoreCertificateExpirationCallback);
    }
  }

  if (config.certificateValidationContext() != nullptr &&
      !config.certificateValidationContext()->certificateRevocationList().empty()) {
    bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(
        const_cast<char*>(
            config.certificateValidationContext()->certificateRevocationList().data()),
        config.certificateValidationContext()->certificateRevocationList().size()));
    RELEASE_ASSERT(bio != nullptr, "");

    // Based on BoringSSL's X509_load_cert_crl_file().
    bssl::UniquePtr<STACK_OF(X509_INFO)> list(
        PEM_X509_INFO_read_bio(bio.get(), nullptr, nullptr, nullptr));
    if (list == nullptr) {
      throw EnvoyException(
          absl::StrCat("Failed to load CRL from ",
                       config.certificateValidationContext()->certificateRevocationListPath()));
    }

    X509_STORE* store = SSL_CTX_get_cert_store(tls_context_.ssl_ctx_.get());
    for (const X509_INFO* item : list.get()) {
      if (item->crl) {
        X509_STORE_add_crl(store, item->crl);
      }
    }

    X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
  }


  const Envoy::Ssl::CertificateValidationContextConfig* cert_validation_config =
      config.certificateValidationContext();
  if (cert_validation_config != nullptr) {
    if (!cert_validation_config->verifySubjectAltNameList().empty()) {
      verify_subject_alt_name_list_ = cert_validation_config->verifySubjectAltNameList();
      verify_mode = verify_mode_validation_context;
    }

    if (!cert_validation_config->subjectAltNameMatchers().empty()) {
      for (const envoy::type::matcher::v3::StringMatcher& matcher :
           cert_validation_config->subjectAltNameMatchers()) {
        subject_alt_name_matchers_.push_back(Matchers::StringMatcherImpl(matcher));
      }
      verify_mode = verify_mode_validation_context;
    }

    if (!cert_validation_config->verifyCertificateHashList().empty()) {
      for (auto hash : cert_validation_config->verifyCertificateHashList()) {
        // Remove colons from the 95 chars long colon-separated "fingerprint"
        // in order to get the hex-encoded string.
        if (hash.size() == 95) {
          hash.erase(std::remove(hash.begin(), hash.end(), ':'), hash.end());
        }
        const auto& decoded = Hex::decode(hash);
        if (decoded.size() != SHA256_DIGEST_LENGTH) {
          throw EnvoyException(absl::StrCat("Invalid hex-encoded SHA-256 ", hash));
        }
        verify_certificate_hash_list_.push_back(decoded);
      }
      verify_mode = verify_mode_validation_context;
    }

    if (!cert_validation_config->verifyCertificateSpkiList().empty()) {
      for (const auto& hash : cert_validation_config->verifyCertificateSpkiList()) {
        const auto decoded = Base64::decode(hash);
        if (decoded.size() != SHA256_DIGEST_LENGTH) {
          throw EnvoyException(absl::StrCat("Invalid base64-encoded SHA-256 ", hash));
        }
        verify_certificate_spki_list_.emplace_back(decoded.begin(), decoded.end());
      }
      verify_mode = verify_mode_validation_context;
    }
  }

  if (!capabilities_.verifies_peer_certificates) {
    if (verify_mode != SSL_VERIFY_NONE) {
      SSL_CTX_set_verify(tls_context_.ssl_ctx_.get(), verify_mode, nullptr);
      SSL_CTX_set_cert_verify_callback(tls_context_.ssl_ctx_.get(), ContextImpl::verifyCallback, this);
    }
  }

  absl::node_hash_set<int> cert_pkey_ids;
  if (!capabilities_.provides_certificates) {
    for (uint32_t i = 0; i < tls_certificates.size(); ++i) {
      // Load certificate chain.
      const auto& tls_certificate = tls_certificates[i].get();
      tls_context_.cert_chain_file_path_ = tls_certificate.certificateChainPath();
      bssl::UniquePtr<BIO> bio(
          BIO_new_mem_buf(const_cast<char*>(tls_certificate.certificateChain().data()),
                          tls_certificate.certificateChain().size()));
      RELEASE_ASSERT(bio != nullptr, "");
      tls_context_.cert_chain_.reset(PEM_read_bio_X509_AUX(bio.get(), nullptr, nullptr, nullptr));
      if (tls_context_.cert_chain_ == nullptr ||
          !SSL_CTX_use_certificate(tls_context_.ssl_ctx_.get(), tls_context_.cert_chain_.get())) {
        while (uint64_t err = ERR_get_error()) {
          ENVOY_LOG_MISC(error, "SSL error: {}:{}:{}:{}", err, ERR_lib_error_string(err),
                         ERR_func_error_string(err), ERR_GET_REASON(err),
                         ERR_reason_error_string(err));
        }
        throw EnvoyException(
            absl::StrCat("Failed to load certificate chain from ", tls_context_.cert_chain_file_path_, ", please see log for details"));
      }
      // Read rest of the certificate chain.
      while (true) {
        bssl::UniquePtr<X509> cert(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
        if (cert == nullptr) {
          break;
        }
        if (!SSL_CTX_add_extra_chain_cert(tls_context_.ssl_ctx_.get(), cert.get())) {
          throw EnvoyException(
              absl::StrCat("Failed to load certificate chain from ", tls_context_.cert_chain_file_path_));
        }
        // SSL_CTX_add_extra_chain_cert() takes ownership.
        cert.release();
      }
      // Check for EOF.
      const uint32_t err = ERR_peek_last_error();
      if (ERR_GET_LIB(err) == ERR_LIB_PEM && ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
        ERR_clear_error();
      } else {
        throw EnvoyException(
            absl::StrCat("Failed to load certificate chain from ", tls_context_.cert_chain_file_path_));
      }

      // The must staple extension means the certificate promises to carry
      // with it an OCSP staple. https://tools.ietf.org/html/rfc7633#section-6
      constexpr absl::string_view tls_feature_ext = "1.3.6.1.5.5.7.1.24";
      constexpr absl::string_view must_staple_ext_value = "\x30\x3\x02\x01\x05";
      auto must_staple = Utility::getCertificateExtensionValue(*tls_context_.cert_chain_, tls_feature_ext);
      if (must_staple == must_staple_ext_value) {
        tls_context_.is_must_staple_ = true;
      }

      bssl::UniquePtr<EVP_PKEY> public_key(X509_get_pubkey(tls_context_.cert_chain_.get()));
      const int pkey_id = EVP_PKEY_id(public_key.get());
      if (!cert_pkey_ids.insert(pkey_id).second) {
        throw EnvoyException(fmt::format("Failed to load certificate chain from {}, at most one "
                                         "certificate of a given type may be specified",
										 tls_context_.cert_chain_file_path_));
      }
      tls_context_.is_ecdsa_ = pkey_id == EVP_PKEY_EC;
      switch (pkey_id) {
      case EVP_PKEY_EC: {
        // We only support P-256 ECDSA today.
        const EC_KEY* ecdsa_public_key = EVP_PKEY_get0_EC_KEY(public_key.get());
        // Since we checked the key type above, this should be valid.
        ASSERT(ecdsa_public_key != nullptr);
        const EC_GROUP* ecdsa_group = EC_KEY_get0_group(ecdsa_public_key);
        if (ecdsa_group == nullptr ||
            EC_GROUP_get_curve_name(ecdsa_group) != NID_X9_62_prime256v1) {
          throw EnvoyException(fmt::format("Failed to load certificate chain from {}, only P-256 "
                                           "ECDSA certificates are supported",
                                           tls_context_.cert_chain_file_path_));
        }
        tls_context_.is_ecdsa_ = true;
      } break;
      case EVP_PKEY_RSA: {
        // We require RSA certificates with 2048-bit or larger keys.
        const RSA* rsa_public_key = EVP_PKEY_get0_RSA(public_key.get());
        // Since we checked the key type above, this should be valid.
        ASSERT(rsa_public_key != nullptr);
        const unsigned rsa_key_length = RSA_size(rsa_public_key);

        if (rsa_key_length < 2048 / 8) {
          throw EnvoyException(
              fmt::format("Failed to load certificate chain from {}, only RSA "
                          "certificates with 2048-bit or larger keys are supported",
                          tls_context_.cert_chain_file_path_));
        } 
      } break;
      }

      Envoy::Ssl::PrivateKeyMethodProviderSharedPtr private_key_method_provider =
          tls_certificate.privateKeyMethod();
      // We either have a private key or a BoringSSL private key method provider.
      if (private_key_method_provider) {
        throw EnvoyException(
            fmt::format("Private key provider configured, but not supported"));
/*
      ctx.private_key_method_provider_ = private_key_method_provider;
      // The provider has a reference to the private key method for the context lifetime.
      Ssl::BoringSslPrivateKeyMethodSharedPtr private_key_method =
          private_key_method_provider->getBoringSslPrivateKeyMethod();
      if (private_key_method == nullptr) {
      }
      SSL_CTX_set_private_key_method(ctx.ssl_ctx_.get(), private_key_method.get());
*/
      } else {
        // Load private key.
        bio.reset(BIO_new_mem_buf(const_cast<char *>(tls_certificate.privateKey().data()),
                                  tls_certificate.privateKey().size()));
        RELEASE_ASSERT(bio != nullptr, "");
        bssl::UniquePtr<EVP_PKEY> pkey(
            PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr,
                                    !tls_certificate.password().empty()
                                        ? const_cast<char*>(tls_certificate.password().c_str())
                                        : nullptr));
        if (pkey == nullptr || !SSL_CTX_use_PrivateKey(tls_context_.ssl_ctx_.get(), pkey.get())) {
          throw EnvoyException(
              absl::StrCat("Failed to load private key from ", tls_certificate.privateKeyPath()));
        }
      }
    }
  }

  // use the server's cipher list preferences
  SSL_CTX_set_options(tls_context_.ssl_ctx_.get(), SSL_OP_CIPHER_SERVER_PREFERENCE);

  if (config.certificateValidationContext() != nullptr) {
    allow_untrusted_certificate_ =
        config.certificateValidationContext()->trustChainVerification() ==
        envoy::extensions::transport_sockets::tls::v3::CertificateValidationContext::
            ACCEPT_UNTRUSTED;
  }

  parsed_alpn_protocols_ = parseAlpnProtocols(config.alpnProtocols());

  // To enumerate the required builtin ciphers, curves, algorithms, and
  // versions, uncomment '#define LOG_BUILTIN_STAT_NAMES' below, and run
  //  bazel test //test/extensions/transport_sockets/tls/... --test_output=streamed
  //      | grep " Builtin ssl." | sort | uniq
  // #define LOG_BUILTIN_STAT_NAMES
  //
  // TODO(#8035): improve tooling to find any other built-ins needed to avoid
  // contention.

  // Ciphers
  stat_name_set_->rememberBuiltin("AEAD-AES128-GCM-SHA256");
  stat_name_set_->rememberBuiltin("ECDHE-ECDSA-AES128-GCM-SHA256");
  stat_name_set_->rememberBuiltin("ECDHE-RSA-AES128-GCM-SHA256");
  stat_name_set_->rememberBuiltin("ECDHE-RSA-AES128-SHA");
  stat_name_set_->rememberBuiltin("ECDHE-RSA-CHACHA20-POLY1305");
  stat_name_set_->rememberBuiltin("TLS_AES_128_GCM_SHA256");

  // Curves from
  // https://github.com/google/boringssl/blob/f4d8b969200f1ee2dd872ffb85802e6a0976afe7/ssl/ssl_key_share.cc#L384
  stat_name_set_->rememberBuiltins(
      {"P-224", "P-256", "P-384", "P-521", "X25519", "CECPQ2", "CECPQ2b"});

  // Algorithms
  stat_name_set_->rememberBuiltins({"ecdsa_secp256r1_sha256", "rsa_pss_rsae_sha256"});

  // Versions
  stat_name_set_->rememberBuiltins({"TLSv1", "TLSv1.1", "TLSv1.2", "TLSv1.3"});
}

int ServerContextImpl::alpnSelectCallback(const unsigned char** out, unsigned char* outlen,
                                          const unsigned char* in, unsigned int inlen) {
  // Currently this uses the standard selection algorithm in priority order.
  const uint8_t* alpn_data = parsed_alpn_protocols_.data();
  size_t alpn_data_size = parsed_alpn_protocols_.size();

  if (SSL_select_next_proto(const_cast<unsigned char**>(out), outlen, alpn_data, alpn_data_size, in,
                            inlen) != OPENSSL_NPN_NEGOTIATED) {
    return SSL_TLSEXT_ERR_NOACK;
  } else {
    return SSL_TLSEXT_ERR_OK;
  }
}

std::vector<uint8_t> ContextImpl::parseAlpnProtocols(const std::string& alpn_protocols) {
  if (alpn_protocols.empty()) {
    return {};
  }

  if (alpn_protocols.size() >= 65535) {
    throw EnvoyException("Invalid ALPN protocol string");
  }

  std::vector<uint8_t> out(alpn_protocols.size() + 1);
  size_t start = 0;
  for (size_t i = 0; i <= alpn_protocols.size(); i++) {
    if (i == alpn_protocols.size() || alpn_protocols[i] == ',') {
      if (i - start > 255) {
        throw EnvoyException("Invalid ALPN protocol string");
      }

      out[start] = i - start;
      start = i + 1;
    } else {
      out[i + 1] = alpn_protocols[i];
    }
  }

  return out;
}

bssl::UniquePtr<SSL> ContextImpl::newSsl(const Network::TransportSocketOptions*) {
  // We use the first certificate for a new SSL object, later in the
  // SSL_CTX_set_select_certificate_cb() callback following ClientHello, we replace with the
  // selected certificate via SSL_set_SSL_CTX().
  return bssl::UniquePtr<SSL>(SSL_new(tls_context_.ssl_ctx_.get()));
}

int ContextImpl::ignoreCertificateExpirationCallback(int ok, X509_STORE_CTX* ctx) {
  if (!ok) {
    int err = X509_STORE_CTX_get_error(ctx);
    if (err == X509_V_ERR_CERT_HAS_EXPIRED || err == X509_V_ERR_CERT_NOT_YET_VALID) {
      return 1;
    }
  }

  return ok;
}

int ContextImpl::verifyCallback(X509_STORE_CTX* store_ctx, void* arg) {
  ContextImpl* impl = reinterpret_cast<ContextImpl*>(arg);
  SSL* ssl = reinterpret_cast<SSL*>(
      X509_STORE_CTX_get_ex_data(store_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));

  X509* cert = X509_STORE_CTX_get_current_cert(store_ctx);
  if (cert == nullptr) {
    cert = X509_STORE_CTX_get0_cert(store_ctx);
  }

  return impl->doVerifyCertChain(
      store_ctx,
      reinterpret_cast<Envoy::Ssl::SslExtendedSocketInfo*>(
          SSL_get_ex_data(ssl, ContextImpl::sslExtendedSocketInfoIndex())),
      *cert, static_cast<const Network::TransportSocketOptions*>(SSL_get_app_data(ssl)));
}

int ContextImpl::doVerifyCertChain(
    X509_STORE_CTX* store_ctx, Ssl::SslExtendedSocketInfo* ssl_extended_info, X509& leaf_cert,
    const Network::TransportSocketOptions* transport_socket_options) {
  if (verify_trusted_ca_) {
    int ret = X509_verify_cert(store_ctx);
    if (ssl_extended_info) {
      ssl_extended_info->setCertificateValidationStatus(
          ret == 1 ? Envoy::Ssl::ClientValidationStatus::Validated
                   : Envoy::Ssl::ClientValidationStatus::Failed);
    }

    if (ret <= 0) {
      stats_.fail_verify_error_.inc();
      return allow_untrusted_certificate_ ? 1 : ret;
    }
  }

  Envoy::Ssl::ClientValidationStatus validated = verifyCertificate(
      &leaf_cert,
      transport_socket_options &&
              !transport_socket_options->verifySubjectAltNameListOverride().empty()
          ? transport_socket_options->verifySubjectAltNameListOverride()
          : verify_subject_alt_name_list_,
      subject_alt_name_matchers_);

  if (ssl_extended_info) {
    if (ssl_extended_info->certificateValidationStatus() ==
        Envoy::Ssl::ClientValidationStatus::NotValidated) {
      ssl_extended_info->setCertificateValidationStatus(validated);
    } else if (validated != Envoy::Ssl::ClientValidationStatus::NotValidated) {
      ssl_extended_info->setCertificateValidationStatus(validated);
    }
  }

  return allow_untrusted_certificate_ ? 1
                                      : (validated != Envoy::Ssl::ClientValidationStatus::Failed);
}

Envoy::Ssl::ClientValidationStatus ContextImpl::verifyCertificate(
    X509* cert, const std::vector<std::string>& verify_san_list,
    const std::vector<Matchers::StringMatcherImpl>& subject_alt_name_matchers) {
  Envoy::Ssl::ClientValidationStatus validated = Envoy::Ssl::ClientValidationStatus::NotValidated;

  if (!verify_san_list.empty()) {
    if (!verifySubjectAltName(cert, verify_san_list)) {
      stats_.fail_verify_san_.inc();
      return Envoy::Ssl::ClientValidationStatus::Failed;
    }
    validated = Envoy::Ssl::ClientValidationStatus::Validated;
  }

  if (!subject_alt_name_matchers.empty() && !matchSubjectAltName(cert, subject_alt_name_matchers)) {
    stats_.fail_verify_san_.inc();
    return Envoy::Ssl::ClientValidationStatus::Failed;
  }

  if (!verify_certificate_hash_list_.empty() || !verify_certificate_spki_list_.empty()) {
    const bool valid_certificate_hash =
        !verify_certificate_hash_list_.empty() &&
        verifyCertificateHashList(cert, verify_certificate_hash_list_);
    const bool valid_certificate_spki =
        !verify_certificate_spki_list_.empty() &&
        verifyCertificateSpkiList(cert, verify_certificate_spki_list_);

    if (!valid_certificate_hash && !valid_certificate_spki) {
      stats_.fail_verify_cert_hash_.inc();
      return Envoy::Ssl::ClientValidationStatus::Failed;
    }

    validated = Envoy::Ssl::ClientValidationStatus::Validated;
  }

  return validated;
}

void ContextImpl::incCounter(const Stats::StatName name, absl::string_view value,
                             const Stats::StatName fallback) const {
  Stats::Counter& counter = Stats::Utility::counterFromElements(
      scope_, {name, stat_name_set_->getBuiltin(value, fallback)});
  counter.inc();

#ifdef LOG_BUILTIN_STAT_NAMES
  std::cerr << absl::StrCat("Builtin ", symbol_table.toString(name), ": ", value, "\n")
            << std::flush;
#endif
}

void ContextImpl::logHandshake(SSL* ssl) const {
  stats_.handshake_.inc();

  if (SSL_session_reused(ssl)) {
    stats_.session_reused_.inc();
  }

  incCounter(ssl_ciphers_, SSL_get_cipher_name(ssl), unknown_ssl_cipher_);
  incCounter(ssl_versions_, SSL_get_version(ssl), unknown_ssl_version_);

  //uint16_t curve_id = SSL_get_curve_id(ssl);
  //if (curve_id) {
  //  incCounter(ssl_curves_, SSL_get_curve_name(curve_id), unknown_ssl_curve_);
  //}
  int group = SSL_get_shared_group(ssl, NULL);
  if (group > 0) {
    switch (group) {
      case NID_X25519:
        incCounter(ssl_curves_, "X25519", unknown_ssl_curve_);
        break;
      case NID_X9_62_prime256v1:
        incCounter(ssl_curves_, "P-256", unknown_ssl_curve_);
        break;
      default:
        incCounter(ssl_curves_, "", unknown_ssl_curve_);
        // case NID_secp384r1: {
        //	scope_.counter(fmt::format("ssl.curves.{}", "P-384")).inc();
        //} break;
    }
  }

  // TODO (dmitri-d) sort out ssl_sigalgs_ stats
  //uint16_t sigalg_id = SSL_get_peer_signature_algorithm(ssl);
  //if (sigalg_id) {
  //  const char *sigalg = SSL_get_signature_algorithm_name(sigalg_id, 1 /* include curve */);
  //  incCounter(ssl_sigalgs_, sigalg, unknown_ssl_algorithm_);
  //}

  bssl::UniquePtr<X509> cert(SSL_get_peer_certificate(ssl));
  if (!cert.get()) {
    stats_.no_certificate_.inc();
  }
}

std::vector<Ssl::PrivateKeyMethodProviderSharedPtr> ContextImpl::getPrivateKeyMethodProviders() {
  std::vector<Envoy::Ssl::PrivateKeyMethodProviderSharedPtr> providers;

  Envoy::Ssl::PrivateKeyMethodProviderSharedPtr provider =
      tls_context_.getPrivateKeyMethodProvider();
  if (provider) {
    providers.push_back(provider);
  }

  return providers;
}

bool ContextImpl::matchSubjectAltName(
    X509* cert, const std::vector<Matchers::StringMatcherImpl>& subject_alt_name_matchers) {
  bssl::UniquePtr<GENERAL_NAMES> san_names(
      static_cast<GENERAL_NAMES*>(X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr)));
  if (san_names == nullptr) {
    return false;
  }
  for (const GENERAL_NAME* general_name : san_names.get()) {
    const std::string san = Utility::generalNameAsString(general_name);
    for (auto& config_san_matcher : subject_alt_name_matchers) {
      // For DNS SAN, if the StringMatcher type is exact, we have to follow DNS matching semantics.
      if (general_name->type == GEN_DNS &&
                  config_san_matcher.matcher().match_pattern_case() ==
                      envoy::type::matcher::v3::StringMatcher::MatchPatternCase::kExact
              ? dnsNameMatch(config_san_matcher.matcher().exact(), absl::string_view(san))
              : config_san_matcher.match(san)) {
        return true;
      }
    }
  }
  return false;
}

bool ContextImpl::verifySubjectAltName(X509* cert,
                                       const std::vector<std::string>& subject_alt_names) {
  bssl::UniquePtr<GENERAL_NAMES> san_names(
      static_cast<GENERAL_NAMES*>(X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr)));
  if (san_names == nullptr) {
    return false;
  }
  for (const GENERAL_NAME* general_name : san_names.get()) {
    const std::string san = Utility::generalNameAsString(general_name);
    for (auto& config_san : subject_alt_names) {
      if (general_name->type == GEN_DNS ? dnsNameMatch(config_san, san.c_str())
                                        : config_san == san) {
        return true;
      }
    }
  }
  return false;
}

bool ContextImpl::dnsNameMatch(const absl::string_view dns_name, const absl::string_view pattern) {
  if (dns_name == pattern) {
    return true;
  }

  size_t pattern_len = pattern.length();
  if (pattern_len > 1 && pattern[0] == '*' && pattern[1] == '.') {
    if (dns_name.length() > pattern_len - 1) {
      const size_t off = dns_name.length() - pattern_len + 1;
      if (Runtime::runtimeFeatureEnabled("envoy.reloadable_features.fix_wildcard_matching")) {
        return dns_name.substr(0, off).find('.') == std::string::npos &&
               dns_name.substr(off, pattern_len - 1) == pattern.substr(1, pattern_len - 1);
      } else {
        return dns_name.substr(off, pattern_len - 1) == pattern.substr(1, pattern_len - 1);
      }
    }
  }

  return false;
}

bool ContextImpl::verifyCertificateHashList(
    X509* cert, const std::vector<std::vector<uint8_t>>& expected_hashes) {
  std::vector<uint8_t> computed_hash(SHA256_DIGEST_LENGTH);
  unsigned int n;
  X509_digest(cert, EVP_sha256(), computed_hash.data(), &n);
  RELEASE_ASSERT(n == computed_hash.size(), "");

  for (const auto& expected_hash : expected_hashes) {
    if (computed_hash == expected_hash) {
      return true;
    }
  }
  return false;
}

bool ContextImpl::verifyCertificateSpkiList(
    X509* cert, const std::vector<std::vector<uint8_t>>& expected_hashes) {
  X509_PUBKEY* pubkey = X509_get_X509_PUBKEY(cert);
  if (pubkey == nullptr) {
    return false;
  }
  uint8_t* spki = nullptr;
  const int len = i2d_X509_PUBKEY(pubkey, &spki);
  if (len < 0) {
    return false;
  }
  bssl::UniquePtr<uint8_t> free_spki(spki);

  std::vector<uint8_t> computed_hash(SHA256_DIGEST_LENGTH);
  SHA256(spki, len, computed_hash.data());

  for (const auto& expected_hash : expected_hashes) {
    if (computed_hash == expected_hash) {
      return true;
    }
  }
  return false;
}

SslStats ContextImpl::generateStats(Stats::Scope& store) {
  std::string prefix("ssl.");
  return {ALL_SSL_STATS(POOL_COUNTER_PREFIX(store, prefix), POOL_GAUGE_PREFIX(store, prefix),
                        POOL_HISTOGRAM_PREFIX(store, prefix))};
}

size_t ContextImpl::daysUntilFirstCertExpires() const {
  int daysUntilExpiration = Utility::getDaysUntilExpiration(ca_cert_.get(), time_source_);
  daysUntilExpiration = std::min<int>(
      Utility::getDaysUntilExpiration(tls_context_.cert_chain_.get(), time_source_), daysUntilExpiration);
  if (daysUntilExpiration < 0) { // Ensure that the return value is unsigned
    return 0;
  }
  return daysUntilExpiration;
}

Envoy::Ssl::CertificateDetailsPtr ContextImpl::getCaCertInformation() const {
  if (ca_cert_ == nullptr) {
    return nullptr;
  }
  return certificateDetails(ca_cert_.get(), getCaFileName(), nullptr);
}

std::vector<Envoy::Ssl::CertificateDetailsPtr> ContextImpl::getCertChainInformation() const {
  std::vector<Envoy::Ssl::CertificateDetailsPtr> cert_details;
  if (tls_context_.cert_chain_ == nullptr) {
    return cert_details;
  }
  cert_details.emplace_back(certificateDetails(tls_context_.cert_chain_.get(), tls_context_.getCertChainFileName(),
                                                 tls_context_.ocsp_response_.get()));
  return cert_details;
}

Envoy::Ssl::CertificateDetailsPtr
ContextImpl::certificateDetails(X509* cert, const std::string& path,
                                const Ocsp::OcspResponseWrapper* ocsp_response) const {
  Envoy::Ssl::CertificateDetailsPtr certificate_details =
      std::make_unique<envoy::admin::v3::CertificateDetails>();
  certificate_details->set_path(path);
  certificate_details->set_serial_number(Utility::getSerialNumberFromCertificate(*cert));
  certificate_details->set_days_until_expiration(
      Utility::getDaysUntilExpiration(cert, time_source_));
  ProtobufWkt::Timestamp* valid_from = certificate_details->mutable_valid_from();
  TimestampUtil::systemClockToTimestamp(Utility::getValidFrom(*cert), *valid_from);
  ProtobufWkt::Timestamp* expiration_time = certificate_details->mutable_expiration_time();
  TimestampUtil::systemClockToTimestamp(Utility::getExpirationTime(*cert), *expiration_time);

  for (auto& dns_san : Utility::getSubjectAltNames(*cert, GEN_DNS)) {
    envoy::admin::v3::SubjectAlternateName& subject_alt_name =
        *certificate_details->add_subject_alt_names();
    subject_alt_name.set_dns(dns_san);
  }
  for (auto& uri_san : Utility::getSubjectAltNames(*cert, GEN_URI)) {
    envoy::admin::v3::SubjectAlternateName& subject_alt_name =
        *certificate_details->add_subject_alt_names();
    subject_alt_name.set_uri(uri_san);
  }
  for (auto& ip_san : Utility::getSubjectAltNames(*cert, GEN_IPADD)) {
    envoy::admin::v3::SubjectAlternateName& subject_alt_name =
        *certificate_details->add_subject_alt_names();
    subject_alt_name.set_ip_address(ip_san);
  }
  return certificate_details;
}

ClientContextImpl::ClientContextImpl(Stats::Scope& scope,
                                     const Envoy::Ssl::ClientContextConfig& config,
                                     TimeSource& time_source)
    : ContextImpl(scope, config, time_source),
      server_name_indication_(config.serverNameIndication()),
      allow_renegotiation_(config.allowRenegotiation()),
      max_session_keys_(config.maxSessionKeys()) {
  if (!parsed_alpn_protocols_.empty()) {
    const int rc = SSL_CTX_set_alpn_protos(tls_context_.ssl_ctx_.get(), parsed_alpn_protocols_.data(),
                                           parsed_alpn_protocols_.size());
    RELEASE_ASSERT(rc == 0, Utility::getLastCryptoError().value_or(""));
  }

  if (!config.signingAlgorithmsForTest().empty()) {
    const uint16_t sigalgs = parseSigningAlgorithmsForTest(config.signingAlgorithmsForTest());
    RELEASE_ASSERT(sigalgs != 0, fmt::format("unsupported signing algorithm {}",
                                             config.signingAlgorithmsForTest()));

    //TODO (dmitri-d) verify this
    //const int rc = SSL_CTX_set_verify_algorithm_prefs(tls_context_.ssl_ctx_.get(), &sigalgs, 1);
    const int rc = SSL_CTX_set1_sigalgs_list(tls_context_.ssl_ctx_.get(), config.signingAlgorithmsForTest().c_str());
    RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
  }

  if (max_session_keys_ > 0) {
    SSL_CTX_set_session_cache_mode(tls_context_.ssl_ctx_.get(), SSL_SESS_CACHE_CLIENT);
    SSL_CTX_sess_set_new_cb(
        tls_context_.ssl_ctx_.get(), [](SSL* ssl, SSL_SESSION* session) -> int {
          ContextImpl* context_impl =
              static_cast<ContextImpl*>(SSL_CTX_get_app_data(SSL_get_SSL_CTX(ssl)));
          ClientContextImpl* client_context_impl = dynamic_cast<ClientContextImpl*>(context_impl);
          RELEASE_ASSERT(client_context_impl != nullptr, ""); // for Coverity
          return client_context_impl->newSessionKey(session);
        });
  }
}

bool ContextImpl::parseAndSetAlpn(const std::vector<std::string>& alpn, SSL& ssl) {
  std::vector<uint8_t> parsed_alpn = parseAlpnProtocols(absl::StrJoin(alpn, ","));
  if (!parsed_alpn.empty()) {
    const int rc = SSL_set_alpn_protos(&ssl, parsed_alpn.data(), parsed_alpn.size());
    // This should only if memory allocation fails, e.g. OOM.
    RELEASE_ASSERT(rc == 0, Utility::getLastCryptoError().value_or(""));
    return true;
  }

  return false;
}

bssl::UniquePtr<SSL> ClientContextImpl::newSsl(const Network::TransportSocketOptions* options) {
  bssl::UniquePtr<SSL> ssl_con(ContextImpl::newSsl(options));

  const std::string server_name_indication = options && options->serverNameOverride().has_value()
                                                 ? options->serverNameOverride().value()
                                                 : server_name_indication_;

  if (!server_name_indication.empty()) {
    const int rc = SSL_set_tlsext_host_name(ssl_con.get(), server_name_indication.c_str());
    RELEASE_ASSERT(rc, Utility::getLastCryptoError().value_or(""));
  }

  if (options && !options->verifySubjectAltNameListOverride().empty()) {
    SSL_set_app_data(ssl_con.get(), options);
    SSL_set_verify(ssl_con.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
  }

  // We determine what ALPN using the following precedence:
  // 1. Option-provided ALPN override.
  // 2. ALPN statically configured in the upstream TLS context.
  // 3. Option-provided ALPN fallback.

  // At this point in the code the ALPN has already been set (if present) to the value specified in
  // the TLS context. We've stored this value in parsed_alpn_protocols_ so we can check that to see
  // if it's already been set.
  bool has_alpn_defined = !parsed_alpn_protocols_.empty();
  if (options) {
    // ALPN override takes precedence over TLS context specified, so blindly overwrite it.
    has_alpn_defined |= parseAndSetAlpn(options->applicationProtocolListOverride(), *ssl_con);
  }

  if (options && !has_alpn_defined && options->applicationProtocolFallback().has_value()) {
    // If ALPN hasn't already been set (either through TLS context or override), use the fallback.
    parseAndSetAlpn({*options->applicationProtocolFallback()}, *ssl_con);
  }

  if (allow_renegotiation_) {
    Envoy::Extensions::TransportSockets::Tls::allowRenegotiation(ssl_con.get());
  }

  if (max_session_keys_ > 0) {
    if (session_keys_single_use_) {
      // Stored single-use session keys, use write/write locks.
      absl::WriterMutexLock l(&session_keys_mu_);
      if (!session_keys_.empty()) {
        // Use the most recently stored session key, since it has the highest
        // probability of still being recognized/accepted by the server.
        SSL_SESSION* session = session_keys_.front().get();
        SSL_set_session(ssl_con.get(), session);
        // Remove single-use session key (TLS 1.3) after first use.
        if (Envoy::Extensions::TransportSockets::Tls::should_be_single_use(session)) {
          session_keys_.pop_front();
        }
      }
    } else {
      // Never stored single-use session keys, use read/write locks.
      absl::ReaderMutexLock l(&session_keys_mu_);
      if (!session_keys_.empty()) {
        // Use the most recently stored session key, since it has the highest
        // probability of still being recognized/accepted by the server.
        SSL_SESSION* session = session_keys_.front().get();
        SSL_set_session(ssl_con.get(), session);
      }
    }
  }

  return ssl_con;
}

int ClientContextImpl::newSessionKey(SSL_SESSION* session) {
  // In case we ever store single-use session key (TLS 1.3),
  // we need to switch to using write/write locks.
  if (Envoy::Extensions::TransportSockets::Tls::should_be_single_use(session)) {
    session_keys_single_use_ = true;
  }
  absl::WriterMutexLock l(&session_keys_mu_);
  // Evict oldest entries.
  while (session_keys_.size() >= max_session_keys_) {
    session_keys_.pop_back();
  }
  // Add new session key at the front of the queue, so that it's used first.
  session_keys_.push_front(bssl::UniquePtr<SSL_SESSION>(session));
  return 1; // Tell BoringSSL that we took ownership of the session.
}

uint16_t ClientContextImpl::parseSigningAlgorithmsForTest(const std::string& sigalgs) {
  // This is used only when testing RSA/ECDSA certificate selection, so only the signing algorithms
  // used in tests are supported here.
  if (sigalgs == "rsa_pss_rsae_sha256") {
    return 0x0804; //SSL_SIGN_RSA_PSS_RSAE_SHA256
  } else if (sigalgs == "ecdsa_secp256r1_sha256") {
    return 0x0403; //SSL_SIGN_ECDSA_SECP256R1_SHA256
  }
  return 0;
}

ServerContextImpl::ServerContextImpl(Stats::Scope& scope,
                                     const Envoy::Ssl::ServerContextConfig& config,
                                     const std::vector<std::string>& server_names,
                                     TimeSource& time_source)
    : ContextImpl(scope, config, time_source), session_ticket_keys_(config.sessionTicketKeys()),
      ocsp_staple_policy_(config.ocspStaplePolicy()) {
  if (config.tlsCertificates().empty() && !config.capabilities().provides_certificates) {
    throw EnvoyException("Server TlsCertificates must have a certificate specified");
  }

  // Compute the session context ID hash. We use all the certificate identities,
  // since we should have a common ID for session resumption no matter what cert
  // is used. We do this early because it can throw an EnvoyException.
  const SessionContextID session_id = generateHashForSessionContextId(server_names);

  const auto tls_certificates = config.tlsCertificates();
  if (!config.capabilities().verifies_peer_certificates &&
      config.certificateValidationContext() != nullptr &&
      !config.certificateValidationContext()->caCert().empty()) {
    tls_context_.addClientValidationContext(*config.certificateValidationContext(),
                                   config.requireClientCertificate());
  }

  if (!parsed_alpn_protocols_.empty() && !config.capabilities().handles_alpn_selection) {
    SSL_CTX_set_alpn_select_cb(
        tls_context_.ssl_ctx_.get(),
        [](SSL*, const unsigned char** out, unsigned char* outlen, const unsigned char* in,
           unsigned int inlen, void* arg) -> int {
          return static_cast<ServerContextImpl*>(arg)->alpnSelectCallback(out, outlen, in, inlen);
        },
        this);
  }

  if (config.disableStatelessSessionResumption()) {
    SSL_CTX_set_options(tls_context_.ssl_ctx_.get(), SSL_OP_NO_TICKET);
  } else if (!session_ticket_keys_.empty() && !config.capabilities().handles_alpn_selection) {
    SSL_CTX_set_tlsext_ticket_key_cb(
        tls_context_.ssl_ctx_.get(),
        +[](SSL* ssl, uint8_t* key_name, uint8_t* iv, EVP_CIPHER_CTX* ctx, HMAC_CTX* hmac_ctx,
           int encrypt) -> int {
          ContextImpl* context_impl =
              static_cast<ContextImpl*>(SSL_CTX_get_app_data(SSL_get_SSL_CTX(ssl)));
          ServerContextImpl* server_context_impl = dynamic_cast<ServerContextImpl*>(context_impl);
          RELEASE_ASSERT(server_context_impl != nullptr, ""); // for Coverity
          return server_context_impl->sessionTicketProcess(ssl, key_name, iv, ctx, hmac_ctx,
                                                           encrypt);
        });
  }

  if (config.sessionTimeout() && !config.capabilities().handles_session_resumption) {
    auto timeout = config.sessionTimeout().value().count();
    SSL_CTX_set_timeout(tls_context_.ssl_ctx_.get(), uint32_t(timeout));
  }

  int rc =
      SSL_CTX_set_session_id_context(tls_context_.ssl_ctx_.get(), session_id.data(), session_id.size());
  RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));

//    
// TODO (dmitri-d) return to this to sort out certificate stapling/ocsp response parsing
//
//    for (uint32_t i = 0; i < tls_certificates.size(); ++i) {
//      auto& ocsp_resp_bytes = tls_certificates[i].get().ocspStaple();
//      if (ocsp_resp_bytes.empty()) {
//        if (Runtime::runtimeFeatureEnabled(
//                "envoy.reloadable_features.require_ocsp_response_for_must_staple_certs") &&
//            tls_context_.is_must_staple_) {
//          throw EnvoyException("OCSP response is required for must-staple certificate");
//        }
//        if (ocsp_staple_policy_ == Ssl::ServerContextConfig::OcspStaplePolicy::MustStaple) {
//          throw EnvoyException("Required OCSP response is missing from TLS context");
//        }
//      } else {
//        auto response = std::make_unique<Ocsp::OcspResponseWrapper>(ocsp_resp_bytes, time_source_);
//        if (!response->matchesCertificate(*tls_context_.cert_chain_)) {
//          throw EnvoyException("OCSP response does not match its TLS certificate");
//        }
//        tls_context_.ocsp_response_ = std::move(response);
//      }
//    }
}

ServerContextImpl::SessionContextID
ServerContextImpl::generateHashForSessionContextId(const std::vector<std::string>& server_names) {
  uint8_t hash_buffer[EVP_MAX_MD_SIZE];
  unsigned hash_length;

  // md is released before we return from the method
  EVP_MD_CTX* md = Envoy::Extensions::TransportSockets::Tls::newEVP_MD_CTX();

  int rc = EVP_DigestInit(md, EVP_sha256());
  RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));

  // Hash the CommonName/SANs of all the server certificates. This makes sure that sessions can only
  // be resumed to certificate(s) for the same name(s), but allows resuming to unique certs in the
  // case that different Envoy instances each have their own certs. All certificates in a
  // ServerContextImpl context are hashed together, since they all constitute a match on a filter
  // chain for resumption purposes.
  if (!capabilities_.provides_certificates) {
    X509* cert = SSL_CTX_get0_certificate(tls_context_.ssl_ctx_.get());
    RELEASE_ASSERT(cert != nullptr, "TLS context should have an active certificate");
    X509_NAME* cert_subject = X509_get_subject_name(cert);
    RELEASE_ASSERT(cert_subject != nullptr, "TLS certificate should have a subject");

    const int cn_index = X509_NAME_get_index_by_NID(cert_subject, NID_commonName, -1);
    if (cn_index >= 0) {
      X509_NAME_ENTRY* cn_entry = X509_NAME_get_entry(cert_subject, cn_index);
      RELEASE_ASSERT(cn_entry != nullptr, "certificate subject CN should be present");

      ASN1_STRING* cn_asn1 = X509_NAME_ENTRY_get_data(cn_entry);
      if (ASN1_STRING_length(cn_asn1) <= 0) {
        throw EnvoyException("Invalid TLS context has an empty subject CN");
      }

      rc = EVP_DigestUpdate(md, ASN1_STRING_data(cn_asn1), ASN1_STRING_length(cn_asn1));
      RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
      }

    unsigned san_count = 0;
    bssl::UniquePtr<GENERAL_NAMES> san_names(static_cast<GENERAL_NAMES*>(
        X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr)));

    if (san_names != nullptr) {
      for (const GENERAL_NAME* san : san_names.get()) {
        switch (san->type) {
        case GEN_IPADD:
          rc = EVP_DigestUpdate(md, san->d.iPAddress->data, san->d.iPAddress->length);
          RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
          ++san_count;
          break;
        case GEN_DNS:
          rc = EVP_DigestUpdate(md, ASN1_STRING_data(san->d.dNSName),
                                ASN1_STRING_length(san->d.dNSName));
          RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
          ++san_count;
          break;
        case GEN_URI:
          rc = EVP_DigestUpdate(md, ASN1_STRING_data(san->d.uniformResourceIdentifier),
                                ASN1_STRING_length(san->d.uniformResourceIdentifier));
          RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
          ++san_count;
          break;
        }
      }
    }

    // It's possible that the certificate doesn't have a subject, but
    // does have SANs. Make sure that we have one or the other.
    if (cn_index < 0 && san_count == 0) {
      throw EnvoyException("Invalid TLS context has neither subject CN nor SAN names");
    }

    rc = X509_NAME_digest(X509_get_issuer_name(cert), EVP_sha256(), hash_buffer, &hash_length);
    RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
    RELEASE_ASSERT(hash_length == SHA256_DIGEST_LENGTH,
                   fmt::format("invalid SHA256 hash length {}", hash_length));

    rc = EVP_DigestUpdate(md, hash_buffer, hash_length);
    RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
  }

  // Hash all the settings that affect whether the server will allow/accept
  // the client connection. This ensures that the client is always validated against
  // the correct settings, even if session resumption across different listeners
  // is enabled.
  if (ca_cert_ != nullptr) {
    rc = X509_digest(ca_cert_.get(), EVP_sha256(), hash_buffer, &hash_length);
    RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
    RELEASE_ASSERT(hash_length == SHA256_DIGEST_LENGTH,
                   fmt::format("invalid SHA256 hash length {}", hash_length));

    rc = EVP_DigestUpdate(md, hash_buffer, hash_length);
    RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));

    // verify_subject_alt_name_list_ can only be set with a ca_cert
    for (const std::string& name : verify_subject_alt_name_list_) {
      rc = EVP_DigestUpdate(md, name.data(), name.size());
      RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
    }
  }

  for (const auto& hash : verify_certificate_hash_list_) {
    rc = EVP_DigestUpdate(md, hash.data(),
                          hash.size() *
                              sizeof(std::remove_reference<decltype(hash)>::type::value_type));
    RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
  }

  for (const auto& hash : verify_certificate_spki_list_) {
    rc = EVP_DigestUpdate(md, hash.data(),
                          hash.size() *
                              sizeof(std::remove_reference<decltype(hash)>::type::value_type));
    RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
  }

  // Hash configured SNIs for this context, so that sessions cannot be resumed across different
  // filter chains, even when using the same server certificate.
  for (const auto& name : server_names) {
    rc = EVP_DigestUpdate(md, name.data(), name.size());
    RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
  }

  SessionContextID session_id;

  // Ensure that the output size of the hash we are using is no greater than
  // TLS session ID length that we want to generate.
  static_assert(session_id.size() == SHA256_DIGEST_LENGTH, "hash size mismatch");
  static_assert(session_id.size() == SSL_MAX_SSL_SESSION_ID_LENGTH, "TLS session ID size mismatch");

  rc = EVP_DigestFinal(md, session_id.data(), &hash_length);
  RELEASE_ASSERT(rc == 1, Utility::getLastCryptoError().value_or(""));
  RELEASE_ASSERT(hash_length == session_id.size(),
                 "SHA256 hash length must match TLS Session ID size");

  EVP_MD_CTX_free(md);
  return session_id;
}

int ServerContextImpl::sessionTicketProcess(SSL*, uint8_t* key_name, uint8_t* iv,
                                            EVP_CIPHER_CTX* ctx, HMAC_CTX* hmac_ctx, int encrypt) {
  const EVP_MD* hmac = EVP_sha256();
  const EVP_CIPHER* cipher = EVP_aes_256_cbc();

  if (encrypt == 1) {
    // Encrypt
    RELEASE_ASSERT(!session_ticket_keys_.empty(), "");
    // TODO(ggreenway): validate in SDS that session_ticket_keys_ cannot be empty,
    // or if we allow it to be emptied, reconfigure the context so this callback
    // isn't set.

    const Envoy::Ssl::ServerContextConfig::SessionTicketKey& key = session_ticket_keys_.front();

    static_assert(std::tuple_size<decltype(key.name_)>::value == SSL_TICKET_KEY_NAME_LEN,
                  "Expected key.name length");
    std::copy_n(key.name_.begin(), SSL_TICKET_KEY_NAME_LEN, key_name);

    const int rc = RAND_bytes(iv, EVP_CIPHER_iv_length(cipher));
    ASSERT(rc);

    // This RELEASE_ASSERT is logically a static_assert, but we can't actually get
    // EVP_CIPHER_key_length(cipher) at compile-time
    RELEASE_ASSERT(key.aes_key_.size() == EVP_CIPHER_key_length(cipher), "");
    if (!EVP_EncryptInit_ex(ctx, cipher, nullptr, key.aes_key_.data(), iv)) {
      return -1;
    }

    if (!HMAC_Init_ex(hmac_ctx, key.hmac_key_.data(), key.hmac_key_.size(), hmac, nullptr)) {
      return -1;
    }

    return 1; // success
  } else {
    // Decrypt
    bool is_enc_key = true; // first element is the encryption key
    for (const Envoy::Ssl::ServerContextConfig::SessionTicketKey& key : session_ticket_keys_) {
      static_assert(std::tuple_size<decltype(key.name_)>::value == SSL_TICKET_KEY_NAME_LEN,
                    "Expected key.name length");
      if (std::equal(key.name_.begin(), key.name_.end(), key_name)) {
        if (!HMAC_Init_ex(hmac_ctx, key.hmac_key_.data(), key.hmac_key_.size(), hmac, nullptr)) {
          return -1;
        }

        RELEASE_ASSERT(key.aes_key_.size() == EVP_CIPHER_key_length(cipher), "");
        if (!EVP_DecryptInit_ex(ctx, cipher, nullptr, key.aes_key_.data(), iv)) {
          return -1;
        }

        // If our current encryption was not the decryption key, renew
        return is_enc_key ? 1  // success; do not renew
                          : 2; // success: renew key
      }
      is_enc_key = false;
    }

    return 0; // decryption failed
  }
}

void ServerContextImpl::TlsContext::addClientValidationContext(
    const Envoy::Ssl::CertificateValidationContextConfig& config, bool require_client_cert) {
  bssl::UniquePtr<BIO> bio(
      BIO_new_mem_buf(const_cast<char*>(config.caCert().data()), config.caCert().size()));
  RELEASE_ASSERT(bio != nullptr, "");
  // Based on BoringSSL's SSL_add_file_cert_subjects_to_stack().
  bssl::UniquePtr<STACK_OF(X509_NAME)> list(sk_X509_NAME_new(
      [](const X509_NAME* const* a, const X509_NAME* const* b) -> int { return X509_NAME_cmp(*a, *b); }));
  RELEASE_ASSERT(list != nullptr, "");
  for (;;) {
    bssl::UniquePtr<X509> cert(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
    if (cert == nullptr) {
      break;
    }
    X509_NAME* name = X509_get_subject_name(cert.get());
    if (name == nullptr) {
      throw EnvoyException(
          absl::StrCat("Failed to load trusted client CA certificates from ", config.caCertPath()));
    }
    // Check for duplicates.
    if (sk_X509_NAME_find(list.get(), nullptr, name)) {
      continue;
    }
    bssl::UniquePtr<X509_NAME> name_dup(X509_NAME_dup(name));
    if (name_dup == nullptr || !sk_X509_NAME_push(list.get(), name_dup.release())) {
      throw EnvoyException(
          absl::StrCat("Failed to load trusted client CA certificates from ", config.caCertPath()));
    }
  }
  // Check for EOF.
  const uint32_t err = ERR_peek_last_error();
  if (ERR_GET_LIB(err) == ERR_LIB_PEM && ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
    ERR_clear_error();
  } else {
    throw EnvoyException(
        absl::StrCat("Failed to load trusted client CA certificates from ", config.caCertPath()));
  }
  if (sk_X509_NAME_num(list.get()) > 0)
    SSL_CTX_set_client_CA_list(ssl_ctx_.get(), list.release());

  // SSL_VERIFY_PEER or stronger mode was already set in ContextImpl::ContextImpl().
  if (require_client_cert) {
    SSL_CTX_set_verify(ssl_ctx_.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
  }
}

bool ContextImpl::verifyCertChain(X509& leaf_cert, STACK_OF(X509) & intermediates,
                                  std::string& error_details) {
  bssl::UniquePtr<X509_STORE_CTX> ctx(X509_STORE_CTX_new());
  // It doesn't matter which SSL context is used, because they share the same
  // cert validation config.
  X509_STORE* store = SSL_CTX_get_cert_store(tls_context_.ssl_ctx_.get());
  if (!X509_STORE_CTX_init(ctx.get(), store, &leaf_cert, &intermediates)) {
    error_details = "Failed to verify certificate chain: X509_STORE_CTX_init";
    return false;
  }

  int res = doVerifyCertChain(ctx.get(), nullptr, leaf_cert, nullptr);
  if (res <= 0) {
    const int n = X509_STORE_CTX_get_error(ctx.get());
    const int depth = X509_STORE_CTX_get_error_depth(ctx.get());
    error_details = absl::StrCat("X509_verify_cert: certificate verification error at depth ",
                                 depth, ": ", X509_verify_cert_error_string(n));
    return false;
  }
  return true;
}

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
