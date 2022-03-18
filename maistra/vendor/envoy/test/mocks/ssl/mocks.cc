#include "mocks.h"

namespace Envoy {
namespace Ssl {

MockContextManager::MockContextManager() = default;
MockContextManager::~MockContextManager() = default;

MockConnectionInfo::MockConnectionInfo() = default;
MockConnectionInfo::~MockConnectionInfo() = default;

MockClientContext::MockClientContext() = default;
MockClientContext::~MockClientContext() = default;

MockClientContextConfig::MockClientContextConfig() {
  capabilities_.provides_ciphers_and_curves = true;
  ON_CALL(*this, serverNameIndication()).WillByDefault(testing::ReturnRef(sni_));
  ON_CALL(*this, cipherSuites()).WillByDefault(testing::ReturnRef(ciphers_));
  ON_CALL(*this, capabilities()).WillByDefault(testing::Return(capabilities_));
  ON_CALL(*this, alpnProtocols()).WillByDefault(testing::ReturnRef(alpn_));
  ON_CALL(*this, signingAlgorithmsForTest()).WillByDefault(testing::ReturnRef(test_));
}
MockClientContextConfig::~MockClientContextConfig() = default;

MockServerContextConfig::MockServerContextConfig() = default;
MockServerContextConfig::~MockServerContextConfig() = default;

MockPrivateKeyMethodManager::MockPrivateKeyMethodManager() = default;
MockPrivateKeyMethodManager::~MockPrivateKeyMethodManager() = default;

MockPrivateKeyMethodProvider::MockPrivateKeyMethodProvider() = default;
MockPrivateKeyMethodProvider::~MockPrivateKeyMethodProvider() = default;

} // namespace Ssl
} // namespace Envoy
