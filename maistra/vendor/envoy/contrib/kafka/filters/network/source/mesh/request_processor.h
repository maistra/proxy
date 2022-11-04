#pragma once

#include "source/common/common/logger.h"

#include "contrib/kafka/filters/network/source/external/requests.h"
#include "contrib/kafka/filters/network/source/mesh/abstract_command.h"
#include "contrib/kafka/filters/network/source/mesh/upstream_config.h"
#include "contrib/kafka/filters/network/source/mesh/upstream_kafka_facade.h"
#include "contrib/kafka/filters/network/source/request_codec.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace Kafka {
namespace Mesh {

/**
 * Processes (enriches) incoming requests and passes it back to origin.
 */
class RequestProcessor : public RequestCallback, private Logger::Loggable<Logger::Id::kafka> {
public:
  RequestProcessor(AbstractRequestListener& origin, const UpstreamKafkaConfiguration& configuration,
                   UpstreamKafkaFacade& upstream_kafka_facade);

  // RequestCallback
  void onMessage(AbstractRequestSharedPtr arg) override;
  void onFailedParse(RequestParseFailureSharedPtr) override;

private:
  void process(const std::shared_ptr<Request<ProduceRequest>> request) const;
  void process(const std::shared_ptr<Request<MetadataRequest>> request) const;
  void process(const std::shared_ptr<Request<ApiVersionsRequest>> request) const;

  AbstractRequestListener& origin_;
  const UpstreamKafkaConfiguration& configuration_;
  UpstreamKafkaFacade& upstream_kafka_facade_;
};

} // namespace Mesh
} // namespace Kafka
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
