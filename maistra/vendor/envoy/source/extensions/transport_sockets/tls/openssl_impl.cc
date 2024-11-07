#include "source/extensions/transport_sockets/tls/openssl_impl.h"

#include <set>
#include <vector>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>

#include "openssl/crypto.h"
#include "openssl/hmac.h"
#include "openssl/rand.h"
#include "openssl/ssl.h"
#include "openssl/x509v3.h"
#include "openssl/err.h"
#include <openssl/safestack.h>

#include "source/common/common/logger.h"

#include <arpa/inet.h>

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

const unsigned char* extract_ext_str_value(const unsigned char *data, size_t& data_len, unsigned int type) {
    // https://github.com/openssl/openssl/blob/12a765a5235f181c2f4992b615eb5f892c368e88/test/handshake_helper.c#L150
    unsigned int len, remaining = data_len;
    const unsigned char *p = data;
    if ( p && remaining > 2 ) {
        // ntohs?
        len = (*(p++) << 8);
        len += *(p++);
        if (len + 2 == remaining) {
            remaining = len;
            if (remaining > 0 && *p++ == type) {
                remaining--;
                /* Now we can finally pull out the byte array with the actual extn value. */
                if (remaining > 2) {
                    len = (*(p++) << 8);
                    len += *(p++);
                    if (len + 2 == remaining) {
                        data_len = len;
                        return p;
                    }
                }
            }
        }
    }

    data_len = 0;
    return nullptr;
}

/**
 * Retrieve SNI value
 * @param ssl
 * @return string_view of name (may be empy)
 */
absl::string_view SSL_extract_client_hello_sni_host_name(const SSL* ssl) {
    const unsigned char* p;
    size_t len;

    // debug begin
    int *extension_ids;
    size_t extension_ids_len;

//    if ( SSL_client_hello_get1_extensions_present(const_cast<SSL*>(ssl), &extension_ids, &extension_ids_len)) {
//        for (size_t i = 0; i < extension_ids_len; i++) {
//            printf("Extension %d is %u\n", i, extension_ids[i]);
//        }
//    }
//
//    OPENSSL_free(extension_ids);

    // debug end
    if ( SSL_client_hello_get0_ext(const_cast<SSL*>(ssl), TLSEXT_TYPE_server_name, &p, &len) ) {
        if ( (p = extract_ext_str_value(p, len, TLSEXT_NAMETYPE_host_name)) != nullptr ) {
            return absl::string_view(reinterpret_cast<const char *>(p), len);
        }
    }

    return absl::string_view();
}

template<typename C>
std::string string_of_collection(const C &c) {
  std::ostringstream list;

  for (auto it = c.begin(); it != c.end(); ++it) {
    list << ((it == c.begin()) ? "" : ":") << *it;
  }

  return list.str();
}

int SSL_CTX_set_strict_cipher_list(SSL_CTX *ctx, const char *str) {
  // TLS 1.3 Cipher Suite Names (See RFC 8446 Section B.4)
  // https://datatracker.ietf.org/doc/html/rfc8446#appendix-B.4
  static const std::set<std::string> supported_tls1_3 {
      "TLS_AES_128_GCM_SHA256",
      "TLS_AES_256_GCM_SHA384",
      "TLS_CHACHA20_POLY1305_SHA256",
      "TLS_AES_128_CCM_8_SHA256",
      "TLS_AES_128_CCM_SHA256",
  };

  // if no cipher to apply then fallback to default
  // the prior implementation of this function return 1 if str was empty
  if ( str == nullptr ||
      strlen(str) == 0 )
    return 1;

  std::vector<std::string> tls_set;
  std::vector<std::string> tls1_3_set;

  std::string dup (str); // Take a modifiable copy for strtok()
  char* token = strtok(dup.data(), ":+![|]");
  while (token != nullptr) {
    if (supported_tls1_3.find(token) != supported_tls1_3.end()) {
      tls1_3_set.emplace_back(token);
    }
    else {
      tls_set.emplace_back(token);
    }
    token = strtok(nullptr, ":[]|");
  }

  // at this point any specific TLS 1.3 context should be removed from the tls_set
  // this allows calling the respective OpenSSL API Set calls as the TLS1_3 call is stricter
  // and can't have any none TLS 1.3 codes.
  // order here is important set 1.3 then 1.2
  std::ostringstream error;
  if (!tls1_3_set.empty()) {
    std::string s_1_3 = string_of_collection(tls1_3_set);
    if (SSL_CTX_set_ciphersuites(ctx, s_1_3.c_str()) == 0) { // 1.3
      char buf[256];
      unsigned long error_1_3 = ERR_get_error();
      ERR_error_string_n(error_1_3, buf, sizeof(buf));
      ENVOY_LOG_MISC(error, "Unable to apply tlsv1.3 cipher {} : {}", s_1_3, buf);
    }
  }

  // if the min protocol version is 1.3 then we don't want any 1.2 ciphers
  if (!tls_set.empty()) {
    std::string s_other = string_of_collection(tls_set);
    // this api call is more permissive than SSL_CTX_set_ciphersuites
    // if there is a bogus cipher it will be ignored.
    // it is possible that the ctx will reference TLS 1.2 ciphers by default
    // these are filtered on connection based on the min/max protocol versions.
    if (SSL_CTX_set_cipher_list(ctx, s_other.c_str()) == 0) {
      char buf[256];
      unsigned long error_1_2 = ERR_get_error();
      ERR_error_string_n(error_1_2, buf, sizeof(buf));
      ENVOY_LOG_MISC(error, "Unable to apply cipher {} : {}", s_other, buf);
      error << "Unable to apply cipher " << s_other << " : " << buf << std::endl;
    }
  }

  if (!error.str().empty()) {
    ENVOY_LOG_MISC(error, "Error configuring ciphers: {}", error.str());
    return 0;
  }

  // now check for any missing from either set now that context is configured
  // this is to ensure that any other openssl processing that may have occurred
  // is accounted for.
  STACK_OF(SSL_CIPHER)* ciphers = reinterpret_cast<STACK_OF(SSL_CIPHER)*>(SSL_CTX_get_ciphers(ctx));
  std::set<std::string> ctx_set;
  for (int i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
    const SSL_CIPHER* cipher = sk_SSL_CIPHER_value(ciphers, i);
    std::string str2(SSL_CIPHER_get_name(cipher));
    ctx_set.insert(str2);
  }
  ENVOY_LOG_MISC(debug, "SSL_CTX set to {}", string_of_collection(ctx_set));

  bool found = true;
  if (!tls1_3_set.empty()) {
    for(const auto &e : tls1_3_set) {
      ENVOY_LOG_MISC(debug, "checking tls1.3 cipher {}", e);
      if ( ctx_set.find(e) == ctx_set.end() ) {
        found = false;
        break;
      }
    }
  }

  // check the not tls 1.3 case
  if (!tls_set.empty()) {
    for (const auto &e: tls_set) {
      ENVOY_LOG_MISC(debug, "checking cipher {}", e);
      if (ctx_set.find(e) == ctx_set.end()
          && e != "ALL"
          && e != "-ALL") {
        found = false;
        break;
      }
    }
  }

  if ( !found ) {
    return 0;
  }

  return 1;
}

/*
 * BoringSSL only returns: TLS1_3_VERSION, TLS1_2_VERSION, or SSL3_VERSION
 */
uint16_t SSL_CIPHER_get_min_version(const SSL_CIPHER *cipher) {
    // This logic was copied from BoringSSL's ssl_cipher.cc

    if ((SSL_CIPHER_get_kx_nid(cipher) == NID_kx_any) ||
        (SSL_CIPHER_get_auth_nid(cipher) == NID_auth_any)) {
        return TLS1_3_VERSION;
    }

    const EVP_MD *digest = SSL_CIPHER_get_handshake_digest(cipher);
    if ((digest == nullptr) || (EVP_MD_type(digest) != NID_md5_sha1)) {
        return TLS1_2_VERSION;
    }

    return SSL3_VERSION;
}

int SSL_set_ocsp_response(SSL *ssl, const uint8_t *response, size_t response_len) {
// OpenSSL takes ownership of the response buffer so we have to take a copy
   void *copy = OPENSSL_memdup(response, response_len);
   if ((copy == NULL) && response) {
     return 0;
   }
   return SSL_set_tlsext_status_ocsp_resp(ssl, copy, response_len);
}

inline bool ssl_client_hello_get_extension(const SSL_CLIENT_HELLO *client_hello,
                                           CBS *out, uint16_t extension_type) {
    CBS extensions;
    CBS_init(&extensions, client_hello->extensions.curr, client_hello->extensions.remaining);
    while (CBS_len(&extensions) != 0) {
        // Decode the next extension.
        uint16_t type;
        CBS extension;
        if (!CBS_get_u16(&extensions, &type) ||
            !CBS_get_u16_length_prefixed(&extensions, &extension)) {
            return false;
        }

        if (type == extension_type) {
            *out = extension;
            return true;
        }
    }

    return false;
}

int SSL_early_callback_ctx_extension_get(const SSL_CLIENT_HELLO *client_hello,
                                         uint16_t extension_type,
                                         const uint8_t **out_data,
                                         size_t *out_len) {
    CBS cbs;
    if (!ssl_client_hello_get_extension(client_hello, &cbs, extension_type)) {
        return 0;
    }

    *out_data = CBS_data(&cbs);
    *out_len = CBS_len(&cbs);
    return 1;
}

bssl::UniquePtr<SSL> newSsl(SSL_CTX* ctx) { return bssl::UniquePtr<SSL>(SSL_new(ctx)); }

int set_strict_cipher_list(SSL_CTX* ctx, const char* str) {
  SSL_CTX_set_cipher_list(ctx, str);

  STACK_OF(SSL_CIPHER)* ciphers = SSL_CTX_get_ciphers(ctx);
  char* dup = strdup(str);
  char* token = std::strtok(dup, ":+![|]");
  while (token != NULL) {
    std::string str1(token);
    bool found = false;
    for (int i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
      const SSL_CIPHER* cipher = sk_SSL_CIPHER_value(ciphers, i);
      std::string str2(SSL_CIPHER_get_name(cipher));
      if (str1.compare(str2) == 0) {
        found = true;
      }
    }

    if (!found && str1.compare("-ALL") && str1.compare("ALL")) {
      free(dup);
      return 0;
    }

    token = std::strtok(NULL, ":[]|");
  }

  free(dup);
  return 1;
}

STACK_OF(X509)* SSL_get_peer_full_cert_chain(const SSL* ssl) {
  STACK_OF(X509)* to_copy = SSL_get_peer_cert_chain(ssl);
  // sk_X509_dup does not increase reference counts on certs in the stack.
  STACK_OF(X509)* ret = to_copy == nullptr ? nullptr : sk_X509_dup(to_copy);
  if (ret != nullptr && SSL_is_server(ssl)) {
    X509* peer_cert = SSL_get_peer_certificate(ssl);
    if (peer_cert == nullptr) {
      return ret;
    }
    if (!sk_X509_insert(ret, peer_cert, 0)) {
      sk_X509_pop_free(ret, X509_free);
      return nullptr;
    }
  }

  return ret;
}

void allowRenegotiation(SSL*) {
  // Refer to https://www.openssl.org/docs/man1.1.1/man3/SSL_get_secure_renegotiation_support.html
  // "OpenSSL always attempts to use secure renegotiation as described in RFC5746.
  // This counters the prefix attack described in CVE-2009-3555 and elsewhere."
  // SSL_set_renegotiate_mode(ssl, mode);
}

SSL_SESSION* ssl_session_from_bytes(SSL* client_ssl_socket, const SSL_CTX*,
                                    const std::string& client_session) {
  SSL_SESSION* client_ssl_session = SSL_get_session(client_ssl_socket);
  SSL_SESSION_set_app_data(client_ssl_session, client_session.data());
  return client_ssl_session;
}

int ssl_session_to_bytes(const SSL_SESSION*, uint8_t** out_data, size_t* out_len) {
  //   void *data = SSL_SESSION_get_app_data(in);
  //   *out_data = data;
  *out_data = static_cast<uint8_t*>(OPENSSL_malloc(1));
  *out_len = 1;

  return 1;
}

int should_be_single_use(const SSL_SESSION*) { return 1; }

const SSL_CIPHER *SSL_get_cipher_by_value(uint16_t value) {
    const SSL_CIPHER *result = NULL;
    SSL_CTX *ctx = SSL_CTX_new(TLS_method());
    if(ctx) {
        SSL *ssl = SSL_new(ctx);
        if(ssl) {
            uint16_t nvalue = htons(value);
            result = SSL_CIPHER_find(ssl, (const unsigned char*)&nvalue);
            SSL_free(ssl);
        }
        SSL_CTX_free(ctx);
    }

    return result;
}

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
