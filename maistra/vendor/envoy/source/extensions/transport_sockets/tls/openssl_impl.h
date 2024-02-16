#pragma once

#include <functional>
#include <string>
#include <vector>

#include "bssl_wrapper/bssl_wrapper.h"
#include "openssl/ssl.h"
#include "ssl/ssl_local.h"
#include "absl/strings/string_view.h"

// The client hello in OpenSSL is defined in ssl/ssl_locl.h
// BoringSSL has a related definition as SSL_CLIENT_HELLO
// The OpenSSL struct is not an exact match, use
// in conjunction with the SSL struct instance.
#define SSL_CLIENT_HELLO  CLIENTHELLO_MSG
#define BORINGSSL_ENUM_INT : int

// See BoringSSL ssl.h and OpenSSL ssl.h
enum ssl_select_cert_result_t BORINGSSL_ENUM_INT {
    ssl_select_cert_success = SSL_CLIENT_HELLO_SUCCESS,
    ssl_select_cert_error   = SSL_CLIENT_HELLO_ERROR,
    ssl_select_cert_retry   = SSL_CLIENT_HELLO_RETRY
};

/*
 * MAISTRA
 * Contains the functions where BoringSSL and OpenSSL diverge. In most cases this means that there
 * are functions in BoringSSL that do not exist in OpenSSL
 */



namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

absl::string_view SSL_extract_client_hello_sni_host_name(const SSL* ssl);

int SSL_CTX_set_strict_cipher_list(SSL_CTX *ctx, const char *str);

int SSL_set_ocsp_response(SSL *ssl, const uint8_t *response, size_t response_len);
int SSL_early_callback_ctx_extension_get(const SSL_CLIENT_HELLO *client_hello,
                                         uint16_t extension_type,
                                         const uint8_t **out_data,
                                         size_t *out_len);

int set_strict_cipher_list(SSL_CTX* ctx, const char* str);

uint16_t SSL_CIPHER_get_min_version(const SSL_CIPHER *cipher);
int SSL_set_ocsp_response(SSL *ssl, const uint8_t *response, size_t response_len);

// SSL_get_peer_full_cert_chain exists in the BoringSSL library. This diverges in that
// the caller will own the returned certificate chain and needs to call sk_X509_pop_free on any
// non-null value this returns.
// Also, note that this call does not increase reference counts of any certs in the chain, and
// therefore the result should not be persisted but rather used and discarded directly in the
// consuming call.
STACK_OF(X509)* SSL_get_peer_full_cert_chain(const SSL* ssl);

void allowRenegotiation(SSL* ssl);

SSL_SESSION* ssl_session_from_bytes(SSL* client_ssl_socket, const SSL_CTX* client_ssl_context,
                                    const std::string& client_session);

int ssl_session_to_bytes(const SSL_SESSION* in, uint8_t** out_data, size_t* out_len);

int should_be_single_use(const SSL_SESSION* session);

const SSL_CIPHER *SSL_get_cipher_by_value(uint16_t value);

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
