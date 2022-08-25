#pragma once
#include <atomic>
#include <cstdint>
#include <list>
#include <memory>

#include "envoy/common/time.h"
#include "envoy/event/deferred_deletable.h"
#include "envoy/event/dispatcher.h"
#include "envoy/network/connection.h"
#include "envoy/network/connection_handler.h"
#include "envoy/network/filter.h"
#include "envoy/network/listen_socket.h"
#include "envoy/network/listener.h"
#include "envoy/server/listener_manager.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/timespan.h"

#include "source/common/common/linked_object.h"
#include "source/common/common/non_copyable.h"
#include "source/common/stream_info/stream_info_impl.h"
#include "source/server/active_stream_listener_base.h"

#include "spdlog/spdlog.h"

namespace Envoy {
namespace Server {

class ActiveInternalListener : public OwnedActiveStreamListenerBase,
                               public Network::InternalListener {
public:
  ActiveInternalListener(Network::ConnectionHandler& conn_handler, Event::Dispatcher& dispatcher,
                         Network::ListenerConfig& config);
  ActiveInternalListener(Network::ConnectionHandler& conn_handler, Event::Dispatcher& dispatcher,
                         Network::ListenerPtr listener, Network::ListenerConfig& config);
  ~ActiveInternalListener() override;

  class NetworkInternalListener : public Network::Listener {

    void disable() override {
      // Similar to the listeners that does not bind to port. Accept is not driven by OS io event so
      // the disable is not working.
      // TODO(lambdai): Explore the approach to elegantly disable internal listener. Maybe an user
      // space accept queue should be put here.
      ENVOY_LOG(debug, "Warning: the internal listener cannot be disabled.");
    }

    void enable() override {
      ENVOY_LOG(debug, "Warning: the internal listener is always enabled.");
    }

    void setRejectFraction(UnitFloat) override {}
  };

  // ActiveListenerImplBase
  Network::Listener* listener() override { return listener_.get(); }

  // Network::TcpConnectionHandler
  Network::BalancedConnectionHandlerOptRef
  getBalancedHandlerByAddress(const Network::Address::Instance&) override {
    // Internal listener doesn't support migrate connection to another worker.
    // TODO(lambdai): implement the function of handling off to another listener of the same worker.
    PANIC("not implemented");
  }

  void pauseListening() override {
    if (listener_ != nullptr) {
      listener_->disable();
    }
  }
  void resumeListening() override {
    if (listener_ != nullptr) {
      listener_->enable();
    }
  }
  void shutdownListener() override { listener_.reset(); }

  // Network::InternalListener
  void onAccept(Network::ConnectionSocketPtr&& socket) override;

  // Network::BalancedConnectionHandler
  void incNumConnections() override { config_->openConnections().inc(); }
  void decNumConnections() override { config_->openConnections().dec(); }

  void newActiveConnection(const Network::FilterChain& filter_chain,
                           Network::ServerConnectionPtr server_conn_ptr,
                           std::unique_ptr<StreamInfo::StreamInfo> stream_info) override;
  /**
   * Update the listener config. The follow up connections will see the new config. The existing
   * connections are not impacted.
   */
  void updateListenerConfig(Network::ListenerConfig& config) override;
};

} // namespace Server
} // namespace Envoy
