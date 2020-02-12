#pragma once

#include <functional>
#include <string>
#include <vector>

#include "openssl/ssl.h"

namespace Envoy {
namespace Tcp {
namespace SniVerifier {

int getServernameCallbackReturn(int* out_alert);

}  // namespace SniVerifier
}  // namespace Tcp
}  // namespace Envoy
