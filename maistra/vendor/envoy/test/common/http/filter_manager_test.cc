#include "envoy/common/optref.h"
#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"
#include "envoy/matcher/matcher.h"
#include "envoy/stream_info/filter_state.h"

#include "common/http/filter_manager.h"
#include "common/matcher/exact_map_matcher.h"
#include "common/stream_info/filter_state_impl.h"
#include "common/stream_info/stream_info_impl.h"

#include "test/mocks/event/mocks.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/local_reply/mocks.h"
#include "test/mocks/network/mocks.h"

#include "gtest/gtest.h"

using testing::Return;

namespace Envoy {
namespace Http {
namespace {
class FilterManagerTest : public testing::Test {
public:
  void initialize() {
    filter_manager_ = std::make_unique<FilterManager>(
        filter_manager_callbacks_, dispatcher_, connection_, 0, true, 10000, filter_factory_,
        local_reply_, protocol_, time_source_, filter_state_,
        StreamInfo::FilterState::LifeSpan::Connection);
  }

  std::unique_ptr<FilterManager> filter_manager_;
  NiceMock<MockFilterManagerCallbacks> filter_manager_callbacks_;
  Event::MockDispatcher dispatcher_;
  NiceMock<Network::MockConnection> connection_;
  Envoy::Http::MockFilterChainFactory filter_factory_;
  LocalReply::MockLocalReply local_reply_;
  Protocol protocol_{Protocol::Http2};
  NiceMock<MockTimeSystem> time_source_;
  StreamInfo::FilterStateSharedPtr filter_state_ =
      std::make_shared<StreamInfo::FilterStateImpl>(StreamInfo::FilterState::LifeSpan::Connection);
};

// Verifies that the local reply persists the gRPC classification even if the request headers are
// modified.
TEST_F(FilterManagerTest, SendLocalReplyDuringDecodingGrpcClassiciation) {
  initialize();

  std::shared_ptr<MockStreamDecoderFilter> filter(new NiceMock<MockStreamDecoderFilter>());

  EXPECT_CALL(*filter, decodeHeaders(_, true))
      .WillRepeatedly(Invoke([&](RequestHeaderMap& headers, bool) -> FilterHeadersStatus {
        headers.setContentType("text/plain");

        filter->callbacks_->sendLocalReply(Code::InternalServerError, "", nullptr, absl::nullopt,
                                           "");

        return FilterHeadersStatus::StopIteration;
      }));

  RequestHeaderMapPtr grpc_headers{
      new TestRequestHeaderMapImpl{{":authority", "host"},
                                   {":path", "/"},
                                   {":method", "GET"},
                                   {"content-type", "application/grpc"}}};

  ON_CALL(filter_manager_callbacks_, requestHeaders())
      .WillByDefault(Return(makeOptRef(*grpc_headers)));

  EXPECT_CALL(filter_factory_, createFilterChain(_))
      .WillRepeatedly(Invoke([&](FilterChainFactoryCallbacks& callbacks) -> void {
        callbacks.addStreamDecoderFilter(filter);
      }));

  filter_manager_->createFilterChain();

  filter_manager_->requestHeadersInitialized();
  EXPECT_CALL(local_reply_, rewrite(_, _, _, _, _, _));
  EXPECT_CALL(filter_manager_callbacks_, setResponseHeaders_(_))
      .WillOnce(Invoke([](auto& response_headers) {
        EXPECT_THAT(response_headers,
                    HeaderHasValueRef(Http::Headers::get().ContentType, "application/grpc"));
      }));
  EXPECT_CALL(filter_manager_callbacks_, resetIdleTimer());
  EXPECT_CALL(filter_manager_callbacks_, encodeHeaders(_, _));
  EXPECT_CALL(filter_manager_callbacks_, endStream());
  filter_manager_->decodeHeaders(*grpc_headers, true);
  filter_manager_->destroyFilters();
}

// Verifies that the local reply persists the gRPC classification even if the request headers are
// modified when directly encoding a response.
TEST_F(FilterManagerTest, SendLocalReplyDuringEncodingGrpcClassiciation) {
  initialize();

  std::shared_ptr<MockStreamDecoderFilter> decoder_filter(new NiceMock<MockStreamDecoderFilter>());

  EXPECT_CALL(*decoder_filter, decodeHeaders(_, true))
      .WillRepeatedly(Invoke([&](RequestHeaderMap& headers, bool) -> FilterHeadersStatus {
        headers.setContentType("text/plain");

        ResponseHeaderMapPtr response_headers{new TestResponseHeaderMapImpl{{":status", "200"}}};
        decoder_filter->callbacks_->encodeHeaders(std::move(response_headers), true, "test");

        return FilterHeadersStatus::StopIteration;
      }));

  std::shared_ptr<MockStreamFilter> encoder_filter(new NiceMock<MockStreamFilter>());

  EXPECT_CALL(*encoder_filter, encodeHeaders(_, true))
      .WillRepeatedly(Invoke([&](auto&, bool) -> FilterHeadersStatus {
        encoder_filter->encoder_callbacks_->sendLocalReply(Code::InternalServerError, "", nullptr,
                                                           absl::nullopt, "");
        return FilterHeadersStatus::StopIteration;
      }));

  EXPECT_CALL(filter_factory_, createFilterChain(_))
      .WillRepeatedly(Invoke([&](FilterChainFactoryCallbacks& callbacks) -> void {
        callbacks.addStreamDecoderFilter(decoder_filter);
        callbacks.addStreamFilter(encoder_filter);
      }));

  RequestHeaderMapPtr grpc_headers{
      new TestRequestHeaderMapImpl{{":authority", "host"},
                                   {":path", "/"},
                                   {":method", "GET"},
                                   {"content-type", "application/grpc"}}};

  ON_CALL(filter_manager_callbacks_, requestHeaders())
      .WillByDefault(Return(makeOptRef(*grpc_headers)));
  filter_manager_->createFilterChain();

  filter_manager_->requestHeadersInitialized();
  EXPECT_CALL(local_reply_, rewrite(_, _, _, _, _, _));
  EXPECT_CALL(filter_manager_callbacks_, setResponseHeaders_(_))
      .WillOnce(Invoke([](auto&) {}))
      .WillOnce(Invoke([](auto& response_headers) {
        EXPECT_THAT(response_headers,
                    HeaderHasValueRef(Http::Headers::get().ContentType, "application/grpc"));
      }));
  EXPECT_CALL(filter_manager_callbacks_, encodeHeaders(_, _));
  EXPECT_CALL(filter_manager_callbacks_, endStream());
  filter_manager_->decodeHeaders(*grpc_headers, true);
  filter_manager_->destroyFilters();
}

Matcher::MatchTreeSharedPtr<HttpMatchingData> createRequestMatchingTree() {
  auto tree = std::make_shared<Matcher::ExactMapMatcher<HttpMatchingData>>(
      std::make_unique<HttpRequestHeadersDataInput>("match-header"), absl::nullopt);

  tree->addChild("match", Matcher::OnMatch<HttpMatchingData>{
                              []() { return std::make_unique<SkipAction>(); }, nullptr});

  return tree;
}

Matcher::MatchTreeSharedPtr<HttpMatchingData> createRequestAndResponseMatchingTree() {
  auto tree = std::make_shared<Matcher::ExactMapMatcher<HttpMatchingData>>(
      std::make_unique<HttpResponseHeadersDataInput>("match-header"), absl::nullopt);

  tree->addChild("match",
                 Matcher::OnMatch<HttpMatchingData>{[]() { return std::make_unique<SkipAction>(); },
                                                    createRequestMatchingTree()});

  return tree;
}

TEST_F(FilterManagerTest, MatchTreeSkipActionDecodingHeaders) {
  initialize();

  // The filter is added, but since we match on the request header we skip the filter.
  std::shared_ptr<MockStreamDecoderFilter> decoder_filter(new MockStreamDecoderFilter());
  EXPECT_CALL(*decoder_filter, setDecoderFilterCallbacks(_));
  EXPECT_CALL(*decoder_filter, onDestroy());

  EXPECT_CALL(filter_factory_, createFilterChain(_))
      .WillRepeatedly(Invoke([&](FilterChainFactoryCallbacks& callbacks) -> void {
        callbacks.addStreamDecoderFilter(decoder_filter, createRequestMatchingTree());
      }));

  RequestHeaderMapPtr grpc_headers{
      new TestRequestHeaderMapImpl{{":authority", "host"},
                                   {":path", "/"},
                                   {":method", "GET"},
                                   {"match-header", "match"},
                                   {"content-type", "application/grpc"}}};

  ON_CALL(filter_manager_callbacks_, requestHeaders())
      .WillByDefault(Return(makeOptRef(*grpc_headers)));
  filter_manager_->createFilterChain();

  filter_manager_->requestHeadersInitialized();
  filter_manager_->decodeHeaders(*grpc_headers, true);
  filter_manager_->destroyFilters();
}

TEST_F(FilterManagerTest, MatchTreeSkipActionRequestAndResponseHeaders) {
  initialize();

  EXPECT_CALL(dispatcher_, pushTrackedObject(_));
  EXPECT_CALL(dispatcher_, popTrackedObject(_));

  // This stream filter will skip further callbacks once it sees both the request and response
  // header. As such, it should see the decoding callbacks but none of the encoding callbacks.
  auto stream_filter = std::make_shared<MockStreamFilter>();
  EXPECT_CALL(*stream_filter, setDecoderFilterCallbacks(_));
  EXPECT_CALL(*stream_filter, setEncoderFilterCallbacks(_));
  EXPECT_CALL(*stream_filter, onDestroy());
  EXPECT_CALL(*stream_filter, decodeHeaders(_, false))
      .WillOnce(Return(FilterHeadersStatus::Continue));
  EXPECT_CALL(*stream_filter, decodeData(_, true)).WillOnce(Return(FilterDataStatus::Continue));

  auto decoder_filter = std::make_shared<NiceMock<Envoy::Http::MockStreamDecoderFilter>>();
  EXPECT_CALL(*decoder_filter, decodeHeaders(_, false))
      .WillOnce(Return(FilterHeadersStatus::StopIteration));
  EXPECT_CALL(*decoder_filter, decodeData(_, true))
      .WillOnce(Invoke([&](auto&, bool) -> FilterDataStatus {
        ResponseHeaderMapPtr headers{new TestResponseHeaderMapImpl{
            {":status", "200"}, {"match-header", "match"}, {"content-type", "application/grpc"}}};
        decoder_filter->callbacks_->encodeHeaders(std::move(headers), false, "details");

        Buffer::OwnedImpl data("data");
        decoder_filter->callbacks_->encodeData(data, true);
        return FilterDataStatus::StopIterationNoBuffer;
      }));

  EXPECT_CALL(filter_factory_, createFilterChain(_))
      .WillRepeatedly(Invoke([&](FilterChainFactoryCallbacks& callbacks) -> void {
        callbacks.addStreamFilter(stream_filter, createRequestAndResponseMatchingTree());
        callbacks.addStreamDecoderFilter(decoder_filter);
      }));

  RequestHeaderMapPtr headers{new TestRequestHeaderMapImpl{{":authority", "host"},
                                                           {":path", "/"},
                                                           {":method", "GET"},
                                                           {"match-header", "match"},
                                                           {"content-type", "application/grpc"}}};
  Buffer::OwnedImpl data("data");

  ON_CALL(filter_manager_callbacks_, requestHeaders())
      .WillByDefault(Return((makeOptRef(*headers))));
  filter_manager_->createFilterChain();

  EXPECT_CALL(filter_manager_callbacks_, encodeHeaders(_, _));
  EXPECT_CALL(filter_manager_callbacks_, endStream());

  filter_manager_->requestHeadersInitialized();
  filter_manager_->decodeHeaders(*headers, false);
  filter_manager_->decodeData(data, true);
  filter_manager_->destroyFilters();
}
} // namespace
} // namespace Http
} // namespace Envoy
