#include <arpa/inet.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "openssl/hmac.h"
#include "openssl/rand.h"
#include "openssl/ssl.h"
#include "openssl/x509v3.h"

namespace Envoy {
namespace Tcp {
namespace SniVerifier {

int getServernameCallbackReturn(int* out_alert) { return 1; }; //SSL_TLSEXT_ERR_OK; }

}  // namespace SniVerifier
}  // namespace Tcp
}  // namespace Envoy
