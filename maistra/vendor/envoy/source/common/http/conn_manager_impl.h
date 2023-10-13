#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "envoy/access_log/access_log.h"
#include "envoy/common/optref.h"
#include "envoy/common/random_generator.h"
#include "envoy/common/scope_tracker.h"
#include "envoy/common/time.h"
#include "envoy/event/deferred_deletable.h"
#include "envoy/http/api_listener.h"
#include "envoy/http/codec.h"
#include "envoy/http/codes.h"
#include "envoy/http/context.h"
#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"
#include "envoy/network/connection.h"
#include "envoy/network/drain_decision.h"
#include "envoy/network/filter.h"
#include "envoy/router/rds.h"
#include "envoy/router/scopes.h"
#include "envoy/runtime/runtime.h"
#include "envoy/server/overload/overload_manager.h"
#include "envoy/ssl/connection.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "envoy/stream_info/filter_state.h"
#include "envoy/tracing/http_tracer.h"
#include "envoy/upstream/upstream.h"

#include "source/common/buffer/watermark_buffer.h"
#include "source/common/common/dump_state_utils.h"
#include "source/common/common/linked_object.h"
#include "source/common/grpc/common.h"
#include "source/common/http/conn_manager_config.h"
#include "source/common/http/filter_manager.h"
#include "source/common/http/user_agent.h"
#include "source/common/http/utility.h"
#include "source/common/local_reply/local_reply.h"
#include "source/common/network/proxy_protocol_filter_state.h"
#include "source/common/router/scoped_rds.h"
#include "source/common/stream_info/stream_info_impl.h"
#include "source/common/tracing/http_tracer_impl.h"

namespace Envoy {
namespace Http {

/**
 * Implementation of both ConnectionManager and ServerConnectionCallbacks. This is a
 * Network::Filter that can be installed on a connection that will perform HTTP protocol agnostic
 * handling of a connection and all requests/pushes that occur on a connection.
 */
class ConnectionManagerImpl : Logger::Loggable<Logger::Id::http>,
                              public Network::ReadFilter,
                              public ServerConnectionCallbacks,
                              public Network::ConnectionCallbacks,
                              public Http::ApiListener {
public:
  ConnectionManagerImpl(ConnectionManagerConfig& config, const Network::DrainDecision& drain_close,
                        Random::RandomGenerator& random_generator, Http::Context& http_context,
                        Runtime::Loader& runtime, const LocalInfo::LocalInfo& local_info,
                        Upstream::ClusterManager& cluster_manager,
                        Server::OverloadManager& overload_manager, TimeSource& time_system);
  ~ConnectionManagerImpl() override;

  static ConnectionManagerStats generateStats(const std::string& prefix, Stats::Scope& scope);
  static ConnectionManagerTracingStats generateTracingStats(const std::string& prefix,
                                                            Stats::Scope& scope);
  static void chargeTracingStats(const Tracing::Reason& tracing_reason,
                                 ConnectionManagerTracingStats& tracing_stats);
  static ConnectionManagerListenerStats generateListenerStats(const std::string& prefix,
                                                              Stats::Scope& scope);
  static const ResponseHeaderMap& continueHeader();

  // Currently the ConnectionManager creates a codec lazily when either:
  //   a) onConnection for H3.
  //   b) onData for H1 and H2.
  // With the introduction of ApiListeners, neither event occurs. This function allows consumer code
  // to manually create a codec.
  // TODO(junr03): consider passing a synthetic codec instead of creating once. The codec in the
  // ApiListener case is solely used to determine the protocol version.
  void createCodec(Buffer::Instance& data);

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance& data, bool end_stream) override;
  Network::FilterStatus onNewConnection() override;
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override;

  // Http::ConnectionCallbacks
  void onGoAway(GoAwayErrorCode error_code) override;

  // Http::ServerConnectionCallbacks
  RequestDecoder& newStream(ResponseEncoder& response_encoder,
                            bool is_internally_created = false) override;

  // Network::ConnectionCallbacks
  void onEvent(Network::ConnectionEvent event) override;
  // Pass connection watermark events on to all the streams associated with that connection.
  void onAboveWriteBufferHighWatermark() override {
    codec_->onUnderlyingConnectionAboveWriteBufferHighWatermark();
  }
  void onBelowWriteBufferLowWatermark() override {
    codec_->onUnderlyingConnectionBelowWriteBufferLowWatermark();
  }

  TimeSource& timeSource() { return time_source_; }

  void setClearHopByHopResponseHeaders(bool value) { clear_hop_by_hop_response_headers_ = value; }
  bool clearHopByHopResponseHeaders() const { return clear_hop_by_hop_response_headers_; }

  // This runtime key configures the number of streams which must be closed on a connection before
  // envoy will potentially drain a connection due to excessive prematurely reset streams.
  static const absl::string_view PrematureResetTotalStreamCountKey;

  // The minimum lifetime of a stream, in seconds, in order not to be considered
  // prematurely closed.
  static const absl::string_view PrematureResetMinStreamLifetimeSecondsKey;
  static const absl::string_view MaxRequestsPerIoCycle;

private:
  struct ActiveStream;
  class MobileConnectionManagerImpl;

  class RdsRouteConfigUpdateRequester {
  public:
    RdsRouteConfigUpdateRequester(Router::RouteConfigProvider* route_config_provider,
                                  ActiveStream& parent)
        : route_config_provider_(route_config_provider), parent_(parent) {}

    RdsRouteConfigUpdateRequester(Config::ConfigProvider* scoped_route_config_provider,
                                  ActiveStream& parent)
        // Expect the dynamic cast to succeed because only ScopedRdsConfigProvider is fully
        // implemented. Inline provider will be cast to nullptr here but it is not full implemented
        // and can't not be used at this point. Should change this implementation if we have a
        // functional inline scope route provider in the future.
        : scoped_route_config_provider_(
              dynamic_cast<Router::ScopedRdsConfigProvider*>(scoped_route_config_provider)),
          parent_(parent) {}

    void
    requestRouteConfigUpdate(Http::RouteConfigUpdatedCallbackSharedPtr route_config_updated_cb);
    void requestVhdsUpdate(const std::string& host_header,
                           Event::Dispatcher& thread_local_dispatcher,
                           Http::RouteConfigUpdatedCallbackSharedPtr route_config_updated_cb);
    void requestSrdsUpdate(Router::ScopeKeyPtr scope_key,
                           Event::Dispatcher& thread_local_dispatcher,
                           Http::RouteConfigUpdatedCallbackSharedPtr route_config_updated_cb);

  private:
    Router::RouteConfigProvider* route_config_provider_;
    Router::ScopedRdsConfigProvider* scoped_route_config_provider_;
    ActiveStream& parent_;
  };

  /**
   * Wraps a single active stream on the connection. These are either full request/response pairs
   * or pushes.
   */
  struct ActiveStream final : LinkedObject<ActiveStream>,
                              public Event::DeferredDeletable,
                              public StreamCallbacks,
                              public RequestDecoder,
                              public Tracing::Config,
                              public ScopeTrackedObject,
                              public FilterManagerCallbacks,
                              public DownstreamStreamFilterCallbacks {
    ActiveStream(ConnectionManagerImpl& connection_manager, uint32_t buffer_limit,
                 Buffer::BufferMemoryAccountSharedPtr account);
    void completeRequest();

    const Network::Connection* connection();
    uint64_t streamId() { return stream_id_; }

    // This is a helper function for encodeHeaders and responseDataTooLarge which allows for
    // shared code for the two headers encoding paths. It does header munging, updates timing
    // stats, and sends the headers to the encoder.
    void encodeHeadersInternal(ResponseHeaderMap& headers, bool end_stream);
    // This is a helper function for encodeData and responseDataTooLarge which allows for shared
    // code for the two data encoding paths. It does stats updates and tracks potential end of
    // stream.
    void encodeDataInternal(Buffer::Instance& data, bool end_stream);

    // Http::StreamCallbacks
    void onResetStream(StreamResetReason reason,
                       absl::string_view transport_failure_reason) override;
    void onAboveWriteBufferHighWatermark() override;
    void onBelowWriteBufferLowWatermark() override;

    // Http::StreamDecoder
    void decodeData(Buffer::Instance& data, bool end_stream) override;
    void decodeMetadata(MetadataMapPtr&&) override;

    // Mark that the last downstream byte is received, and the downstream stream is complete.
    void maybeEndDecode(bool end_stream);

    // Http::RequestDecoder
    void decodeHeaders(RequestHeaderMapPtr&& headers, bool end_stream) override;
    void decodeTrailers(RequestTrailerMapPtr&& trailers) override;
    StreamInfo::StreamInfo& streamInfo() override { return filter_manager_.streamInfo(); }
    void sendLocalReply(Code code, absl::string_view body,
                        const std::function<void(ResponseHeaderMap& headers)>& modify_headers,
                        const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                        absl::string_view details) override {
      return filter_manager_.sendLocalReply(code, body, modify_headers, grpc_status, details);
    }

    // Tracing::TracingConfig
    Tracing::OperationName operationName() const override;
    const Tracing::CustomTagMap* customTags() const override;
    bool verbose() const override;
    uint32_t maxPathTagLength() const override;

    // ScopeTrackedObject
    void dumpState(std::ostream& os, int indent_level = 0) const override {
      const char* spaces = spacesForLevel(indent_level);
      os << spaces << "ActiveStream " << this << DUMP_MEMBER(stream_id_);

      DUMP_DETAILS(&filter_manager_);
    }

    // FilterManagerCallbacks
    void encodeHeaders(ResponseHeaderMap& response_headers, bool end_stream) override;
    void encode1xxHeaders(ResponseHeaderMap& response_headers) override;
    void encodeData(Buffer::Instance& data, bool end_stream) override;
    void encodeTrailers(ResponseTrailerMap& trailers) override;
    void encodeMetadata(MetadataMapPtr&& metadata) override;
    void setRequestTrailers(Http::RequestTrailerMapPtr&& request_trailers) override {
      ASSERT(!request_trailers_);
      request_trailers_ = std::move(request_trailers);
    }
    void setInformationalHeaders(Http::ResponseHeaderMapPtr&& informational_headers) override {
      ASSERT(!informational_headers_);
      informational_headers_ = std::move(informational_headers);
    }
    void setResponseHeaders(Http::ResponseHeaderMapPtr&& response_headers) override {
      // We'll overwrite the headers in the case where we fail the stream after upstream headers
      // have begun filter processing but before they have been sent downstream.
      response_headers_ = std::move(response_headers);
    }
    void setResponseTrailers(Http::ResponseTrailerMapPtr&& response_trailers) override {
      response_trailers_ = std::move(response_trailers);
    }
    void chargeStats(const ResponseHeaderMap& headers) override;

    Http::RequestHeaderMapOptRef requestHeaders() override {
      return makeOptRefFromPtr(request_headers_.get());
    }
    Http::RequestTrailerMapOptRef requestTrailers() override {
      return makeOptRefFromPtr(request_trailers_.get());
    }
    Http::ResponseHeaderMapOptRef informationalHeaders() override {
      return makeOptRefFromPtr(informational_headers_.get());
    }
    Http::ResponseHeaderMapOptRef responseHeaders() override {
      return makeOptRefFromPtr(response_headers_.get());
    }
    Http::ResponseTrailerMapOptRef responseTrailers() override {
      return makeOptRefFromPtr(response_trailers_.get());
    }

    void endStream() override {
      ASSERT(!state_.codec_saw_local_complete_);
      state_.codec_saw_local_complete_ = true;
      filter_manager_.streamInfo().downstreamTiming().onLastDownstreamTxByteSent(
          connection_manager_.time_source_);
      request_response_timespan_->complete();
      connection_manager_.doEndStream(*this);
    }
    void onDecoderFilterBelowWriteBufferLowWatermark() override;
    void onDecoderFilterAboveWriteBufferHighWatermark() override;
    void upgradeFilterChainCreated() override {
      connection_manager_.stats_.named_.downstream_cx_upgrades_total_.inc();
      connection_manager_.stats_.named_.downstream_cx_upgrades_active_.inc();
      state_.successful_upgrade_ = true;
    }
    void disarmRequestTimeout() override;
    void resetIdleTimer() override;
    void recreateStream(StreamInfo::FilterStateSharedPtr filter_state) override;
    void resetStream(Http::StreamResetReason reset_reason = Http::StreamResetReason::LocalReset,
                     absl::string_view transport_failure_reason = "") override;
    const Router::RouteEntry::UpgradeMap* upgradeMap() override;
    Upstream::ClusterInfoConstSharedPtr clusterInfo() override;
    Tracing::Span& activeSpan() override;
    void onResponseDataTooLarge() override;
    void onRequestDataTooLarge() override;
    Http1StreamEncoderOptionsOptRef http1StreamEncoderOptions() override;
    void onLocalReply(Code code) override;
    // TODO(alyssawilk) this should be an optional reference.
    Tracing::Config& tracingConfig() override;
    const ScopeTrackedObject& scope() override;
    OptRef<DownstreamStreamFilterCallbacks> downstreamCallbacks() override { return *this; }

    // DownstreamStreamFilterCallbacks
    void setRoute(Router::RouteConstSharedPtr route) override;
    Router::RouteConstSharedPtr route(const Router::RouteCallback& cb) override;
    void clearRouteCache() override;
    void requestRouteConfigUpdate(
        Http::RouteConfigUpdatedCallbackSharedPtr route_config_updated_cb) override;

    absl::optional<Router::ConfigConstSharedPtr> routeConfig();
    void traceRequest();

    // Updates the snapped_route_config_ (by reselecting scoped route configuration), if a scope is
    // not found, snapped_route_config_ is set to Router::NullConfigImpl.
    void snapScopedRouteConfig();

    void refreshCachedRoute();
    void refreshCachedRoute(const Router::RouteCallback& cb);

    void refreshCachedTracingCustomTags();
    void refreshDurationTimeout();
    void refreshIdleTimeout();

    // All state for the stream. Put here for readability.
    struct State {
      State()
          : codec_saw_local_complete_(false), saw_connection_close_(false),
            successful_upgrade_(false), is_internally_created_(false), is_tunneling_(false),
            decorated_propagate_(true), deferred_to_next_io_iteration_(false) {}

      bool codec_saw_local_complete_ : 1; // This indicates that local is complete as written all
                                          // the way through to the codec.
      bool saw_connection_close_ : 1;
      bool successful_upgrade_ : 1;

      // True if this stream is internally created. Currently only used for
      // internal redirects or other streams created via recreateStream().
      bool is_internally_created_ : 1;

      // True if the response headers indicate a successful upgrade or connect
      // response.
      bool is_tunneling_ : 1;

      bool decorated_propagate_ : 1;

      // Indicates that sending headers to the filter manager is deferred to the
      // next I/O cycle. If data or trailers are received when this flag is set
      // they are deferred too.
      // TODO(yanavlasov): encapsulate the entire state of deferred streams into a separate
      // structure, so it can be atomically created and cleared.
      bool deferred_to_next_io_iteration_ : 1;
      bool deferred_end_stream_ : 1;
    };

    // Per-stream idle timeout callback.
    void onIdleTimeout();
    // Per-stream request timeout callback.
    void onRequestTimeout();
    // Per-stream request header timeout callback.
    void onRequestHeaderTimeout();
    // Per-stream alive duration reached.
    void onStreamMaxDurationReached();
    bool hasCachedRoute() { return cached_route_.has_value() && cached_route_.value(); }

    // Return local port of the connection.
    uint32_t localPort();

    friend std::ostream& operator<<(std::ostream& os, const ActiveStream& s) {
      s.dumpState(os);
      return os;
    }

    Tracing::CustomTagMap& getOrMakeTracingCustomTagMap() {
      if (tracing_custom_tags_ == nullptr) {
        tracing_custom_tags_ = std::make_unique<Tracing::CustomTagMap>();
      }
      return *tracing_custom_tags_;
    }

    // Note: this method is a noop unless ENVOY_ENABLE_UHV is defined
    // Call header validator extension to validate request header map after it was deserialized.
    // If header map failed validation, it sends an error response and returns false.
    bool validateHeaders();

    // Dispatch deferred headers, body and trailers to the filter manager.
    // Return true if this stream was deferred and dispatched pending headers, body and trailers (if
    // present). Return false if this stream was not deferred.
    bool onDeferredRequestProcessing();

    ConnectionManagerImpl& connection_manager_;
    // TODO(snowp): It might make sense to move this to the FilterManager to avoid storing it in
    // both locations, then refer to the FM when doing stream logs.
    const uint64_t stream_id_;

    RequestHeaderMapPtr request_headers_;
    RequestTrailerMapPtr request_trailers_;

    ResponseHeaderMapPtr informational_headers_;
    ResponseHeaderMapPtr response_headers_;
    ResponseTrailerMapPtr response_trailers_;

    // Note: The FM must outlive the above headers, as they are possibly accessed during filter
    // destruction.
    DownstreamFilterManager filter_manager_;

    Router::ConfigConstSharedPtr snapped_route_config_;
    Router::ScopedConfigConstSharedPtr snapped_scoped_routes_config_;
    Tracing::SpanPtr active_span_;
    ResponseEncoder* response_encoder_{};
    Stats::TimespanPtr request_response_timespan_;
    // Per-stream idle timeout. This timer gets reset whenever activity occurs on the stream, and,
    // when triggered, will close the stream.
    Event::TimerPtr stream_idle_timer_;
    // Per-stream request timeout. This timer is enabled when the stream is created and disabled
    // when the stream ends. If triggered, it will close the stream.
    Event::TimerPtr request_timer_;
    // Per-stream request header timeout. This timer is enabled when the stream is created and
    // disabled when the downstream finishes sending headers. If triggered, it will close the
    // stream.
    Event::TimerPtr request_header_timer_;
    // Per-stream alive duration. This timer is enabled once when the stream is created and, if
    // triggered, will close the stream.
    Event::TimerPtr max_stream_duration_timer_;
    std::chrono::milliseconds idle_timeout_ms_{};
    State state_;
    absl::optional<Router::RouteConstSharedPtr> cached_route_;
    absl::optional<Upstream::ClusterInfoConstSharedPtr> cached_cluster_info_;
    const std::string* decorated_operation_{nullptr};
    std::unique_ptr<RdsRouteConfigUpdateRequester> route_config_update_requester_;
    std::unique_ptr<Tracing::CustomTagMap> tracing_custom_tags_{nullptr};
    Http::HeaderValidatorPtr header_validator_;

    friend FilterManager;

    std::unique_ptr<Buffer::OwnedImpl> deferred_data_;
  };

  using ActiveStreamPtr = std::unique_ptr<ActiveStream>;

  /**
   * Check to see if the connection can be closed after gracefully waiting to send pending codec
   * data.
   */
  void checkForDeferredClose(bool skip_deferred_close);

  /**
   * Do a delayed destruction of a stream to allow for stack unwind. Also calls onDestroy() for
   * each filter.
   */
  void doDeferredStreamDestroy(ActiveStream& stream);

  /**
   * Process a stream that is ending due to upstream response or reset.
   * If check_for_deferred_close is true, the ConnectionManager will check to
   * see if the connection was drained and should be closed if no streams remain.
   */
  void doEndStream(ActiveStream& stream, bool check_for_deferred_close = true);

  void resetAllStreams(absl::optional<StreamInfo::ResponseFlag> response_flag,
                       absl::string_view details);
  void onIdleTimeout();
  void onConnectionDurationTimeout();
  void onDrainTimeout();
  void startDrainSequence();
  Tracing::HttpTracer& tracer() { return *config_.tracer(); }
  void handleCodecError(absl::string_view error);
  void doConnectionClose(absl::optional<Network::ConnectionCloseType> close_type,
                         absl::optional<StreamInfo::ResponseFlag> response_flag,
                         absl::string_view details);
  // Returns true if a RST_STREAM for the given stream is premature. Premature
  // means the RST_STREAM arrived before response headers were sent and than
  // the stream was alive for short period of time. This period is specified
  // by the optional runtime value PrematureResetMinStreamLifetimeSecondsKey,
  // or one second if that is not present.
  bool isPrematureRstStream(const ActiveStream& stream) const;
  // Sends a GOAWAY if both sufficient streams have been closed on a connection
  // and at least half have been prematurely reset?
  void maybeDrainDueToPrematureResets();

  bool shouldDeferRequestProxyingToNextIoCycle();
  void onDeferredRequestProcessing();

  enum class DrainState { NotDraining, Draining, Closing };

  ConnectionManagerConfig& config_;
  ConnectionManagerStats& stats_; // We store a reference here to avoid an extra stats() call on
                                  // the config in the hot path.
  ServerConnectionPtr codec_;
  std::list<ActiveStreamPtr> streams_;
  Stats::TimespanPtr conn_length_;
  const Network::DrainDecision& drain_close_;
  DrainState drain_state_{DrainState::NotDraining};
  UserAgent user_agent_;
  // An idle timer for the connection. This is only armed when there are no streams on the
  // connection. When there are active streams it is disarmed in favor of each stream's
  // stream_idle_timer_.
  Event::TimerPtr connection_idle_timer_;
  // A connection duration timer. Armed during handling new connection if enabled in config.
  Event::TimerPtr connection_duration_timer_;
  Event::TimerPtr drain_timer_;
  Random::RandomGenerator& random_generator_;
  Http::Context& http_context_;
  Runtime::Loader& runtime_;
  const LocalInfo::LocalInfo& local_info_;
  Upstream::ClusterManager& cluster_manager_;
  Network::ReadFilterCallbacks* read_callbacks_{};
  ConnectionManagerListenerStats& listener_stats_;
  Server::ThreadLocalOverloadState& overload_state_;
  // References into the overload manager thread local state map. Using these lets us avoid a
  // map lookup in the hot path of processing each request.
  const Server::OverloadActionState& overload_stop_accepting_requests_ref_;
  const Server::OverloadActionState& overload_disable_keepalive_ref_;
  TimeSource& time_source_;
  bool remote_close_{};
  // Hop by hop headers should always be cleared for Envoy-as-a-proxy but will
  // not be for Envoy-mobile.
  bool clear_hop_by_hop_response_headers_{true};
  // The number of requests accumulated on the current connection.
  uint64_t accumulated_requests_{};
  // The number of requests closed on the current connection which were
  // not internally destroyed
  uint64_t closed_non_internally_destroyed_requests_{};
  // The number of requests that received a premature RST_STREAM, according to
  // the definition given in `isPrematureRstStream()`.
  uint64_t number_premature_stream_resets_{0};
  const std::string proxy_name_; // for Proxy-Status.
  uint32_t requests_during_dispatch_count_{0};
  const uint32_t max_requests_during_dispatch_{UINT32_MAX};
  Event::SchedulableCallbackPtr deferred_request_processing_callback_;
};

} // namespace Http
} // namespace Envoy
