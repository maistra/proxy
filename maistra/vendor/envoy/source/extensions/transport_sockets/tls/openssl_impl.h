#pragma once

#include <functional>
#include <string>
#include <vector>

#include "bssl_wrapper/bssl_wrapper.h"
#include "openssl/ssl.h"

/*
 * MAISTRA
 * Contains the functions where BoringSSL and OpenSSL diverge. In most cases this means that there
 * are functions in BoringSSL that do not exist in OpenSSL
 */
namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {

int set_strict_cipher_list(SSL_CTX* ctx, const char* str);

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

} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
