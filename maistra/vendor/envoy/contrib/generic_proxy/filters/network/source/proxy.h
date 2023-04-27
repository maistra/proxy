#pragma once

#include <memory>
#include <string>

#include "envoy/config/core/v3/extension.pb.h"
#include "envoy/network/connection.h"
#include "envoy/network/filter.h"
#include "envoy/server/factory_context.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/linked_object.h"
#include "source/common/common/logger.h"
#include "source/common/stream_info/stream_info_impl.h"

#include "contrib/envoy/extensions/filters/network/generic_proxy/v3/generic_proxy.pb.h"
#include "contrib/envoy/extensions/filters/network/generic_proxy/v3/generic_proxy.pb.validate.h"
#include "contrib/envoy/extensions/filters/network/generic_proxy/v3/route.pb.h"
#include "contrib/generic_proxy/filters/network/source/interface/codec.h"
#include "contrib/generic_proxy/filters/network/source/interface/filter.h"
#include "contrib/generic_proxy/filters/network/source/interface/route.h"
#include "contrib/generic_proxy/filters/network/source/interface/stream.h"
#include "contrib/generic_proxy/filters/network/source/route.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace GenericProxy {

using ProxyConfig = envoy::extensions::filters::network::generic_proxy::v3::GenericProxy;
using RouteConfiguration =
    envoy::extensions::filters::network::generic_proxy::v3::RouteConfiguration;

class Filter;
class ActiveStream;

struct NamedFilterFactoryCb {
  std::string config_name_;
  FilterFactoryCb callback_;
};

class FilterConfig : public FilterChainFactory {
public:
  FilterConfig(const std::string& stat_prefix, CodecFactoryPtr codec, RouteMatcherPtr route_matcher,
               std::vector<NamedFilterFactoryCb> factories, Server::Configuration::FactoryContext&)
      : stat_prefix_(stat_prefix), codec_factory_(std::move(codec)),
        route_matcher_(std::move(route_matcher)), factories_(std::move(factories)) {}

  FilterConfig(const ProxyConfig& config, Server::Configuration::FactoryContext& context)
      : FilterConfig(config.stat_prefix(), codecFactoryFromProto(config.codec_config(), context),
                     routeMatcherFromProto(config.route_config(), context),
                     filtersFactoryFromProto(config.filters(), config.stat_prefix(), context),
                     context) {}

  RouteEntryConstSharedPtr routeEntry(const Request& request) const {
    return route_matcher_->routeEntry(request);
  }

  // FilterChainFactory
  void createFilterChain(FilterChainManager& manager) override {
    for (auto& factory : factories_) {
      manager.applyFilterFactoryCb({factory.config_name_}, factory.callback_);
    }
  }

  const CodecFactory& codecFactory() { return *codec_factory_; }

  static CodecFactoryPtr
  codecFactoryFromProto(const envoy::config::core::v3::TypedExtensionConfig& codec_config,
                        Server::Configuration::FactoryContext& context);

  static RouteMatcherPtr routeMatcherFromProto(const RouteConfiguration& route_config,
                                               Server::Configuration::FactoryContext& context);

  static std::vector<NamedFilterFactoryCb> filtersFactoryFromProto(
      const ProtobufWkt::RepeatedPtrField<envoy::config::core::v3::TypedExtensionConfig>& filters,
      const std::string stats_prefix, Server::Configuration::FactoryContext& context);

private:
  friend class ActiveStream;
  friend class Filter;

  const std::string stat_prefix_;

  CodecFactoryPtr codec_factory_;

  RouteMatcherPtr route_matcher_;

  std::vector<NamedFilterFactoryCb> factories_;
};
using FilterConfigSharedPtr = std::shared_ptr<FilterConfig>;

class ActiveStream : public FilterChainManager,
                     public LinkedObject<ActiveStream>,
                     public Envoy::Event::DeferredDeletable,
                     public ResponseEncoderCallback,
                     Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  class ActiveFilterBase : public virtual StreamFilterCallbacks {
  public:
    ActiveFilterBase(ActiveStream& parent, FilterContext context, bool is_dual)
        : parent_(parent), context_(context), is_dual_(is_dual) {}

    // StreamFilterCallbacks
    Envoy::Event::Dispatcher& dispatcher() override { return parent_.dispatcher(); }
    const CodecFactory& downstreamCodec() override { return parent_.downstreamCodec(); }
    void resetStream() override { parent_.resetStream(); }
    const RouteEntry* routeEntry() const override { return parent_.routeEntry(); }
    const RouteSpecificFilterConfig* perFilterConfig() const override {
      if (const auto* entry = parent_.routeEntry(); entry != nullptr) {
        return entry->perFilterConfig(context_.config_name);
      }
      return nullptr;
    }

    bool isDualFilter() const { return is_dual_; }

    ActiveStream& parent_;
    FilterContext context_;
    const bool is_dual_{};
  };

  class ActiveDecoderFilter : public ActiveFilterBase, public DecoderFilterCallback {
  public:
    ActiveDecoderFilter(ActiveStream& parent, FilterContext context, DecoderFilterSharedPtr filter,
                        bool is_dual)
        : ActiveFilterBase(parent, context, is_dual), filter_(std::move(filter)) {
      filter_->setDecoderFilterCallbacks(*this);
    }

    // DecoderFilterCallback
    void sendLocalReply(Status status, ResponseUpdateFunction&& func) override {
      parent_.sendLocalReply(status, std::move(func));
    }
    void continueDecoding() override { parent_.continueDecoding(); }
    void upstreamResponse(ResponsePtr response) override {
      parent_.upstreamResponse(std::move(response));
    }
    void completeDirectly() override { parent_.completeDirectly(); }

    DecoderFilterSharedPtr filter_;
  };
  using ActiveDecoderFilterPtr = std::unique_ptr<ActiveDecoderFilter>;

  class ActiveEncoderFilter : public ActiveFilterBase, public EncoderFilterCallback {
  public:
    ActiveEncoderFilter(ActiveStream& parent, FilterContext context, EncoderFilterSharedPtr filter,
                        bool is_dual)
        : ActiveFilterBase(parent, context, is_dual), filter_(std::move(filter)) {
      filter_->setEncoderFilterCallbacks(*this);
    }

    // EncoderFilterCallback
    void continueEncoding() override { parent_.continueEncoding(); }

    EncoderFilterSharedPtr filter_;
  };
  using ActiveEncoderFilterPtr = std::unique_ptr<ActiveEncoderFilter>;

  class FilterChainFactoryCallbacksHelper : public FilterChainFactoryCallbacks {
  public:
    FilterChainFactoryCallbacksHelper(ActiveStream& parent, FilterContext context)
        : parent_(parent), context_(context) {}

    // FilterChainFactoryCallbacks
    void addDecoderFilter(DecoderFilterSharedPtr filter) override {
      parent_.addDecoderFilter(
          std::make_unique<ActiveDecoderFilter>(parent_, context_, std::move(filter), false));
    }
    void addEncoderFilter(EncoderFilterSharedPtr filter) override {
      parent_.addEncoderFilter(
          std::make_unique<ActiveEncoderFilter>(parent_, context_, std::move(filter), false));
    }
    void addFilter(StreamFilterSharedPtr filter) override {
      parent_.addDecoderFilter(
          std::make_unique<ActiveDecoderFilter>(parent_, context_, filter, true));
      parent_.addEncoderFilter(
          std::make_unique<ActiveEncoderFilter>(parent_, context_, std::move(filter), true));
    }

  private:
    ActiveStream& parent_;
    FilterContext context_;
  };

  ActiveStream(Filter& parent, RequestPtr request);
  ~ActiveStream() override;

  void addDecoderFilter(ActiveDecoderFilterPtr filter) {
    decoder_filters_.emplace_back(std::move(filter));
  }
  void addEncoderFilter(ActiveEncoderFilterPtr filter) {
    encoder_filters_.emplace_back(std::move(filter));
  }

  Envoy::Event::Dispatcher& dispatcher();
  const CodecFactory& downstreamCodec();
  void resetStream();
  const RouteEntry* routeEntry() const { return cached_route_entry_.get(); }

  void sendLocalReply(Status status, ResponseUpdateFunction&&);
  void continueDecoding();
  void upstreamResponse(ResponsePtr response);
  void completeDirectly();

  void continueEncoding();

  // FilterChainManager
  void applyFilterFactoryCb(FilterContext context, FilterFactoryCb& factory) override {
    FilterChainFactoryCallbacksHelper callbacks(*this, context);
    factory(callbacks);
  }

  // ResponseEncoderCallback
  void onEncodingSuccess(Buffer::Instance& buffer, bool close_connection) override;

  std::vector<ActiveDecoderFilterPtr>& decoderFiltersForTest() { return decoder_filters_; }
  std::vector<ActiveEncoderFilterPtr>& encoderFiltersForTest() { return encoder_filters_; }
  size_t nextDecoderFilterIndexForTest() { return next_decoder_filter_index_; }
  size_t nextEncoderFilterIndexForTest() { return next_encoder_filter_index_; }

private:
  bool active_stream_reset_{false};

  Filter& parent_;

  RequestPtr downstream_request_stream_;
  ResponsePtr local_or_upstream_response_stream_;

  RouteEntryConstSharedPtr cached_route_entry_;

  std::vector<ActiveDecoderFilterPtr> decoder_filters_;
  size_t next_decoder_filter_index_{0};

  std::vector<ActiveEncoderFilterPtr> encoder_filters_;
  size_t next_encoder_filter_index_{0};
};
using ActiveStreamPtr = std::unique_ptr<ActiveStream>;

class Filter : public Envoy::Network::ReadFilter,
               public Network::ConnectionCallbacks,
               public Envoy::Logger::Loggable<Envoy::Logger::Id::filter>,
               public RequestDecoderCallback {
public:
  Filter(FilterConfigSharedPtr config, Server::Configuration::FactoryContext& context)
      : config_(std::move(config)), context_(context) {
    decoder_ = config_->codecFactory().requestDecoder();
    decoder_->setDecoderCallback(*this);
    response_encoder_ = config_->codecFactory().responseEncoder();
    creator_ = config_->codecFactory().messageCreator();
  }

  // Envoy::Network::ReadFilter
  Envoy::Network::FilterStatus onData(Envoy::Buffer::Instance& data, bool end_stream) override;
  Envoy::Network::FilterStatus onNewConnection() override {
    return Envoy::Network::FilterStatus::Continue;
  }
  void initializeReadFilterCallbacks(Envoy::Network::ReadFilterCallbacks& callbacks) override {
    callbacks_ = &callbacks;
    callbacks_->connection().addConnectionCallbacks(*this);
  }

  // RequestDecoderCallback
  void onDecodingSuccess(RequestPtr request) override;
  void onDecodingFailure() override;

  // Network::ConnectionCallbacks
  void onEvent(Network::ConnectionEvent event) override {
    if (event == Network::ConnectionEvent::Connected) {
      return;
    }
    downstream_connection_closed_ = true;
    resetStreamsForUnexpectedError();
  }
  void onAboveWriteBufferHighWatermark() override {}
  void onBelowWriteBufferLowWatermark() override {}

  void sendReplyDownstream(Response& response, ResponseEncoderCallback& callback);

  Network::Connection& connection() {
    ASSERT(callbacks_ != nullptr);
    return callbacks_->connection();
  }

  Server::Configuration::FactoryContext& factoryContext() { return context_; }

  void newDownstreamRequest(RequestPtr request);
  void deferredStream(ActiveStream& stream);

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "envoy.filters.network.generic_proxy");
  }

  std::list<ActiveStreamPtr>& activeStreamsForTest() { return active_streams_; }

  void resetStreamsForUnexpectedError();

private:
  friend class ActiveStream;

  bool downstream_connection_closed_{};

  Envoy::Network::ReadFilterCallbacks* callbacks_{nullptr};

  FilterConfigSharedPtr config_{};

  RequestDecoderPtr decoder_;
  ResponseEncoderPtr response_encoder_;
  MessageCreatorPtr creator_;

  Buffer::OwnedImpl response_buffer_;

  Server::Configuration::FactoryContext& context_;

  std::list<ActiveStreamPtr> active_streams_;
};

} // namespace GenericProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
