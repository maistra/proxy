#pragma once

#include <memory>
#include <string>
#include <vector>

#include "envoy/common/pure.h"

namespace Envoy {
namespace Ssl {

enum class ClientValidationStatus { NotValidated, NoClientCertificate, Validated, Failed };

enum class ValidateStatus {
  NotStarted,
  Pending,
  Successful,
  Failed,
};

/**
 * Used to return the result from an asynchronous cert validation.
 */
class ValidateResultCallback {
public:
  virtual ~ValidateResultCallback() = default;

  virtual Event::Dispatcher& dispatcher() PURE;

  /**
   * Called when the asynchronous cert validation completes.
   * @param succeeded true if the validation succeeds
   * @param detailed_status detailed status of the underlying validation. Depending on the
   *        validation configuration, `succeeded` may be true but `detailed_status` might
   *        indicate a failure. This detailed status can be used to inform routing
   *        decisions.
   * @param error_details failure details, only used if the validation fails.
   * @param tls_alert the TLS error related to the failure, only used if the validation fails.
   */
  virtual void onCertValidationResult(bool succeeded, ClientValidationStatus detailed_status,
                                      const std::string& error_details, uint8_t tls_alert) PURE;
};

using ValidateResultCallbackPtr = std::unique_ptr<ValidateResultCallback>;

class SslExtendedSocketInfo {
public:
  virtual ~SslExtendedSocketInfo() = default;

  /**
   * Set the peer certificate validation status.
   **/
  virtual void setCertificateValidationStatus(ClientValidationStatus validated) PURE;

  /**
   * @return ClientValidationStatus The peer certificate validation status.
   **/
  virtual ClientValidationStatus certificateValidationStatus() const PURE;
};

} // namespace Ssl
} // namespace Envoy
