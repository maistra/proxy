#include "extensions/transport_sockets/tls/openssl_impl.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "openssl/crypto.h"
#include "openssl/hmac.h"
#include "openssl/rand.h"
#include "openssl/ssl.h"
#include "openssl/x509v3.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

int alpnSelectCallback(std::vector<uint8_t> parsed_alpn_protocols, const unsigned char** out,
                       unsigned char* outlen, const unsigned char* in, unsigned int inlen) {
  // Currently this uses the standard selection algorithm in priority order.
  const uint8_t* alpn_data = &parsed_alpn_protocols[0];
  size_t alpn_data_size = parsed_alpn_protocols.size();

  if (SSL_select_next_proto(const_cast<unsigned char**>(out), outlen, alpn_data, alpn_data_size, in,
                            inlen) != OPENSSL_NPN_NEGOTIATED) {
    return SSL_TLSEXT_ERR_NOACK;
  } else {
    return SSL_TLSEXT_ERR_OK;
  }
}

auto select_client_cert_cb = +[](SSL* ssl, X509** x509, EVP_PKEY** pkey) -> int {
  std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!! select_client_cert_cb \n";
  return 0;
};

void set_select_certificate_cb(SSL_CTX* ctx) {
  SSL_CTX_set_client_cert_cb(ctx, select_client_cert_cb);
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

STACK_OF(X509)* SSL_get_peer_full_cert_chain(const SSL *ssl) {
  STACK_OF(X509)* to_copy = SSL_get_peer_cert_chain(ssl);
  if (!to_copy) {
    return nullptr;
  }
  STACK_OF(X509)* ret = sk_X509_dup(SSL_get_peer_cert_chain(ssl));

  if (SSL_is_server(ssl)) {
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

void allowRenegotiation(SSL* ssl) {
  // SSL_set_renegotiate_mode(ssl, mode);
}

SSL_SESSION* ssl_session_from_bytes(SSL* client_ssl_socket, const SSL_CTX* client_ssl_context,
                                    const std::string& client_session) {
  SSL_SESSION* client_ssl_session = SSL_get_session(client_ssl_socket);
  SSL_SESSION_set_app_data(client_ssl_session, client_session.data());
  return client_ssl_session;
}

int ssl_session_to_bytes(const SSL_SESSION* in, uint8_t** out_data, size_t* out_len) {
  //   void *data = SSL_SESSION_get_app_data(in);
  //   *out_data = data;
  *out_data = static_cast<uint8_t*>(OPENSSL_malloc(1));
  *out_len = 1;

  return 1;
}

void ssl_ctx_add_client_CA(SSL_CTX* ctx, X509* x) { SSL_CTX_add_client_CA(ctx, x); }

int should_be_single_use(const SSL_SESSION* session) { return 1; }

// void ssl_ctx_set_client_CA_list(SSL_CTX *ctx, bssl::UniquePtr<STACK_OF(X509_NAME)> list) {
//	if (sk_X509_NAME_num(list.get()) > 0)
//	  SSL_CTX_set_client_CA_list(ctx, list.release());
//}

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
