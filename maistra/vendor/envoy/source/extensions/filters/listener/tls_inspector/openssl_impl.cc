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
namespace Extensions {
namespace ListenerFilters {
namespace TlsInspector {

std::vector<absl::string_view> getAlpnProtocols(const unsigned char* data, unsigned int len) {
  std::vector<absl::string_view> protocols;
  absl::string_view str(reinterpret_cast<const char*>(data));
  for (int i = 0; i < len;) {
    uint32_t protocol_length = 0;
    protocol_length <<= 8;
    protocol_length |= data[i];
    ++i;
    absl::string_view protocol(str.substr(i, protocol_length));
    protocols.push_back(protocol);
    i += protocol_length;
  }

  return protocols;
}

} // namespace TlsInspector
} // namespace ListenerFilters
} // namespace Extensions
} // namespace Envoy
