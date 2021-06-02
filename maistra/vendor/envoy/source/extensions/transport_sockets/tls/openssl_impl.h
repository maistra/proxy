#pragma once

#include <functional>
#include <string>
#include <vector>

#include "bssl_wrapper/bssl_wrapper.h"
#include "openssl/ssl.h"

/*
 * MAISTRA
 * Contains the functions where BoringSSL and OpenSSL diverge. In most cases this means that there are functions in BoringSSL that do not exist
 * in OpenSSL
 */
namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

int alpnSelectCallback(std::vector<uint8_t> parsed_alpn_protocols, const unsigned char** out,
                       unsigned char* outlen, const unsigned char* in, unsigned int inlen);

void set_select_certificate_cb(SSL_CTX* ctx);

// bssl::UniquePtr<SSL> newSsl(SSL_CTX *ctx);

int set_strict_cipher_list(SSL_CTX* ctx, const char* str);

STACK_OF(X509)* SSL_get_peer_full_cert_chain(const SSL *ssl);

void allowRenegotiation(SSL* ssl);

SSL_SESSION* ssl_session_from_bytes(SSL* client_ssl_socket, const SSL_CTX* client_ssl_context,
                                    const std::string& client_session);

int ssl_session_to_bytes(const SSL_SESSION* in, uint8_t** out_data, size_t* out_len);

void ssl_ctx_add_client_CA(SSL_CTX* ctx, X509* x);

int should_be_single_use(const SSL_SESSION* session);

// void ssl_ctx_set_client_CA_list(SSL_CTX *ctx, bssl::UniquePtr<STACK_OF(X509_NAME)> list);

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
