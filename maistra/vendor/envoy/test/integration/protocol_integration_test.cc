#include "test/integration/protocol_integration_test.h"

#include <functional>
#include <list>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "envoy/buffer/buffer.h"
#include "envoy/config/bootstrap/v3/bootstrap.pb.h"
#include "envoy/config/route/v3/route_components.pb.h"
#include "envoy/event/dispatcher.h"
#include "envoy/extensions/filters/network/http_connection_manager/v3/http_connection_manager.pb.h"
#include "envoy/http/header_map.h"
#include "envoy/registry/registry.h"

#include "source/common/api/api_impl.h"
#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/fmt.h"
#include "source/common/common/thread_annotations.h"
#include "source/common/http/headers.h"
#include "source/common/http/utility.h"
#include "source/common/network/utility.h"
#include "source/common/protobuf/utility.h"
#include "source/common/runtime/runtime_impl.h"
#include "source/common/upstream/upstream_impl.h"

#include "test/common/http/http2/http2_frame.h"
#include "test/common/upstream/utility.h"
#include "test/integration/autonomous_upstream.h"
#include "test/integration/http_integration.h"
#include "test/integration/socket_interface_swap.h"
#include "test/integration/test_host_predicate_config.h"
#include "test/integration/utility.h"
#include "test/mocks/upstream/retry_priority.h"
#include "test/mocks/upstream/retry_priority_factory.h"
#include "test/test_common/environment.h"
#include "test/test_common/logging.h"
#include "test/test_common/network_utility.h"
#include "test/test_common/registry.h"
#include "test/test_common/threadsafe_singleton_injector.h"

#include "absl/time/time.h"
#include "gtest/gtest.h"

using testing::HasSubstr;
using testing::Not;

namespace Envoy {

void setDoNotValidateRouteConfig(
    envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager& hcm) {
  auto* route_config = hcm.mutable_route_config();
  route_config->mutable_validate_clusters()->set_value(false);
};

TEST_P(ProtocolIntegrationTest, TrailerSupportHttp1) {
  config_helper_.addConfigModifier(setEnableDownstreamTrailersHttp1());
  config_helper_.addConfigModifier(setEnableUpstreamTrailersHttp1());

  testTrailers(10, 20, true, true);
}

TEST_P(ProtocolIntegrationTest, ShutdownWithActiveConnPoolConnections) {
  auto response = makeHeaderOnlyRequest(nullptr, 0);
  // Shut down the server with active connection pool connections.
  test_server_.reset();
  checkSimpleRequestSuccess(0U, 0U, response.get());
}

// Change the default route to be restrictive, and send a request to an alternate route.
TEST_P(DownstreamProtocolIntegrationTest, RouterNotFound) { testRouterNotFound(); }

TEST_P(ProtocolIntegrationTest, RouterVirtualClusters) { testRouterVirtualClusters(); }

// Change the default route to be restrictive, and send a POST to an alternate route.
TEST_P(DownstreamProtocolIntegrationTest, RouterNotFoundBodyNoBuffer) {
  testRouterNotFoundWithBody();
}

// Add a route that uses unknown cluster (expect 404 Not Found).
TEST_P(DownstreamProtocolIntegrationTest, RouterClusterNotFound404) {
  config_helper_.addConfigModifier(&setDoNotValidateRouteConfig);
  config_helper_.addConfigModifier(configureProxyStatus());
  auto host = config_helper_.createVirtualHost("foo.lyft.com", "/unknown", "unknown_cluster");
  host.mutable_routes(0)->mutable_route()->set_cluster_not_found_response_code(
      envoy::config::route::v3::RouteAction::NOT_FOUND);
  config_helper_.addVirtualHost(host);
  initialize();

  BufferingStreamDecoderPtr response = IntegrationUtil::makeSingleRequest(
      lookupPort("http"), "GET", "/unknown", "", downstream_protocol_, version_, "foo.lyft.com");
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("404", response->headers().getStatusValue());
  EXPECT_EQ(response->headers().getProxyStatusValue(),
            "envoy; error=destination_unavailable; details=\"cluster_not_found; NC\"");
}

TEST_P(DownstreamProtocolIntegrationTest, TestHostWhitespacee) {
  config_helper_.addConfigModifier(&setDoNotValidateRouteConfig);
  auto host = config_helper_.createVirtualHost("foo.lyft.com", "/unknown", "unknown_cluster");
  host.mutable_routes(0)->mutable_route()->set_cluster_not_found_response_code(
      envoy::config::route::v3::RouteAction::NOT_FOUND);
  config_helper_.addVirtualHost(host);
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder = codec_client_->startRequest(Http::TestRequestHeaderMapImpl{
      {":method", "GET"}, {":authority", " foo.lyft.com "}, {":path", "/unknown"}});
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  // For HTTP/1 the whitespace will be stripped, and 404 returned as above.
  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(response->waitForEndStream());
    EXPECT_EQ("404", response->headers().getStatusValue());
    EXPECT_TRUE(response->complete());
  } else {
    // For HTTP/2 and above, the whitespace is illegal.
    ASSERT_TRUE(response->waitForReset());
    ASSERT_TRUE(codec_client_->waitForDisconnect());
  }
}

// Add a route that uses unknown cluster (expect 503 Service Unavailable).
TEST_P(DownstreamProtocolIntegrationTest, RouterClusterNotFound503) {
  config_helper_.addConfigModifier(&setDoNotValidateRouteConfig);
  auto host = config_helper_.createVirtualHost("foo.lyft.com", "/unknown", "unknown_cluster");
  host.mutable_routes(0)->mutable_route()->set_cluster_not_found_response_code(
      envoy::config::route::v3::RouteAction::SERVICE_UNAVAILABLE);
  config_helper_.addVirtualHost(host);
  initialize();

  BufferingStreamDecoderPtr response = IntegrationUtil::makeSingleRequest(
      lookupPort("http"), "GET", "/unknown", "", downstream_protocol_, version_, "foo.lyft.com");
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("503", response->headers().getStatusValue());
}

// Add a route which redirects HTTP to HTTPS, and verify Envoy sends a 301
TEST_P(DownstreamProtocolIntegrationTest, RouterRedirectHttpRequest) {
  autonomous_upstream_ = true;
  useAccessLog("%DOWNSTREAM_WIRE_BYTES_SENT% %DOWNSTREAM_WIRE_BYTES_RECEIVED% "
               "%DOWNSTREAM_HEADER_BYTES_SENT% %DOWNSTREAM_HEADER_BYTES_RECEIVED%");
  auto host = config_helper_.createVirtualHost("www.redirect.com", "/");
  host.set_require_tls(envoy::config::route::v3::VirtualHost::ALL);
  config_helper_.addVirtualHost(host);
  initialize();

  BufferingStreamDecoderPtr response = IntegrationUtil::makeSingleRequest(
      lookupPort("http"), "GET", "/foo", "", downstream_protocol_, version_, "www.redirect.com");
  ASSERT_TRUE(response->complete());
  if (downstream_protocol_ <= Http::CodecType::HTTP2) {
    EXPECT_EQ("301", response->headers().getStatusValue());
    EXPECT_EQ("https://www.redirect.com/foo",
              response->headers().get(Http::Headers::get().Location)[0]->value().getStringView());
    expectDownstreamBytesSentAndReceived(BytesCountExpectation(145, 45, 111, 23),
                                         BytesCountExpectation(0, 30, 0, 30),
                                         BytesCountExpectation(0, 30, 0, 30));
  } else {
    // All QUIC requests use https, and should not be redirected. (Even those sent with http scheme
    // will be overridden to https by HCM.)
    EXPECT_EQ("200", response->headers().getStatusValue());
  }
}

TEST_P(ProtocolIntegrationTest, UnknownResponsecode) {
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  Http::TestResponseHeaderMapImpl response_headers{{":status", "600"}};
  auto response = sendRequestAndWaitForResponse(default_request_headers_, 0, response_headers, 0);

  // Regression test https://github.com/envoyproxy/envoy/issues/14890 - no content-length added.
  EXPECT_EQ(upstream_request_->headers().ContentLength(), nullptr);
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("600", response->headers().getStatusValue());
}

TEST_P(DownstreamProtocolIntegrationTest, AddInvalidDecodedData) {
  EXPECT_ENVOY_BUG(
      {
        useAccessLog("%RESPONSE_CODE_DETAILS%");
        config_helper_.prependFilter(R"EOF(
  name: add-invalid-data-filter
  )EOF");
        initialize();
        codec_client_ = makeHttpConnection(lookupPort("http"));
        auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
        waitForNextUpstreamRequest();
        upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);
        ASSERT_TRUE(response->waitForEndStream());
        EXPECT_EQ("502", response->headers().getStatusValue());
        EXPECT_THAT(waitForAccessLog(access_log_name_),
                    HasSubstr("filter_added_invalid_request_data"));
      },
      "Invalid request data");
}

TEST_P(DownstreamProtocolIntegrationTest, AddInvalidEncodedData) {
  EXPECT_ENVOY_BUG(
      {
        useAccessLog("%RESPONSE_CODE_DETAILS%");
        config_helper_.prependFilter(R"EOF(
  name: add-invalid-data-filter
  )EOF");
        initialize();
        codec_client_ = makeHttpConnection(lookupPort("http"));
        default_request_headers_.setCopy(Envoy::Http::LowerCaseString("invalid-encode"), "yes");
        auto response = std::move((codec_client_->startRequest(default_request_headers_)).second);
        ASSERT_TRUE(response->waitForEndStream());
        EXPECT_EQ("502", response->headers().getStatusValue());
        EXPECT_THAT(waitForAccessLog(access_log_name_),
                    HasSubstr("filter_added_invalid_response_data"));
      },
      "Invalid response data");
}

// Verifies behavior for https://github.com/envoyproxy/envoy/pull/11248
TEST_P(ProtocolIntegrationTest, AddBodyToRequestAndWaitForIt) {
  config_helper_.prependFilter(R"EOF(
  name: wait-for-whole-request-and-response-filter
  )EOF");
  config_helper_.prependFilter(R"EOF(
  name: add-body-filter
  )EOF");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest();
  EXPECT_EQ("body", upstream_request_->body().toString());
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, false);
  // encode data, as we have a separate test for the transforming header only response.
  upstream_request_->encodeData(128, true);
  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

TEST_P(ProtocolIntegrationTest, AddBodyToResponseAndWaitForIt) {
  config_helper_.prependFilter(R"EOF(
  name: add-body-filter
  )EOF");
  config_helper_.prependFilter(R"EOF(
  name: wait-for-whole-request-and-response-filter
  )EOF");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(default_request_headers_, 128);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);
  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ("body", response->body());
}

TEST_P(ProtocolIntegrationTest, ContinueHeadersOnlyInjectBodyFilter) {
  config_helper_.prependFilter(R"EOF(
  name: continue-headers-only-inject-body-filter
  )EOF");
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Send a headers only request.
  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest();

  // Make sure that the body was injected to the request.
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(upstream_request_->receivedData());
  EXPECT_EQ(upstream_request_->body().toString(), "body");

  // Send a headers only response.
  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());

  // Make sure that the body was injected to the response.
  EXPECT_TRUE(response->complete());
  EXPECT_EQ(response->body(), "body");
}

TEST_P(ProtocolIntegrationTest, StopIterationHeadersInjectBodyFilter) {
  config_helper_.prependFilter(R"EOF(
  name: stop-iteration-headers-inject-body-filter
  )EOF");
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Send a headers only request.
  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest();

  // Make sure that the body was injected to the request.
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(upstream_request_->receivedData());
  EXPECT_EQ(upstream_request_->body().toString(), "body");

  // Send a headers only response.
  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());

  // Make sure that the body was injected to the response.
  EXPECT_TRUE(response->complete());
  EXPECT_EQ(response->body(), "body");
}

TEST_P(ProtocolIntegrationTest, AddEncodedTrailers) {
  config_helper_.prependFilter(R"EOF(
name: add-trailers-filter
)EOF");
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeRequestWithBody(default_request_headers_, 128);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, false);
  upstream_request_->encodeData(128, true);
  ASSERT_TRUE(response->waitForEndStream());

  if (upstreamProtocol() >= Http::CodecType::HTTP2) {
    EXPECT_EQ("decode", upstream_request_->trailers()
                            ->get(Http::LowerCaseString("grpc-message"))[0]
                            ->value()
                            .getStringView());
  }
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("503", response->headers().getStatusValue());
  if (downstream_protocol_ >= Http::CodecType::HTTP2) {
    EXPECT_EQ("encode", response->trailers()->getGrpcMessageValue());
  }
}

// Regression test for https://github.com/envoyproxy/envoy/issues/9873
TEST_P(ProtocolIntegrationTest, ResponseWithHostHeader) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"}});
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(
      Http::TestResponseHeaderMapImpl{{":status", "200"}, {"host", "host"}}, true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ("host",
            response->headers().get(Http::LowerCaseString("host"))[0]->value().getStringView());
}

// Upstream 304 response with Content-Length and no actual body
// The content-length header should be preserved and no transfer-encoding header should be added
TEST_P(ProtocolIntegrationTest, Upstream304ResponseWithContentLength) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"if-none-match", "\"1234567890\""}});
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "304"},
                                                                   {"etag", "\"1234567890\""},
                                                                   {"content-length", "123"}},
                                   true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("304", response->headers().getStatusValue());
  EXPECT_EQ(
      "123",
      response->headers().get(Http::LowerCaseString("content-length"))[0]->value().getStringView());
  EXPECT_TRUE(response->headers().get(Http::LowerCaseString("transfer-encoding")).empty());

  // Make a HEAD request to make sure the previous 304 response with Content-Length did not cause
  // issue.
  response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "HEAD"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"if-none-match", "\"1234567890\""}});
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "304"},
                                                                   {"etag", "\"1234567890\""},
                                                                   {"content-length", "123"}},
                                   true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("304", response->headers().getStatusValue());
  EXPECT_EQ(
      "123",
      response->headers().get(Http::LowerCaseString("content-length"))[0]->value().getStringView());
  EXPECT_TRUE(response->headers().get(Http::LowerCaseString("transfer-encoding")).empty());
}

// Upstream 304 response without Content-Length
// No content-length nor transfer-encoding header should be added for all protocol combinations.
TEST_P(ProtocolIntegrationTest, 304ResponseWithoutContentLength) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"if-none-match", "\"1234567890\""}});
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(
      Http::TestResponseHeaderMapImpl{{":status", "304"}, {"etag", "\"1234567890\""}}, true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("304", response->headers().getStatusValue());
  EXPECT_TRUE(response->headers().get(Http::LowerCaseString("transfer-encoding")).empty());
  EXPECT_TRUE(response->headers().get(Http::LowerCaseString("content-length")).empty());
}

// Upstream 304 response for HEAD request without Content-Length
// Response to HEAD request is the same as response to GET request and consistent with different
// protocol combinations.
TEST_P(ProtocolIntegrationTest, 304HeadResponseWithoutContentLength) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "HEAD"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"if-none-match", "\"1234567890\""}});
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(
      Http::TestResponseHeaderMapImpl{{":status", "304"}, {"etag", "\"1234567890\""}}, true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("304", response->headers().getStatusValue());
  EXPECT_TRUE(response->headers().get(Http::LowerCaseString("transfer-encoding")).empty());
  EXPECT_TRUE(response->headers().get(Http::LowerCaseString("content-length")).empty());
}

// Tests that the response to a HEAD request can have content-length header but empty body.
TEST_P(ProtocolIntegrationTest, 200HeadResponseWithContentLength) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "HEAD"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"if-none-match", "\"1234567890\""}});
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(
      Http::TestResponseHeaderMapImpl{{":status", "200"}, {"content-length", "123"}}, true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ(
      "123",
      response->headers().get(Http::LowerCaseString("content-length"))[0]->value().getStringView());
}

// Tests missing headers needed for H/1 codec first line.
TEST_P(DownstreamProtocolIntegrationTest, DownstreamRequestWithFaultyFilter) {
  if (upstreamProtocol() == Http::CodecType::HTTP3) {
    // For QUIC, even through the headers are not sent upstream, the stream will
    // be created. Use the autonomous upstream and allow incomplete streams.
    autonomous_allow_incomplete_streams_ = true;
    autonomous_upstream_ = true;
  }
  useAccessLog("%RESPONSE_CODE_DETAILS%");
  config_helper_.prependFilter("{ name: invalid-header-filter }");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Missing method
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"remove-method", "yes"}});
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("503", response->headers().getStatusValue());
  EXPECT_THAT(waitForAccessLog(access_log_name_), HasSubstr("missing_required_header"));

  // Missing path for non-CONNECT
  response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"remove-path", "yes"}});
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("503", response->headers().getStatusValue());
  EXPECT_THAT(waitForAccessLog(access_log_name_), HasSubstr("missing_required_header"));
}

TEST_P(DownstreamProtocolIntegrationTest, FaultyFilterWithConnect) {
  if (upstreamProtocol() == Http::CodecType::HTTP3) {
    // For QUIC, even through the headers are not sent upstream, the stream will
    // be created. Use the autonomous upstream and allow incomplete streams.
    autonomous_allow_incomplete_streams_ = true;
    autonomous_upstream_ = true;
  }
  // Faulty filter that removed host in a CONNECT request.
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        ConfigHelper::setConnectConfig(hcm, false, false,
                                       downstreamProtocol() == Http::CodecType::HTTP3);
      });
  useAccessLog("%RESPONSE_CODE_DETAILS%");
  config_helper_.prependFilter("{ name: invalid-header-filter }");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Missing host for CONNECT
  auto headers = Http::TestRequestHeaderMapImpl{
      {":method", "CONNECT"}, {":scheme", "http"}, {":authority", "www.host.com:80"}};

  auto response = std::move((codec_client_->startRequest(headers)).second);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("503", response->headers().getStatusValue());
  EXPECT_THAT(waitForAccessLog(access_log_name_), HasSubstr("missing_required_header"));
}

TEST_P(DownstreamProtocolIntegrationTest, MissingHeadersLocalReply) {
  useAccessLog("%RESPONSE_CODE_DETAILS%");
  config_helper_.prependFilter("{ name: invalid-header-filter }");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Missing method
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"remove-method", "yes"},
                                     {"send-reply", "yes"}});
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_THAT(waitForAccessLog(access_log_name_), HasSubstr("invalid_header_filter_ready\n"));
}

TEST_P(DownstreamProtocolIntegrationTest, MissingHeadersLocalReplyDownstreamBytesCount) {
  useAccessLog("%DOWNSTREAM_WIRE_BYTES_SENT% %DOWNSTREAM_WIRE_BYTES_RECEIVED% "
               "%DOWNSTREAM_HEADER_BYTES_SENT% %DOWNSTREAM_HEADER_BYTES_RECEIVED%");
  config_helper_.addFilter("{ name: invalid-header-filter }");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Missing method
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"remove-method", "yes"},
                                     {"send-reply", "yes"}});
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  expectDownstreamBytesSentAndReceived(BytesCountExpectation(90, 88, 71, 54),
                                       BytesCountExpectation(0, 58, 0, 58),
                                       BytesCountExpectation(7, 10, 7, 8));
}

TEST_P(DownstreamProtocolIntegrationTest, MissingHeadersLocalReplyUpstreamBytesCount) {
  useAccessLog("%UPSTREAM_WIRE_BYTES_SENT% %UPSTREAM_WIRE_BYTES_RECEIVED% "
               "%UPSTREAM_HEADER_BYTES_SENT% %UPSTREAM_HEADER_BYTES_RECEIVED%");
  config_helper_.addFilter("{ name: invalid-header-filter }");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Missing method
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"remove-method", "yes"},
                                     {"send-reply", "yes"}});
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  expectUpstreamBytesSentAndReceived(BytesCountExpectation(0, 0, 0, 0),
                                     BytesCountExpectation(0, 0, 0, 0),
                                     BytesCountExpectation(0, 0, 0, 0));
}

TEST_P(DownstreamProtocolIntegrationTest, MissingHeadersLocalReplyWithBody) {
  useAccessLog("%RESPONSE_CODE_DETAILS%");
  config_helper_.prependFilter("{ name: invalid-header-filter }");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Missing method
  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"remove-method", "yes"},
                                     {"send-reply", "yes"}},
      1024);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_THAT(waitForAccessLog(access_log_name_), HasSubstr("invalid_header_filter_ready\n"));
}

TEST_P(DownstreamProtocolIntegrationTest, MissingHeadersLocalReplyWithBodyBytesCount) {
  useAccessLog("%DOWNSTREAM_WIRE_BYTES_SENT% %DOWNSTREAM_WIRE_BYTES_RECEIVED% "
               "%DOWNSTREAM_HEADER_BYTES_SENT% %DOWNSTREAM_HEADER_BYTES_RECEIVED%");
  config_helper_.addFilter("{ name: invalid-header-filter }");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Missing method
  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"remove-method", "yes"},
                                     {"send-reply", "yes"}},
      1024);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  expectDownstreamBytesSentAndReceived(BytesCountExpectation(109, 1152, 90, 81),
                                       BytesCountExpectation(0, 58, 0, 58),
                                       BytesCountExpectation(7, 10, 7, 8));
}

// Regression test for https://github.com/envoyproxy/envoy/issues/10270
TEST_P(ProtocolIntegrationTest, LongHeaderValueWithSpaces) {
  // Header with at least 20kb of spaces surrounded by non-whitespace characters to ensure that
  // dispatching is split across 2 dispatch calls. This threshold comes from Envoy preferring 16KB
  // reads, which the buffer rounds up to about 20KB when allocating slices in
  // Buffer::OwnedImpl::reserve().
  const std::string long_header_value_with_inner_lws = "v" + std::string(32 * 1024, ' ') + "v";
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"longrequestvalue", long_header_value_with_inner_lws}});
  waitForNextUpstreamRequest();
  EXPECT_EQ(long_header_value_with_inner_lws, upstream_request_->headers()
                                                  .get(Http::LowerCaseString("longrequestvalue"))[0]
                                                  ->value()
                                                  .getStringView());
  upstream_request_->encodeHeaders(
      Http::TestResponseHeaderMapImpl{{":status", "200"},
                                      {"host", "host"},
                                      {"longresponsevalue", long_header_value_with_inner_lws}},
      true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ("host",
            response->headers().get(Http::LowerCaseString("host"))[0]->value().getStringView());
  EXPECT_EQ(long_header_value_with_inner_lws,
            response->headers()
                .get(Http::LowerCaseString("longresponsevalue"))[0]
                ->value()
                .getStringView());
}

TEST_P(ProtocolIntegrationTest, Retry) {
  config_helper_.addConfigModifier([&](envoy::config::bootstrap::v3::Bootstrap& bootstrap) -> void {
    RELEASE_ASSERT(bootstrap.mutable_static_resources()->clusters_size() == 1, "");
    auto& cluster = *bootstrap.mutable_static_resources()->mutable_clusters(0);
    cluster.mutable_track_cluster_stats()->set_request_response_sizes(true);
  });
  useAccessLog("%UPSTREAM_WIRE_BYTES_SENT% %UPSTREAM_WIRE_BYTES_RECEIVED% "
               "%UPSTREAM_HEADER_BYTES_SENT% %UPSTREAM_HEADER_BYTES_RECEIVED%");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"x-forwarded-for", "10.0.0.1"},
                                     {"x-envoy-retry-on", "5xx"}},
      1024);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, false);

  if (fake_upstreams_[0]->httpType() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
    ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_,
                                                          std::chrono::milliseconds(500)));
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
  }
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(512, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_EQ(1024U, upstream_request_->bodyLength());

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ(512U, response->body().size());
  Stats::Store& stats = test_server_->server().stats();
  if (upstreamProtocol() == Http::CodecType::HTTP2) {
    Stats::CounterSharedPtr counter = TestUtility::findCounter(
        stats, absl::StrCat("cluster.cluster_0.", upstreamProtocolStatsRoot(), ".tx_reset"));
    ASSERT_NE(nullptr, counter);
    EXPECT_EQ(1L, counter->value());
  }
  EXPECT_NE(nullptr,
            test_server_->counter(absl::StrCat("cluster.cluster_0.", upstreamProtocolStatsRoot(),
                                               ".dropped_headers_with_underscores")));

  test_server_->waitUntilHistogramHasSamples("cluster.cluster_0.upstream_rq_headers_size");
  test_server_->waitUntilHistogramHasSamples("cluster.cluster_0.upstream_rs_headers_size");

  auto find_histo_sample_count = [&](const std::string& name) -> uint64_t {
    for (auto& histogram : test_server_->histograms()) {
      if (histogram->name() == name) {
        return TestUtility::readSampleCount(test_server_->server().dispatcher(), *histogram);
      }
    }
    return 0;
  };

  EXPECT_EQ(find_histo_sample_count("cluster.cluster_0.upstream_rq_headers_size"), 2);
  EXPECT_EQ(find_histo_sample_count("cluster.cluster_0.upstream_rs_headers_size"), 2);

  // The two requests are sent with https scheme rather than http for QUIC downstream.
  const size_t quic_https_extra_bytes = (downstreamProtocol() == Http::CodecType::HTTP3 ? 2u : 0u);
  const size_t http2_header_bytes_received =
      (GetParam().http2_implementation == Http2Impl::Oghttp2) ? 24 : 27;
  expectUpstreamBytesSentAndReceived(
      BytesCountExpectation(2566 + quic_https_extra_bytes, 635, 430 + quic_https_extra_bytes, 54),
      BytesCountExpectation(2262, 548, 196, http2_header_bytes_received),
      BytesCountExpectation(2204, 520, 150, 6));
}

TEST_P(ProtocolIntegrationTest, RetryStreaming) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":scheme", "http"},
                                                                 {":authority", "sni.lyft.com"},
                                                                 {"x-forwarded-for", "10.0.0.1"},
                                                                 {"x-envoy-retry-on", "5xx"}});
  auto& encoder = encoder_decoder.first;
  auto& response = encoder_decoder.second;

  // Send some data, but not the entire body.
  std::string data(1024, 'a');
  Buffer::OwnedImpl send1(data);
  encoder.encodeData(send1, false);

  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));

  // Send back an upstream failure.
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, false);

  if (fake_upstreams_[0]->httpType() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
    ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
  }

  // Wait for a retry. Ensure all data, both before and after the retry, is received.
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));

  // Finish the request.
  std::string data2(512, 'b');
  Buffer::OwnedImpl send2(data2);
  encoder.encodeData(send2, true);
  std::string combined_request_data = data + data2;
  ASSERT_TRUE(upstream_request_->waitForData(*dispatcher_, combined_request_data));

  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(512, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_EQ(combined_request_data.size(), upstream_request_->bodyLength());

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ(512U, response->body().size());
}

// Regression test https://github.com/envoyproxy/envoy/issues/11131
// Send complete response headers directing a retry and reset the stream to make
// sure that Envoy cleans up stream state correctly when doing a retry with
// complete response but incomplete request.
TEST_P(ProtocolIntegrationTest, RetryStreamingReset) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":scheme", "http"},
                                                                 {":authority", "sni.lyft.com"},
                                                                 {"x-forwarded-for", "10.0.0.1"},
                                                                 {"x-envoy-retry-on", "5xx"}});
  auto& encoder = encoder_decoder.first;
  auto& response = encoder_decoder.second;

  // Send some data, but not the entire body.
  std::string data(1024, 'a');
  Buffer::OwnedImpl send1(data);
  encoder.encodeData(send1, false);

  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));

  // Send back an upstream failure and end stream. Make sure an immediate reset
  // doesn't cause problems. Schedule via the upstream_request_ dispatcher to ensure that the stream
  // still exists when encoding the reset stream.
  upstream_request_->postToConnectionThread([this]() {
    upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, true);
    upstream_request_->encodeResetStream();
  });

  // Make sure the fake stream is reset.
  if (fake_upstreams_[0]->httpType() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
    ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
  }

  // Wait for a retry. Ensure all data, both before and after the retry, is received.
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));

  // Finish the request.
  std::string data2(512, 'b');
  Buffer::OwnedImpl send2(data2);
  encoder.encodeData(send2, true);
  std::string combined_request_data = data + data2;
  ASSERT_TRUE(upstream_request_->waitForData(*dispatcher_, combined_request_data));

  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(512, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_EQ(combined_request_data.size(), upstream_request_->bodyLength());

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ(512U, response->body().size());
}

TEST_P(ProtocolIntegrationTest, RetryStreamingCancelDueToBufferOverflow) {
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             hcm) {
        auto* route = hcm.mutable_route_config()->mutable_virtual_hosts(0)->mutable_routes(0);

        route->mutable_per_request_buffer_limit_bytes()->set_value(1024);
        route->mutable_route()
            ->mutable_retry_policy()
            ->mutable_retry_back_off()
            ->mutable_base_interval()
            ->MergeFrom(
                ProtobufUtil::TimeUtil::MillisecondsToDuration(100000000)); // Effectively infinity.
      });
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":scheme", "http"},
                                                                 {":authority", "sni.lyft.com"},
                                                                 {"x-forwarded-for", "10.0.0.1"},
                                                                 {"x-envoy-retry-on", "5xx"}});
  auto& encoder = encoder_decoder.first;
  auto& response = encoder_decoder.second;

  // Send some data, but less than the buffer limit, and not end-stream
  std::string data(64, 'a');
  Buffer::OwnedImpl send1(data);
  encoder.encodeData(send1, false);

  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));

  // Send back an upstream failure.
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, false);

  if (fake_upstreams_[0]->httpType() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
  }

  // Overflow the request buffer limit. Because the retry base interval is infinity, no
  // request will be in progress. This will cause the request to be aborted and an error
  // to be returned to the client.
  std::string data2(2048, 'b');
  Buffer::OwnedImpl send2(data2);
  encoder.encodeData(send2, false);

  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("507", response->headers().getStatusValue());
  test_server_->waitForCounterEq("cluster.cluster_0.retry_or_shadow_abandoned", 1);
}

// Tests that the x-envoy-attempt-count header is properly set on the upstream request and the
// downstream response, and updated after the request is retried.
TEST_P(DownstreamProtocolIntegrationTest, RetryAttemptCountHeader) {
  auto host = config_helper_.createVirtualHost("sni.lyft.com", "/test_retry");
  host.set_include_request_attempt_count(true);
  host.set_include_attempt_count_in_response(true);
  config_helper_.addVirtualHost(host);
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/test_retry"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"x-forwarded-for", "10.0.0.1"},
                                     {"x-envoy-retry-on", "5xx"}},
      1024);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, false);

  EXPECT_EQ(atoi(std::string(upstream_request_->headers().getEnvoyAttemptCountValue()).c_str()), 1);

  if (fake_upstreams_[0]->httpType() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
    ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
  }
  waitForNextUpstreamRequest();
  EXPECT_EQ(atoi(std::string(upstream_request_->headers().getEnvoyAttemptCountValue()).c_str()), 2);
  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(512, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_EQ(1024U, upstream_request_->bodyLength());

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ(512U, response->body().size());
  EXPECT_EQ(2, atoi(std::string(response->headers().getEnvoyAttemptCountValue()).c_str()));
}

// Verifies that a retry priority can be configured and affect the host selected during retries.
// The retry priority will always target P1, which would otherwise never be hit due to P0 being
// healthy.
TEST_P(DownstreamProtocolIntegrationTest, RetryPriority) {
  const Upstream::HealthyLoad healthy_priority_load({0u, 100u});
  const Upstream::DegradedLoad degraded_priority_load({0u, 100u});
  NiceMock<Upstream::MockRetryPriority> retry_priority(healthy_priority_load,
                                                       degraded_priority_load);
  Upstream::MockRetryPriorityFactory factory(retry_priority);

  Registry::InjectFactory<Upstream::RetryPriorityFactory> inject_factory(factory);

  // Add route with custom retry policy
  auto host = config_helper_.createVirtualHost("sni.lyft.com", "/test_retry");
  host.set_include_request_attempt_count(true);
  auto retry_policy = host.mutable_routes(0)->mutable_route()->mutable_retry_policy();
  retry_policy->mutable_retry_priority()->set_name(factory.name());
  config_helper_.addVirtualHost(host);
  // We want to work with a cluster with two hosts.
  config_helper_.addConfigModifier([this](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
    auto* cluster = bootstrap.mutable_static_resources()->mutable_clusters(0);
    auto* load_assignment = cluster->mutable_load_assignment();
    load_assignment->clear_endpoints();

    for (int i = 0; i < 2; ++i) {
      auto locality = load_assignment->add_endpoints();
      locality->set_priority(i);
      locality->mutable_locality()->set_region("region");
      locality->mutable_locality()->set_zone("zone");
      locality->mutable_locality()->set_sub_zone("sub_zone" + std::to_string(i));
      locality->add_lb_endpoints()->mutable_endpoint()->MergeFrom(
          ConfigHelper::buildEndpoint(Network::Test::getLoopbackAddressString(version_)));
    }
  });
  fake_upstreams_count_ = 2;
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/test_retry"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"x-forwarded-for", "10.0.0.1"},
                                     {"x-envoy-retry-on", "5xx"}},
      1024);

  // Note how we're expecting each upstream request to hit the same upstream.
  waitForNextUpstreamRequest(0);
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, false);

  if (fake_upstreams_[0]->httpType() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
    ASSERT_TRUE(fake_upstreams_[1]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
  }

  if (upstreamProtocol() == Http::CodecType::HTTP3) {
    // Make sure waitForNextUpstreamRequest waits for a new connection.
    fake_upstream_connection_.reset();
  }
  waitForNextUpstreamRequest(1);
  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(512, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_EQ(1024U, upstream_request_->bodyLength());

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ(512U, response->body().size());
}

//
// Verifies that a retry host filter can be configured and affect the host selected during retries.
// The predicate will keep track of the first host attempted, and attempt to route all requests to
// the same host. With a total of two upstream hosts, this should result in us continuously sending
// requests to the same host.
TEST_P(DownstreamProtocolIntegrationTest, RetryHostPredicateFilter) {
  TestHostPredicateFactory predicate_factory;
  Registry::InjectFactory<Upstream::RetryHostPredicateFactory> inject_factory(predicate_factory);

  // Add route with custom retry policy
  auto host = config_helper_.createVirtualHost("sni.lyft.com", "/test_retry");
  host.set_include_request_attempt_count(true);
  auto retry_policy = host.mutable_routes(0)->mutable_route()->mutable_retry_policy();
  retry_policy->add_retry_host_predicate()->set_name(predicate_factory.name());
  config_helper_.addVirtualHost(host);

  // We want to work with a cluster with two hosts.
  config_helper_.addConfigModifier([this](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
    bootstrap.mutable_static_resources()
        ->mutable_clusters(0)
        ->mutable_load_assignment()
        ->mutable_endpoints(0)
        ->add_lb_endpoints()
        ->mutable_endpoint()
        ->MergeFrom(ConfigHelper::buildEndpoint(Network::Test::getLoopbackAddressString(version_)));
  });
  fake_upstreams_count_ = 2;
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/test_retry"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"x-forwarded-for", "10.0.0.1"},
                                     {"x-envoy-retry-on", "5xx"}},
      1024);

  // Note how we're expecting each upstream request to hit the same upstream.
  auto upstream_idx = waitForNextUpstreamRequest({0, 1});
  ASSERT_TRUE(upstream_idx.has_value());
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, false);

  if (fake_upstreams_[*upstream_idx]->httpType() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
    ASSERT_TRUE(fake_upstreams_[*upstream_idx]->waitForHttpConnection(*dispatcher_,
                                                                      fake_upstream_connection_));
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
  }

  waitForNextUpstreamRequest(*upstream_idx);
  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(512, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_EQ(1024U, upstream_request_->bodyLength());

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ(512U, response->body().size());
}

// Very similar set-up to testRetry but with a 65k request the request will not
// be buffered and the 503 will be returned to the user.
TEST_P(ProtocolIntegrationTest, RetryHittingBufferLimit) {
  config_helper_.setBufferLimits(1024, 1024); // Set buffer limits upstream and downstream.
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"x-forwarded-for", "10.0.0.1"},
                                     {"x-envoy-retry-on", "5xx"}},
      1024 * 65);
  waitForNextUpstreamRequest();

  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_EQ(66560U, upstream_request_->bodyLength());

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("503", response->headers().getStatusValue());
}

// Very similar set-up to RetryHittingBufferLimits but using the route specific cap.
TEST_P(ProtocolIntegrationTest, RetryHittingRouteLimits) {
  auto host = config_helper_.createVirtualHost("routelimit.lyft.com", "/");
  host.mutable_per_request_buffer_limit_bytes()->set_value(0);
  config_helper_.addVirtualHost(host);
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/"},
                                     {":scheme", "http"},
                                     {":authority", "routelimit.lyft.com"},
                                     {"x-forwarded-for", "10.0.0.1"},
                                     {"x-envoy-retry-on", "5xx"}},
      1);
  waitForNextUpstreamRequest();

  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_EQ(1U, upstream_request_->bodyLength());

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("503", response->headers().getStatusValue());
}

// Test hitting the decoder buffer filter with too many request bytes to buffer. Ensure the
// connection manager sends a 413.
TEST_P(DownstreamProtocolIntegrationTest, HittingDecoderFilterLimit) {
  config_helper_.prependFilter("{ name: encoder-decoder-buffer-filter }");
  config_helper_.setBufferLimits(1024, 1024);
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/dynamo/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"x-forwarded-for", "10.0.0.1"},
                                     {"x-envoy-retry-on", "5xx"}},
      1024 * 65);

  ASSERT_TRUE(response->waitForEndStream());
  // With HTTP/1 there's a possible race where if the connection backs up early,
  // the 413-and-connection-close may be sent while the body is still being
  // sent, resulting in a write error and the connection being closed before the
  // response is read.
  if (downstream_protocol_ >= Http::CodecType::HTTP2) {
    ASSERT_TRUE(response->complete());
  }
  if (response->complete()) {
    EXPECT_EQ("413", response->headers().getStatusValue());
  }
}

// Test hitting the encoder buffer filter with too many response bytes to buffer. Given the request
// headers are sent on early, the stream/connection will be reset.
TEST_P(ProtocolIntegrationTest, HittingEncoderFilterLimit) {
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        auto* route_config = hcm.mutable_route_config();
        auto* virtual_host = route_config->mutable_virtual_hosts(0);
        auto* header = virtual_host->mutable_response_headers_to_add()->Add()->mutable_header();
        header->set_key("foo");
        header->set_value("bar");
      });

  useAccessLog();
  config_helper_.prependFilter("{ name: encoder-decoder-buffer-filter }");
  config_helper_.setBufferLimits(1024, 1024);
  initialize();

  // Send the request.
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  Buffer::OwnedImpl data("HTTP body content goes here");
  codec_client_->sendData(*downstream_request, data, true);
  waitForNextUpstreamRequest();

  // Send the response headers.
  upstream_request_->encodeHeaders(default_response_headers_, false);

  // Now send an overly large response body. At some point, too much data will
  // be buffered, the stream will be reset, and the connection will disconnect.
  upstream_request_->encodeData(1024 * 65, false);
  if (upstreamProtocol() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
    ASSERT_TRUE(fake_upstream_connection_->close());
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
  }

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("500", response->headers().getStatusValue());
  // Regression test all sendLocalReply paths add route-requested headers.
  auto foo = Http::LowerCaseString("foo");
  ASSERT_FALSE(response->headers().get(foo).empty());
  EXPECT_EQ("bar", response->headers().get(foo)[0]->value().getStringView());

  // Regression test https://github.com/envoyproxy/envoy/issues/9881 by making
  // sure this path does standard HCM header transformations.
  EXPECT_TRUE(response->headers().Date() != nullptr);
  EXPECT_THAT(waitForAccessLog(access_log_name_), HasSubstr("500"));
  test_server_->waitForCounterEq("http.config_test.downstream_rq_5xx", 1);
}

// The downstream connection is closed when it is read disabled, and on OSX the
// connection error is not detected under these circumstances.
#if !defined(__APPLE__)
TEST_P(ProtocolIntegrationTest, 1xxAndClose) { testEnvoyHandling1xx(false, "", true); }
#endif

TEST_P(ProtocolIntegrationTest, EnvoyHandling1xx) { testEnvoyHandling1xx(); }

TEST_P(ProtocolIntegrationTest, EnvoyHandlingDuplicate1xx) { testEnvoyHandling1xx(true); }

// 100-continue before the request completes.
TEST_P(ProtocolIntegrationTest, EnvoyProxyingEarly1xx) { testEnvoyProxying1xx(true); }

// Multiple 1xx before the request completes.
TEST_P(ProtocolIntegrationTest, EnvoyProxyingEarlyMultiple1xx) {
  testEnvoyProxying1xx(true, false, true);
}

// 100-continue after the request completes.
TEST_P(ProtocolIntegrationTest, EnvoyProxyingLate1xx) { testEnvoyProxying1xx(false); }

// Multiple 1xx after the request completes.
TEST_P(ProtocolIntegrationTest, EnvoyProxyingLateMultiple1xx) {
  testEnvoyProxying1xx(false, false, true);
}

TEST_P(ProtocolIntegrationTest, EnvoyProxying102) {
  testEnvoyProxying1xx(false, false, false, "102");
}

TEST_P(ProtocolIntegrationTest, EnvoyProxying103) {
  testEnvoyProxying1xx(false, false, false, "103");
}

TEST_P(ProtocolIntegrationTest, TwoRequests) { testTwoRequests(); }

TEST_P(ProtocolIntegrationTest, TwoRequestsWithForcedBackup) { testTwoRequests(true); }

TEST_P(ProtocolIntegrationTest, BasicMaxStreamDuration) {
  config_helper_.addConfigModifier([](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
    ConfigHelper::HttpProtocolOptions protocol_options;
    auto* http_protocol_options = protocol_options.mutable_common_http_protocol_options();
    http_protocol_options->mutable_max_stream_duration()->MergeFrom(
        ProtobufUtil::TimeUtil::MillisecondsToDuration(200));
    ConfigHelper::setProtocolOptions(*bootstrap.mutable_static_resources()->mutable_clusters(0),
                                     protocol_options);
  });
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));

  test_server_->waitForCounterGe("cluster.cluster_0.upstream_rq_max_duration_reached", 1);

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(codec_client_->waitForDisconnect());
    ASSERT_TRUE(response->complete());
  } else {
    ASSERT_TRUE(response->waitForEndStream());
    codec_client_->close();
  }
}

TEST_P(ProtocolIntegrationTest, BasicDynamicMaxStreamDuration) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  default_request_headers_.setEnvoyUpstreamStreamDurationMs(500);
  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));

  test_server_->waitForCounterGe("cluster.cluster_0.upstream_rq_max_duration_reached", 1);

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(codec_client_->waitForDisconnect());
    ASSERT_TRUE(response->complete());
  } else {
    ASSERT_TRUE(response->waitForEndStream());
    codec_client_->close();
  }
}

TEST_P(ProtocolIntegrationTest, MaxStreamDurationWithRetryPolicy) {
  config_helper_.addConfigModifier([](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
    ConfigHelper::HttpProtocolOptions protocol_options;
    auto* http_protocol_options = protocol_options.mutable_common_http_protocol_options();
    http_protocol_options->mutable_max_stream_duration()->MergeFrom(
        ProtobufUtil::TimeUtil::MillisecondsToDuration(1000));
    ConfigHelper::setProtocolOptions(*bootstrap.mutable_static_resources()->mutable_clusters(0),
                                     protocol_options);
  });
  Http::TestRequestHeaderMapImpl retriable_header =
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"x-forwarded-for", "10.0.0.1"},
                                     {"x-envoy-retry-on", "5xx"}};

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto encoder_decoder = codec_client_->startRequest(retriable_header);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));
  ASSERT_TRUE(upstream_request_->waitForHeadersComplete());

  if (fake_upstreams_[0]->httpType() == FakeHttpConnection::Type::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
    ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
  }

  test_server_->waitForCounterGe("cluster.cluster_0.upstream_rq_max_duration_reached", 1);

  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));

  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  upstream_request_->encodeHeaders(response_headers, true);

  response->waitForHeaders();
  codec_client_->close();

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

TEST_P(ProtocolIntegrationTest, MaxStreamDurationWithRetryPolicyWhenRetryUpstreamDisconnection) {
  config_helper_.addConfigModifier([](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
    ConfigHelper::HttpProtocolOptions protocol_options;
    auto* http_protocol_options = protocol_options.mutable_common_http_protocol_options();
    http_protocol_options->mutable_max_stream_duration()->MergeFrom(
        ProtobufUtil::TimeUtil::MillisecondsToDuration(1000));
    ConfigHelper::setProtocolOptions(*bootstrap.mutable_static_resources()->mutable_clusters(0),
                                     protocol_options);
  });
  Http::TestRequestHeaderMapImpl retriable_header =
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"x-forwarded-for", "10.0.0.1"},
                                     {"x-envoy-retry-on", "5xx"}};

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto encoder_decoder = codec_client_->startRequest(retriable_header);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));
  ASSERT_TRUE(upstream_request_->waitForHeadersComplete());

  if (fake_upstreams_[0]->httpType() == FakeHttpConnection::Type::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
    ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
  }

  test_server_->waitForCounterGe("cluster.cluster_0.upstream_rq_max_duration_reached", 1);

  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));

  test_server_->waitForCounterGe("cluster.cluster_0.upstream_rq_max_duration_reached", 2);
  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(codec_client_->waitForDisconnect());
    ASSERT_TRUE(response->complete());
  } else {
    ASSERT_TRUE(response->waitForEndStream());
    codec_client_->close();
  }

  EXPECT_EQ("408", response->headers().getStatusValue());
}

// Verify that headers with underscores in their names are dropped from client requests
// but remain in upstream responses.
TEST_P(ProtocolIntegrationTest, HeadersWithUnderscoresDropped) {
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        hcm.mutable_common_http_protocol_options()->set_headers_with_underscores_action(
            envoy::config::core::v3::HttpProtocolOptions::DROP_HEADER);
      });
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"foo_bar", "baz"}});
  waitForNextUpstreamRequest();

  EXPECT_THAT(upstream_request_->headers(), Not(HeaderHasValueRef("foo_bar", "baz")));
  upstream_request_->encodeHeaders(
      Http::TestResponseHeaderMapImpl{{":status", "200"}, {"bar_baz", "fooz"}}, true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_THAT(response->headers(), HeaderHasValueRef("bar_baz", "fooz"));
  Stats::Store& stats = test_server_->server().stats();
  std::string stat_name;
  switch (downstreamProtocol()) {
  case Http::CodecType::HTTP1:
    stat_name = "http1.dropped_headers_with_underscores";
    break;
  case Http::CodecType::HTTP2:
    stat_name = "http2.dropped_headers_with_underscores";
    break;
  case Http::CodecType::HTTP3:
    stat_name = "http3.dropped_headers_with_underscores";
    break;
  default:
    RELEASE_ASSERT(false, fmt::format("Unknown downstream protocol {}",
                                      static_cast<int>(downstream_protocol_)));
  };
  EXPECT_EQ(1L, TestUtility::findCounter(stats, stat_name)->value());
}

// Verify that by default headers with underscores in their names remain in both requests and
// responses when allowed in configuration.
TEST_P(ProtocolIntegrationTest, HeadersWithUnderscoresRemainByDefault) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"foo_bar", "baz"}});
  waitForNextUpstreamRequest();

  EXPECT_THAT(upstream_request_->headers(), HeaderHasValueRef("foo_bar", "baz"));
  upstream_request_->encodeHeaders(
      Http::TestResponseHeaderMapImpl{{":status", "200"}, {"bar_baz", "fooz"}}, true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_THAT(response->headers(), HeaderHasValueRef("bar_baz", "fooz"));
}

// Verify that request with headers containing underscores is rejected when configured.
TEST_P(DownstreamProtocolIntegrationTest, HeadersWithUnderscoresCauseRequestRejectedByDefault) {
  useAccessLog("%RESPONSE_FLAGS% %RESPONSE_CODE_DETAILS%");
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        hcm.mutable_common_http_protocol_options()->set_headers_with_underscores_action(
            envoy::config::core::v3::HttpProtocolOptions::REJECT_REQUEST);
      });
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"foo_bar", "baz"}});

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(codec_client_->waitForDisconnect());
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("400", response->headers().getStatusValue());
  } else {
    ASSERT_TRUE(response->waitForReset());
    codec_client_->close();
    ASSERT_TRUE(response->reset());
    EXPECT_EQ(Http::StreamResetReason::RemoteReset, response->resetReason());
  }
  EXPECT_THAT(waitForAccessLog(access_log_name_), HasSubstr("unexpected_underscore"));
}

TEST_P(DownstreamProtocolIntegrationTest, ValidZeroLengthContent) {
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":path", "/test/long/url"},
                                                 {":scheme", "http"},
                                                 {":authority", "sni.lyft.com"},
                                                 {"content-length", "0"}};
  auto response = sendRequestAndWaitForResponse(request_headers, 0, default_response_headers_, 0);

  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

// Test we're following https://tools.ietf.org/html/rfc7230#section-3.3.2
// as best we can.
TEST_P(ProtocolIntegrationTest, 304WithBody) {
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);

  waitForNextUpstreamRequest();
  Http::TestResponseHeaderMapImpl response_headers{{":status", "304"}, {"content-length", "2"}};
  ASSERT(upstream_request_ != nullptr);
  upstream_request_->encodeHeaders(response_headers, false);
  response->waitForHeaders();
  EXPECT_EQ("304", response->headers().getStatusValue());

  // For HTTP/1.1 http_parser is explicitly told that 304s are header-only
  // requests.
  if (downstream_protocol_ == Http::CodecType::HTTP1 ||
      upstreamProtocol() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(response->complete());
  } else {
    ASSERT_FALSE(response->complete());
  }

  upstream_request_->encodeData(2, true);
  if (upstreamProtocol() == Http::CodecType::HTTP1) {
    // Any body sent after the request is considered complete will not be handled as part of the
    // active request, but will be flagged as a protocol error for the no-longer-associated
    // connection.
    // Ideally if we got the body with the headers we would instead reset the
    // stream, but it turns out that's complicated so instead we consistently
    // forward the headers and error out after.
    test_server_->waitForCounterGe("cluster.cluster_0.upstream_cx_protocol_error", 1);
  }

  // Only for HTTP/2 and Http/3, where streams are ended with an explicit end-stream so we
  // can differentiate between 304-with-advertised-but-absent-body and
  // 304-with-body, is there a protocol error on the active stream.
  if (downstream_protocol_ >= Http::CodecType::HTTP2 &&
      upstreamProtocol() >= Http::CodecType::HTTP2) {
    ASSERT_TRUE(response->waitForReset());
  }
}

TEST_P(ProtocolIntegrationTest, OverflowingResponseCode) {
  useAccessLog("%RESPONSE_CODE_DETAILS%");
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  if (upstreamProtocol() == Http::CodecType::HTTP1) {
    // The http1 codec won't send illegal status codes so send raw HTTP/1.1
    FakeRawConnectionPtr fake_upstream_connection;
    ASSERT_TRUE(fake_upstreams_[0]->waitForRawConnection(fake_upstream_connection));
    ASSERT(fake_upstream_connection != nullptr);
    ASSERT_TRUE(fake_upstream_connection->write(
        "HTTP/1.1 11111111111111111111111111111111111111111111111111111111111111111 OK\r\n",
        false));
    ASSERT_TRUE(fake_upstream_connection->waitForDisconnect());
  } else {
    waitForNextUpstreamRequest();
    default_response_headers_.setStatus(
        "11111111111111111111111111111111111111111111111111111111111111111");
    upstream_request_->encodeHeaders(default_response_headers_, false);
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
  }

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_EQ("502", response->headers().getStatusValue());
  EXPECT_THAT(waitForAccessLog(access_log_name_), HasSubstr("protocol_error"));
}

TEST_P(ProtocolIntegrationTest, MissingStatus) {
  initialize();

  // HTTP1, uses a defined protocol which doesn't split up messages into raw byte frames
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);

  if (upstreamProtocol() == Http::CodecType::HTTP1) {
    FakeRawConnectionPtr fake_upstream_connection;
    ASSERT_TRUE(fake_upstreams_[0]->waitForRawConnection(fake_upstream_connection));
    ASSERT(fake_upstream_connection != nullptr);
    ASSERT_TRUE(fake_upstream_connection->write("HTTP/1.1 OK\r\n", false));
    ASSERT_TRUE(fake_upstream_connection->waitForDisconnect());
  } else if (upstreamProtocol() == Http::CodecType::HTTP2) {
    FakeRawConnectionPtr fake_upstream_connection;
    ASSERT_TRUE(fake_upstreams_[0]->waitForRawConnection(fake_upstream_connection));
    Http::Http2::Http2Frame::SettingsFlags settings_flags =
        static_cast<Http::Http2::Http2Frame::SettingsFlags>(0);
    Http2Frame setting_frame = Http::Http2::Http2Frame::makeEmptySettingsFrame(settings_flags);
    // Ack settings
    settings_flags = static_cast<Http::Http2::Http2Frame::SettingsFlags>(1); // ack
    Http2Frame ack_frame = Http::Http2::Http2Frame::makeEmptySettingsFrame(settings_flags);
    ASSERT(fake_upstream_connection->write(std::string(setting_frame))); // empty settings
    ASSERT(fake_upstream_connection->write(std::string(ack_frame)));     // ack setting
    Http::Http2::Http2Frame missing_status = Http::Http2::Http2Frame::makeHeadersFrameNoStatus(1);
    ASSERT_TRUE(fake_upstream_connection->write(std::string(missing_status)));
    ASSERT_TRUE(fake_upstream_connection->waitForDisconnect());
  } else {
    ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
    waitForNextUpstreamRequest();
    default_response_headers_.removeStatus();
    upstream_request_->encodeHeaders(default_response_headers_, false);
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
  }

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_EQ("502", response->headers().getStatusValue());
}

// Validate that lots of tiny cookies doesn't cause a DoS (single cookie header).
TEST_P(DownstreamProtocolIntegrationTest, LargeCookieParsingConcatenated) {
  if (downstreamProtocol() == Http::CodecType::HTTP3) {
    // QUICHE Qpack splits concatenated cookies into crumbs to increase
    // compression ratio. On the receiver side, the total size of these crumbs
    // may be larger than coalesced cookie headers. Increase the max request
    // header size to avoid QUIC_HEADERS_TOO_LARGE stream error.
    config_helper_.addConfigModifier(
        [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
                hcm) -> void {
          hcm.mutable_max_request_headers_kb()->set_value(96);
          hcm.mutable_common_http_protocol_options()->mutable_max_headers_count()->set_value(8000);
        });
  }
  if (upstreamProtocol() == Http::CodecType::HTTP3) {
    setMaxRequestHeadersKb(96);
    setMaxRequestHeadersCount(8000);
  }
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":path", "/test/long/url"},
                                                 {":scheme", "http"},
                                                 {":authority", "sni.lyft.com"},
                                                 {"content-length", "0"}};
  std::vector<std::string> cookie_pieces;
  cookie_pieces.reserve(7000);
  for (int i = 0; i < 7000; i++) {
    cookie_pieces.push_back(fmt::sprintf("a%x=b", i));
  }
  request_headers.addCopy("cookie", absl::StrJoin(cookie_pieces, "; "));
  auto response = sendRequestAndWaitForResponse(request_headers, 0, default_response_headers_, 0);

  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

// Validate that lots of tiny cookies doesn't cause a DoS (many cookie headers).
TEST_P(DownstreamProtocolIntegrationTest, LargeCookieParsingMany) {
  // Set header count limit to 2010.
  uint32_t max_count = 2010;
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        hcm.mutable_common_http_protocol_options()->mutable_max_headers_count()->set_value(
            max_count);
      });
  setMaxRequestHeadersCount(max_count);

  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":path", "/test/long/url"},
                                                 {":scheme", "http"},
                                                 {":authority", "sni.lyft.com"},
                                                 {"content-length", "0"}};
  for (int i = 0; i < 2000; i++) {
    request_headers.addCopy("cookie", fmt::sprintf("a%x=b", i));
  }
  auto response = sendRequestAndWaitForResponse(request_headers, 0, default_response_headers_, 0);

  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

TEST_P(DownstreamProtocolIntegrationTest, InvalidContentLength) {
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":authority", "sni.lyft.com"},
                                                                 {"content-length", "-1"}});
  auto response = std::move(encoder_decoder.second);

  ASSERT_TRUE(codec_client_->waitForDisconnect());

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("400", response->headers().getStatusValue());
    test_server_->waitForCounterGe("http.config_test.downstream_rq_4xx", 1);
  } else {
    ASSERT_TRUE(response->reset());
    EXPECT_EQ(Http::StreamResetReason::ConnectionTermination, response->resetReason());
  }
}

TEST_P(DownstreamProtocolIntegrationTest, InvalidContentLengthAllowed) {
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             hcm) -> void {
        hcm.mutable_http3_protocol_options()
            ->mutable_override_stream_error_on_invalid_http_message()
            ->set_value(true);
        hcm.mutable_http2_protocol_options()
            ->mutable_override_stream_error_on_invalid_http_message()
            ->set_value(true);
        hcm.mutable_http_protocol_options()
            ->mutable_override_stream_error_on_invalid_http_message()
            ->set_value(true);
      });

  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":authority", "sni.lyft.com"},
                                                                 {"content-length", "-1"}});
  auto response = std::move(encoder_decoder.second);

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(codec_client_->waitForDisconnect());
  } else {
    ASSERT_TRUE(response->waitForReset());
    codec_client_->close();
  }

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("400", response->headers().getStatusValue());
  } else {
    ASSERT_TRUE(response->reset());
    EXPECT_EQ(Http::StreamResetReason::RemoteReset, response->resetReason());
  }
}

TEST_P(DownstreamProtocolIntegrationTest, MultipleContentLengths) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":authority", "sni.lyft.com"},
                                                                 {"content-length", "3,2"}});
  auto response = std::move(encoder_decoder.second);

  ASSERT_TRUE(codec_client_->waitForDisconnect());

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("400", response->headers().getStatusValue());
  } else {
    ASSERT_TRUE(response->reset());
    EXPECT_EQ(Http::StreamResetReason::ConnectionTermination, response->resetReason());
  }
}

// TODO(PiotrSikora): move this HTTP/2 only variant to http2_integration_test.cc.
TEST_P(DownstreamProtocolIntegrationTest, MultipleContentLengthsAllowed) {
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             hcm) -> void {
        hcm.mutable_http3_protocol_options()
            ->mutable_override_stream_error_on_invalid_http_message()
            ->set_value(true);
        hcm.mutable_http2_protocol_options()
            ->mutable_override_stream_error_on_invalid_http_message()
            ->set_value(true);
      });

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":authority", "sni.lyft.com"},
                                                                 {"content-length", "3,2"}});
  auto response = std::move(encoder_decoder.second);

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(codec_client_->waitForDisconnect());
  } else {
    ASSERT_TRUE(response->waitForReset());
    codec_client_->close();
  }

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("400", response->headers().getStatusValue());
  } else {
    ASSERT_TRUE(response->reset());
    EXPECT_EQ(Http::StreamResetReason::RemoteReset, response->resetReason());
  }
}

TEST_P(DownstreamProtocolIntegrationTest, LocalReplyDuringEncoding) {
  config_helper_.prependFilter(R"EOF(
name: local-reply-during-encode
)EOF");
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"}});

  // Wait for the upstream request and begin sending a response with end_stream = false.
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "503"}}, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("500", response->headers().getStatusValue());
  EXPECT_EQ(0, upstream_request_->body().length());
}

TEST_P(DownstreamProtocolIntegrationTest, LocalReplyDuringEncodingData) {
  config_helper_.prependFilter(R"EOF(
name: local-reply-during-encode-data
)EOF");
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);

  // Wait for the upstream request and begin sending a response with end_stream = false.
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(size_, false);
  Http::TestResponseTrailerMapImpl response_trailers{{"response", "trailer"}};
  upstream_request_->encodeTrailers(response_trailers);

  // Response was aborted after headers were sent to the client.
  // The stream was reset. Client does not receive body or trailers.
  ASSERT_TRUE(response->waitForReset());
  EXPECT_FALSE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
  EXPECT_EQ(0, response->body().length());
  EXPECT_EQ(nullptr, response->trailers());
}

TEST_P(DownstreamProtocolIntegrationTest, LargeRequestUrlRejected) {
  // Send one 95 kB URL with limit 60 kB headers.
  testLargeRequestUrl(95, 60);
}

TEST_P(DownstreamProtocolIntegrationTest, LargeRequestUrlAccepted) {
  // Send one 95 kB URL with limit 96 kB headers.
  testLargeRequestUrl(95, 96);
}

TEST_P(DownstreamProtocolIntegrationTest, LargeRequestHeadersRejected) {
  // Send one 95 kB header with limit 60 kB and 100 headers.
  testLargeRequestHeaders(95, 1, 60, 100);
}

TEST_P(DownstreamProtocolIntegrationTest, VeryLargeRequestHeadersRejected) {
  // Send one very large 600 kB header with limit 500 kB and 100 headers.
  // The limit and the header size are set in such a way to accommodate for flow control limits.
  // If the headers are too large and the flow control blocks the response is truncated and the test
  // flakes.
  testLargeRequestHeaders(600, 1, 500, 100);
}

TEST_P(DownstreamProtocolIntegrationTest, LargeRequestHeadersAccepted) {
  // Send one 100 kB header with limit 8192 kB (8 MB) and 100 headers.
  testLargeRequestHeaders(100, 1, 8192, 100);
}

TEST_P(DownstreamProtocolIntegrationTest, ManyLargeRequestHeadersAccepted) {
  // Send 70 headers each of size 100 kB with limit 8192 kB (8 MB) and 100 headers.
  testLargeRequestHeaders(100, 70, 8192, 100, TSAN_TIMEOUT_FACTOR * TestUtility::DefaultTimeout);
}

TEST_P(DownstreamProtocolIntegrationTest, ManyRequestHeadersRejected) {
  // Send 101 empty headers with limit 60 kB and 100 headers.
  testLargeRequestHeaders(0, 101, 60, 80);
}

TEST_P(DownstreamProtocolIntegrationTest, ManyRequestHeadersAccepted) {
  // Send 145 empty headers with limit 60 kB and 150 headers.
  testLargeRequestHeaders(0, 140, 60, 150);
}

TEST_P(DownstreamProtocolIntegrationTest, ManyRequestTrailersRejected) {
  // Default header (and trailer) count limit is 100.
  config_helper_.addConfigModifier(setEnableDownstreamTrailersHttp1());
  config_helper_.addConfigModifier(setEnableUpstreamTrailersHttp1());
  Http::TestRequestTrailerMapImpl request_trailers;
  for (int i = 0; i < 150; i++) {
    request_trailers.addCopy("trailer", std::string(1, 'a'));
  }

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  EXPECT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));
  codec_client_->sendData(*request_encoder_, 1, false);
  codec_client_->sendTrailers(*request_encoder_, request_trailers);

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(codec_client_->waitForDisconnect());
    EXPECT_TRUE(response->complete());
    EXPECT_EQ("431", response->headers().getStatusValue());
  } else {
    ASSERT_TRUE(response->waitForReset());
    codec_client_->close();
  }
}

TEST_P(DownstreamProtocolIntegrationTest, ManyRequestTrailersAccepted) {
  // Set header (and trailer) count limit to 200.
  uint32_t max_count = 200;
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        hcm.mutable_common_http_protocol_options()->mutable_max_headers_count()->set_value(
            max_count);
      });
  config_helper_.addConfigModifier(setEnableUpstreamTrailersHttp1());
  setMaxRequestHeadersCount(max_count);
  Http::TestRequestTrailerMapImpl request_trailers;
  for (int i = 0; i < 150; i++) {
    request_trailers.addCopy("trailer", std::string(1, 'a'));
  }

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  codec_client_->sendData(*request_encoder_, 1, false);
  codec_client_->sendTrailers(*request_encoder_, request_trailers);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

// This test uses an Http::HeaderMapImpl instead of an Http::TestHeaderMapImpl to avoid
// time-consuming byte size validations that will cause this test to timeout.
TEST_P(DownstreamProtocolIntegrationTest, ManyRequestHeadersTimeout) {
  // Set timeout for 5 seconds, and ensure that a request with 10k+ headers can be sent.
  testManyRequestHeaders(std::chrono::milliseconds(5000));
}

TEST_P(DownstreamProtocolIntegrationTest, LargeRequestTrailersAccepted) {
  config_helper_.addConfigModifier(setEnableDownstreamTrailersHttp1());
  testLargeRequestTrailers(60, 96);
}

TEST_P(DownstreamProtocolIntegrationTest, LargeRequestTrailersRejected) {
  config_helper_.addConfigModifier(setEnableDownstreamTrailersHttp1());
  testLargeRequestTrailers(66, 60);
}

// This test uses an Http::HeaderMapImpl instead of an Http::TestHeaderMapImpl to avoid
// time-consuming byte size verification that will cause this test to timeout.
TEST_P(DownstreamProtocolIntegrationTest, ManyTrailerHeaders) {
  setMaxRequestHeadersKb(96);
  setMaxRequestHeadersCount(20005);

  config_helper_.addConfigModifier(setEnableDownstreamTrailersHttp1());
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        hcm.mutable_max_request_headers_kb()->set_value(upstreamConfig().max_request_headers_kb_);
        hcm.mutable_common_http_protocol_options()->mutable_max_headers_count()->set_value(
            upstreamConfig().max_request_headers_count_);
      });

  auto request_trailers = Http::RequestTrailerMapImpl::create();
  for (int i = 0; i < 20000; i++) {
    request_trailers->addCopy(Http::LowerCaseString(std::to_string(i)), "");
  }

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":scheme", "http"},
                                                                 {":authority", "sni.lyft.com"}});
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  codec_client_->sendTrailers(*request_encoder_, *request_trailers);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

// Regression tests for CVE-2019-18801. We only validate the behavior of large
// :method request headers, since the case of other large headers is
// covered in the various testLargeRequest-based integration tests here.
//
// The table below describes the expected behaviors (in addition we should never
// see an ASSERT or ASAN failure trigger).
//
// Downstream    Upstream   Behavior expected
// ------------------------------------------
// H1            H1         Envoy will reject (HTTP/1 codec behavior)
// H1            H2         Envoy will reject (HTTP/1 codec behavior)
// H2, H3        H1         Envoy will forward but backend will reject (HTTP/1
//                          codec behavior)
// H2, H3        H2         Success
TEST_P(ProtocolIntegrationTest, LargeRequestMethod) {
  // There will be no upstream connections for HTTP/1 downstream, we need to
  // test the full mesh regardless.
  testing_upstream_intentionally_ = true;
  const std::string long_method = std::string(48 * 1024, 'a');
  const Http::TestRequestHeaderMapImpl request_headers{{":method", long_method},
                                                       {":path", "/test/long/url"},
                                                       {":scheme", "http"},
                                                       {":authority", "sni.lyft.com"}};

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    auto encoder_decoder = codec_client_->startRequest(request_headers);
    request_encoder_ = &encoder_decoder.first;
    auto response = std::move(encoder_decoder.second);
    ASSERT_TRUE(codec_client_->waitForDisconnect());
    EXPECT_TRUE(response->complete());
    EXPECT_EQ("400", response->headers().getStatusValue());
  } else {
    ASSERT(downstreamProtocol() >= Http::CodecType::HTTP2);
    if (upstreamProtocol() == Http::CodecType::HTTP1) {
      auto response = codec_client_->makeHeaderOnlyRequest(request_headers);
      ASSERT_TRUE(
          fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
      ASSERT_TRUE(response->waitForEndStream());
      EXPECT_TRUE(response->complete());
      EXPECT_EQ("400", response->headers().getStatusValue());
    } else {
      ASSERT(upstreamProtocol() >= Http::CodecType::HTTP2);
      auto response =
          sendRequestAndWaitForResponse(request_headers, 0, default_response_headers_, 0);
      EXPECT_TRUE(response->complete());
    }
  }
}

// Tests StopAllIterationAndBuffer. Verifies decode-headers-return-stop-all-filter calls decodeData
// once after iteration is resumed.
TEST_P(DownstreamProtocolIntegrationTest, TestDecodeHeadersReturnsStopAll) {
  config_helper_.prependFilter(R"EOF(
name: call-decodedata-once-filter
)EOF");
  config_helper_.prependFilter(R"EOF(
name: decode-headers-return-stop-all-filter
)EOF");
  config_helper_.prependFilter(R"EOF(
name: passthrough-filter
)EOF");

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Sends a request with headers and data.
  changeHeadersForStopAllTests(default_request_headers_, false);
  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  for (int i = 0; i < count_ - 1; i++) {
    codec_client_->sendData(*request_encoder_, size_, false);
  }
  // Sleeps for 1s in order to be consistent with testDecodeHeadersReturnsStopAllWatermark.
  absl::SleepFor(absl::Seconds(1));
  codec_client_->sendData(*request_encoder_, size_, true);
  waitForNextUpstreamRequest();

  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ(count_ * size_ + added_decoded_data_size_, upstream_request_->bodyLength());
  EXPECT_EQ(true, upstream_request_->complete());

  // Sends a request with headers, data, and trailers.
  auto encoder_decoder_2 = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder_2.first;
  response = std::move(encoder_decoder_2.second);
  for (int i = 0; i < count_; i++) {
    codec_client_->sendData(*request_encoder_, size_, false);
  }
  Http::TestRequestTrailerMapImpl request_trailers{{"trailer", "trailer"}};
  codec_client_->sendTrailers(*request_encoder_, request_trailers);
  waitForNextUpstreamRequest();

  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());
  verifyUpStreamRequestAfterStopAllFilter();
}

// Tests StopAllIterationAndWatermark. decode-headers-return-stop-all-watermark-filter sets buffer
// limit to 100. Verifies data pause when limit is reached, and resume after iteration continues.
TEST_P(DownstreamProtocolIntegrationTest, TestDecodeHeadersReturnsStopAllWatermark) {
  config_helper_.prependFilter(R"EOF(
name: decode-headers-return-stop-all-filter
)EOF");
  config_helper_.prependFilter(R"EOF(
name: passthrough-filter
)EOF");

  // Sets initial stream window to min value to make the client sensitive to a low watermark.
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        hcm.mutable_http2_protocol_options()->mutable_initial_stream_window_size()->set_value(
            ::Envoy::Http2::Utility::OptionsLimits::MIN_INITIAL_STREAM_WINDOW_SIZE);
      });

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Sends a request with headers and data.
  changeHeadersForStopAllTests(default_request_headers_, true);
  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  for (int i = 0; i < count_ - 1; i++) {
    codec_client_->sendData(*request_encoder_, size_, false);
  }
  // Gives buffer 1s to react to buffer limit.
  absl::SleepFor(absl::Seconds(1));
  codec_client_->sendData(*request_encoder_, size_, true);
  waitForNextUpstreamRequest();

  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ(count_ * size_ + added_decoded_data_size_, upstream_request_->bodyLength());
  EXPECT_EQ(true, upstream_request_->complete());

  // Sends a request with headers, data, and trailers.
  auto encoder_decoder_2 = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder_2.first;
  response = std::move(encoder_decoder_2.second);
  for (int i = 0; i < count_ - 1; i++) {
    codec_client_->sendData(*request_encoder_, size_, false);
  }
  // Gives buffer 1s to react to buffer limit.
  absl::SleepFor(absl::Seconds(1));
  codec_client_->sendData(*request_encoder_, size_, false);
  Http::TestRequestTrailerMapImpl request_trailers{{"trailer", "trailer"}};
  codec_client_->sendTrailers(*request_encoder_, request_trailers);
  waitForNextUpstreamRequest();

  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());
  verifyUpStreamRequestAfterStopAllFilter();
}

// Test two filters that return StopAllIterationAndBuffer back-to-back.
TEST_P(DownstreamProtocolIntegrationTest, TestTwoFiltersDecodeHeadersReturnsStopAll) {
  config_helper_.prependFilter(R"EOF(
name: decode-headers-return-stop-all-filter
)EOF");
  config_helper_.prependFilter(R"EOF(
name: decode-headers-return-stop-all-filter
)EOF");
  config_helper_.prependFilter(R"EOF(
name: passthrough-filter
)EOF");

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Sends a request with headers and data.
  changeHeadersForStopAllTests(default_request_headers_, false);
  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  for (int i = 0; i < count_ - 1; i++) {
    codec_client_->sendData(*request_encoder_, size_, false);
  }
  codec_client_->sendData(*request_encoder_, size_, true);
  waitForNextUpstreamRequest();

  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ(count_ * size_ + added_decoded_data_size_, upstream_request_->bodyLength());
  EXPECT_EQ(true, upstream_request_->complete());

  // Sends a request with headers, data, and trailers.
  auto encoder_decoder_2 = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder_2.first;
  response = std::move(encoder_decoder_2.second);
  for (int i = 0; i < count_; i++) {
    codec_client_->sendData(*request_encoder_, size_, false);
  }
  Http::TestRequestTrailerMapImpl request_trailers{{"trailer", "trailer"}};
  codec_client_->sendTrailers(*request_encoder_, request_trailers);
  waitForNextUpstreamRequest();

  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());
  verifyUpStreamRequestAfterStopAllFilter();
}

// Tests encodeHeaders() returns StopAllIterationAndBuffer.
TEST_P(DownstreamProtocolIntegrationTest, TestEncodeHeadersReturnsStopAll) {
  config_helper_.prependFilter(R"EOF(
name: encode-headers-return-stop-all-filter
)EOF");
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void { hcm.mutable_http2_protocol_options()->set_allow_metadata(true); });

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Upstream responds with headers, data and trailers.
  auto response = codec_client_->makeRequestWithBody(default_request_headers_, 10);
  waitForNextUpstreamRequest();

  changeHeadersForStopAllTests(default_response_headers_, false);
  upstream_request_->encodeHeaders(default_response_headers_, false);
  for (int i = 0; i < count_ - 1; i++) {
    upstream_request_->encodeData(size_, false);
  }
  // Sleeps for 1s in order to be consistent with testEncodeHeadersReturnsStopAllWatermark.
  absl::SleepFor(absl::Seconds(1));
  upstream_request_->encodeData(size_, false);
  Http::TestResponseTrailerMapImpl response_trailers{{"response", "trailer"}};
  upstream_request_->encodeTrailers(response_trailers);

  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  // Data is added in encodeData for all protocols, and encodeTrailers for HTTP/2 and above.
  int times_added = upstreamProtocol() == Http::CodecType::HTTP1 ? 1 : 2;
  EXPECT_EQ(count_ * size_ + added_decoded_data_size_ * times_added, response->body().size());
}

// Tests encodeHeaders() returns StopAllIterationAndWatermark.
TEST_P(DownstreamProtocolIntegrationTest, TestEncodeHeadersReturnsStopAllWatermark) {
  config_helper_.prependFilter(R"EOF(
name: encode-headers-return-stop-all-filter
)EOF");
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void { hcm.mutable_http2_protocol_options()->set_allow_metadata(true); });

  // Sets initial stream window to min value to make the upstream sensitive to a low watermark.
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        hcm.mutable_http2_protocol_options()->mutable_initial_stream_window_size()->set_value(
            ::Envoy::Http2::Utility::OptionsLimits::MIN_INITIAL_STREAM_WINDOW_SIZE);
      });

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Upstream responds with headers, data and trailers.
  auto response = codec_client_->makeRequestWithBody(default_request_headers_, 10);
  waitForNextUpstreamRequest();

  changeHeadersForStopAllTests(default_response_headers_, true);
  upstream_request_->encodeHeaders(default_response_headers_, false);
  for (int i = 0; i < count_ - 1; i++) {
    upstream_request_->encodeData(size_, false);
  }
  // Gives buffer 1s to react to buffer limit.
  absl::SleepFor(absl::Seconds(1));
  upstream_request_->encodeData(size_, false);
  Http::TestResponseTrailerMapImpl response_trailers{{"response", "trailer"}};
  upstream_request_->encodeTrailers(response_trailers);

  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  // Data is added in encodeData for all protocols, and encodeTrailers for HTTP/2 and above.
  int times_added = upstreamProtocol() == Http::CodecType::HTTP1 ? 1 : 2;
  EXPECT_EQ(count_ * size_ + added_decoded_data_size_ * times_added, response->body().size());
}

// Per https://github.com/envoyproxy/envoy/issues/7488 make sure we don't
// combine set-cookie headers
TEST_P(ProtocolIntegrationTest, MultipleCookiesAndSetCookies) {
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"},  {":path", "/dynamo/url"},
      {":scheme", "http"}, {":authority", "sni.lyft.com"},
      {"cookie", "a=b"},   {"cookie", "c=d"}};
  Http::TestResponseHeaderMapImpl response_headers{
      {":status", "200"}, {"set-cookie", "foo"}, {"set-cookie", "bar"}};

  auto response = sendRequestAndWaitForResponse(request_headers, 0, response_headers, 0);
  if (downstreamProtocol() == Http::CodecClient::Type::HTTP3) {
    EXPECT_EQ(upstream_request_->headers().get(Http::Headers::get().Cookie)[0]->value(),
              "a=b; c=d");
  }

  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());

  const auto out = response->headers().get(Http::LowerCaseString("set-cookie"));
  ASSERT_EQ(out.size(), 2);
  ASSERT_EQ(out[0]->value().getStringView(), "foo");
  ASSERT_EQ(out[1]->value().getStringView(), "bar");
}

// Test that delay closed connections are eventually force closed when the timeout triggers.
TEST_P(DownstreamProtocolIntegrationTest, TestDelayedConnectionTeardownTimeoutTrigger) {
  config_helper_.prependFilter("{ name: encoder-decoder-buffer-filter }");
  config_helper_.setBufferLimits(1024, 1024);
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             hcm) {
        // 200ms.
        hcm.mutable_delayed_close_timeout()->set_nanos(200000000);
        hcm.mutable_drain_timeout()->set_seconds(1);
        hcm.mutable_common_http_protocol_options()->mutable_idle_timeout()->set_seconds(1);
      });

  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":scheme", "http"},
                                                                 {":authority", "sni.lyft.com"}});
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  codec_client_->sendData(*request_encoder_, 1024 * 65, false);

  ASSERT_TRUE(response->waitForEndStream());
  // The delayed close timeout should trigger since client is not closing the connection.
  EXPECT_TRUE(codec_client_->waitForDisconnect(std::chrono::milliseconds(5000)));
  EXPECT_EQ(codec_client_->lastConnectionEvent(), Network::ConnectionEvent::RemoteClose);
  EXPECT_EQ(test_server_->counter("http.config_test.downstream_cx_delayed_close_timeout")->value(),
            1);
}

// Resets the downstream stream immediately and verifies that we clean up everything.
TEST_P(ProtocolIntegrationTest, TestDownstreamResetIdleTimeout) {
  useAccessLog("%RESPONSE_FLAGS% %RESPONSE_CODE_DETAILS%");
  config_helper_.setDownstreamHttpIdleTimeout(std::chrono::milliseconds(100));

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);

  EXPECT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));

  EXPECT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));

  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    codec_client_->close();
  } else {
    codec_client_->sendReset(encoder_decoder.first);
  }

  if (upstreamProtocol() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
    ASSERT_TRUE(fake_upstream_connection_->close());
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
  }

  ASSERT_TRUE(codec_client_->waitForDisconnect());
  EXPECT_THAT(waitForAccessLog(access_log_name_), Not(HasSubstr("DPE")));
}

// Test connection is closed after single request processed.
TEST_P(ProtocolIntegrationTest, ConnDurationTimeoutBasic) {
  config_helper_.setDownstreamMaxConnectionDuration(std::chrono::milliseconds(500));
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             hcm) { hcm.mutable_drain_timeout()->set_seconds(1); });
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeRequestWithBody(default_request_headers_, 1024);
  waitForNextUpstreamRequest();

  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(512, true);
  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  test_server_->waitForCounterGe("cluster.cluster_0.upstream_cx_total", 1);
  test_server_->waitForCounterGe("cluster.cluster_0.upstream_rq_200", 1);

  ASSERT_TRUE(codec_client_->waitForDisconnect(std::chrono::milliseconds(10000)));
  test_server_->waitForCounterGe("http.config_test.downstream_cx_max_duration_reached", 1);
}

// Test inflight request is processed correctly when timeout fires during request processing.
TEST_P(ProtocolIntegrationTest, ConnDurationInflightRequest) {
  config_helper_.setDownstreamMaxConnectionDuration(std::chrono::milliseconds(500));
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             hcm) { hcm.mutable_drain_timeout()->set_seconds(1); });
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeRequestWithBody(default_request_headers_, 1024);
  waitForNextUpstreamRequest();

  // block and wait for counter to increase
  test_server_->waitForCounterGe("http.config_test.downstream_cx_max_duration_reached", 1);

  // ensure request processed correctly
  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(512, true);
  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  test_server_->waitForCounterGe("cluster.cluster_0.upstream_cx_total", 1);
  test_server_->waitForCounterGe("cluster.cluster_0.upstream_rq_200", 1);

  ASSERT_TRUE(codec_client_->waitForDisconnect(std::chrono::milliseconds(10000)));
}

// Test connection is closed if no http requests were processed
TEST_P(DownstreamProtocolIntegrationTest, ConnDurationTimeoutNoHttpRequest) {
  config_helper_.setDownstreamMaxConnectionDuration(std::chrono::milliseconds(500));
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             hcm) { hcm.mutable_drain_timeout()->set_seconds(1); });
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  ASSERT_TRUE(codec_client_->waitForDisconnect(std::chrono::milliseconds(10000)));
  test_server_->waitForCounterGe("http.config_test.downstream_cx_max_duration_reached", 1);
}

TEST_P(DownstreamProtocolIntegrationTest, TestPreconnect) {
  config_helper_.addConfigModifier([](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
    auto* cluster = bootstrap.mutable_static_resources()->mutable_clusters(0);
    cluster->mutable_preconnect_policy()->mutable_per_upstream_preconnect_ratio()->set_value(1.5);
  });
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response =
      sendRequestAndWaitForResponse(default_request_headers_, 0, default_response_headers_, 0);
  FakeHttpConnectionPtr fake_upstream_connection_two;
  if (upstreamProtocol() == Http::CodecType::HTTP1) {
    // For HTTP/1.1 there should be a preconnected connection.
    ASSERT_TRUE(
        fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_two));
  } else {
    // For HTTP/2, the original connection can accommodate two requests.
    ASSERT_FALSE(fake_upstreams_[0]->waitForHttpConnection(
        *dispatcher_, fake_upstream_connection_two, std::chrono::milliseconds(5)));
  }
}

TEST_P(DownstreamProtocolIntegrationTest, BasicMaxStreamTimeout) {
  config_helper_.setDownstreamMaxStreamDuration(std::chrono::milliseconds(500));
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));
  ASSERT_TRUE(upstream_request_->waitForHeadersComplete());

  test_server_->waitForCounterGe("http.config_test.downstream_rq_max_duration_reached", 1);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("408", response->headers().getStatusValue());
}

// Test that when request timeout and the request is completed, the gateway timeout (504) is
// returned as response code instead of request timeout (408).
TEST_P(ProtocolIntegrationTest, MaxStreamTimeoutWhenRequestIsNotComplete) {
  config_helper_.setDownstreamMaxStreamDuration(std::chrono::milliseconds(500));

  autonomous_upstream_ = false;

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // The request is not header only. Envoy is expecting more data to end the request.
  auto encoder_decoder =
      codec_client_->startRequest(default_request_headers_, /*header_only_request=*/true);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));
  ASSERT_TRUE(upstream_request_->waitForHeadersComplete());

  test_server_->waitForCounterGe("http.config_test.downstream_rq_max_duration_reached", 1);
  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_TRUE(upstream_request_->complete());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("504", response->headers().getStatusValue());
}

// Test case above except disabling runtime guard "override_request_timeout_by_gateway_timeout".
// Verify the old behavior is reverted by disabling the runtime guard.
TEST_P(ProtocolIntegrationTest, MaxStreamTimeoutWhenRequestIsNotCompleteRuntimeDisabled) {
  config_helper_.setDownstreamMaxStreamDuration(std::chrono::milliseconds(500));

  config_helper_.addRuntimeOverride(
      "envoy.reloadable_features.override_request_timeout_by_gateway_timeout", "false");
  autonomous_upstream_ = false;

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // The request is not header only. Envoy is expecting more data to end the request.
  auto encoder_decoder =
      codec_client_->startRequest(default_request_headers_, /*header_only_request=*/true);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  ASSERT_TRUE(fake_upstreams_[0]->waitForHttpConnection(*dispatcher_, fake_upstream_connection_));
  ASSERT_TRUE(fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_));
  ASSERT_TRUE(upstream_request_->waitForHeadersComplete());

  test_server_->waitForCounterGe("http.config_test.downstream_rq_max_duration_reached", 1);
  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_TRUE(upstream_request_->complete());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("408", response->headers().getStatusValue());
}

TEST_P(DownstreamProtocolIntegrationTest, MaxRequestsPerConnectionReached) {
  config_helper_.setDownstreamMaxRequestsPerConnection(2);
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Sending first request and waiting to complete the response.
  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  codec_client_->sendData(*request_encoder_, 1, true);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(default_response_headers_, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ(test_server_->counter("http.config_test.downstream_cx_max_requests_reached")->value(),
            0);

  // Sending second request and waiting to complete the response.
  auto encoder_decoder_2 = codec_client_->startRequest(default_request_headers_);
  request_encoder_ = &encoder_decoder_2.first;
  auto response_2 = std::move(encoder_decoder_2.second);
  codec_client_->sendData(*request_encoder_, 1, true);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(default_response_headers_, true);

  ASSERT_TRUE(response_2->waitForEndStream());
  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response_2->complete());
  EXPECT_EQ(test_server_->counter("http.config_test.downstream_cx_max_requests_reached")->value(),
            1);

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    EXPECT_EQ(nullptr, response->headers().Connection());
    EXPECT_EQ("close", response_2->headers().getConnectionValue());
  } else {
    EXPECT_TRUE(codec_client_->sawGoAway());
  }
  ASSERT_TRUE(codec_client_->waitForDisconnect());
}

// Make sure that invalid authority headers get blocked at or before the HCM.
TEST_P(DownstreamProtocolIntegrationTest, InvalidAuthority) {
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":path", "/test/long/url"},
                                                 {":scheme", "http"},
                                                 {":authority", "ho|st|"}};

  auto response = codec_client_->makeHeaderOnlyRequest(request_headers);
  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    // For HTTP/1 this is handled by the HCM, which sends a full 400 response.
    ASSERT_TRUE(response->waitForEndStream());
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("400", response->headers().getStatusValue());
  } else {
    // For HTTP/2 this is handled by nghttp2 which resets the connection without
    // sending an HTTP response.
    ASSERT_TRUE(codec_client_->waitForDisconnect());
    ASSERT_FALSE(response->complete());
  }
}

TEST_P(DownstreamProtocolIntegrationTest, ConnectIsBlocked) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder = codec_client_->startRequest(Http::TestRequestHeaderMapImpl{
      {":method", "CONNECT"}, {":authority", "sni.lyft.com.com:80"}});
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    // Because CONNECT requests for HTTP/1 do not include a path, they will fail
    // to find a route match and return a 404.
    ASSERT_TRUE(response->waitForEndStream());
    EXPECT_EQ("404", response->headers().getStatusValue());
    EXPECT_TRUE(response->complete());
  } else {
    ASSERT_TRUE(response->waitForReset());
    ASSERT_TRUE(codec_client_->waitForDisconnect());
  }
}

TEST_P(DownstreamProtocolIntegrationTest, ExtendedConnectIsBlocked) {
  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    return;
  }
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder = codec_client_->startRequest(
      Http::TestRequestHeaderMapImpl{{":method", "CONNECT"},
                                     {":protocol", "bytestream"},
                                     {":path", "/"},
                                     {":authority", "sni.lyft.com.com:80"}});
  request_encoder_ = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  ASSERT_TRUE(response->waitForReset());
  ASSERT_TRUE(codec_client_->waitForDisconnect());
}

// Make sure that with override_stream_error_on_invalid_http_message true, CONNECT
// results in stream teardown not connection teardown.
TEST_P(DownstreamProtocolIntegrationTest, ConnectStreamRejection) {
  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    return;
  }
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             hcm) -> void {
        hcm.mutable_http3_protocol_options()
            ->mutable_override_stream_error_on_invalid_http_message()
            ->set_value(true);
        hcm.mutable_http2_protocol_options()
            ->mutable_override_stream_error_on_invalid_http_message()
            ->set_value(true);
      });

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "CONNECT"}, {":authority", "sni.lyft.com"}});

  ASSERT_TRUE(response->waitForReset());
  EXPECT_FALSE(codec_client_->disconnected());
}

// Regression test for https://github.com/envoyproxy/envoy/issues/12131
TEST_P(DownstreamProtocolIntegrationTest, Test100AndDisconnect) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest();
  upstream_request_->encode1xxHeaders(Http::TestResponseHeaderMapImpl{{":status", "100"}});
  ASSERT_TRUE(fake_upstream_connection_->close());

  // Make sure that a disconnect results in valid 5xx response headers even when preceded by a 100.
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("503", response->headers().getStatusValue());
}

TEST_P(DownstreamProtocolIntegrationTest, HeaderNormalizationRejection) {
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             hcm) -> void {
        hcm.set_path_with_escaped_slashes_action(
            envoy::extensions::filters::network::http_connection_manager::v3::
                HttpConnectionManager::REJECT_REQUEST);
      });

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  default_request_headers_.setPath("/test/long%2Furl");
  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);

  EXPECT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("400", response->headers().getStatusValue());
}

// Tests a filter that returns a FilterHeadersStatus::Continue after a local reply without
// processing new metadata generated in decodeHeader
TEST_P(DownstreamProtocolIntegrationTest, LocalReplyWithMetadata) {
  config_helper_.prependFilter(R"EOF(
  name: local-reply-with-metadata-filter
  )EOF");
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  // Send a headers only request.
  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  ASSERT_EQ("200", response->headers().getStatusValue());
}

// Verify that host's trailing dot is removed and matches the domain for routing request.
TEST_P(ProtocolIntegrationTest, EnableStripTrailingHostDot) {
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        hcm.set_strip_trailing_host_dot(true);
        // clear existing domains and add new domain.
        auto* route_config = hcm.mutable_route_config();
        auto* virtual_host = route_config->mutable_virtual_hosts(0);
        virtual_host->clear_domains();
        virtual_host->add_domains("sni.lyft.com");
      });

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com."}});
  waitForNextUpstreamRequest();

  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

// Verify that host's trailing dot is not removed and thus fails to match configured domains for
// routing request.
TEST_P(DownstreamProtocolIntegrationTest, DisableStripTrailingHostDot) {
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        hcm.set_strip_trailing_host_dot(false);
        // clear existing domains and add new domain.
        auto* route_config = hcm.mutable_route_config();
        auto* virtual_host = route_config->mutable_virtual_hosts(0);
        virtual_host->clear_domains();
        virtual_host->add_domains("sni.lyft.com");
      });

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(
      Http::TestRequestHeaderMapImpl{{":method", "GET"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com."}});
  // Expect local reply as request host fails to match configured domains.
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("404", response->headers().getStatusValue());
}

static std::string remove_response_headers_filter = R"EOF(
name: remove-response-headers-filter
)EOF";

TEST_P(ProtocolIntegrationTest, HeadersOnlyRequestWithRemoveResponseHeadersFilter) {
  config_helper_.prependFilter(remove_response_headers_filter);
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  IntegrationStreamDecoderPtr response =
      codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(default_response_headers_, false);
  ASSERT_TRUE(response->waitForEndStream());
  // If a filter chain removes :status from the response headers, then Envoy must reply with
  // BadGateway and must not crash.
  ASSERT_TRUE(codec_client_->connected());
  EXPECT_EQ("502", response->headers().getStatusValue());
  EXPECT_THAT(response->body(), HasSubstr("missing required header: :status"));
}

TEST_P(ProtocolIntegrationTest, RemoveResponseHeadersFilter) {
  config_helper_.prependFilter(remove_response_headers_filter);
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  IntegrationStreamDecoderPtr response =
      codec_client_->makeRequestWithBody(default_request_headers_, 10);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(default_response_headers_, false);
  ASSERT_TRUE(response->waitForEndStream());
  // If a filter chain removes :status from the response headers, then Envoy must reply with
  // BadGateway and not crash.
  ASSERT_TRUE(codec_client_->connected());
  EXPECT_EQ("502", response->headers().getStatusValue());
  EXPECT_THAT(response->body(), HasSubstr("missing required header: :status"));
}

TEST_P(ProtocolIntegrationTest, ReqRespSizeStats) {
  config_helper_.addConfigModifier([&](envoy::config::bootstrap::v3::Bootstrap& bootstrap) -> void {
    RELEASE_ASSERT(bootstrap.mutable_static_resources()->clusters_size() == 1, "");
    auto& cluster = *bootstrap.mutable_static_resources()->mutable_clusters(0);
    cluster.mutable_track_cluster_stats()->set_request_response_sizes(true);
  });
  initialize();

  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));
  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":path", "/found"}, {":scheme", "http"}, {":authority", "foo.lyft.com"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  auto response = sendRequestAndWaitForResponse(request_headers, 0, response_headers, 0, 0,
                                                TestUtility::DefaultTimeout);
  EXPECT_TRUE(upstream_request_->complete());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());

  test_server_->waitUntilHistogramHasSamples("cluster.cluster_0.upstream_rq_headers_size");
  test_server_->waitUntilHistogramHasSamples("cluster.cluster_0.upstream_rs_headers_size");
}

// Verify that when a filter encodeHeaders callback overflows response buffer in filter manager the
// filter chain is aborted and 500 is sent to the client.
TEST_P(ProtocolIntegrationTest, OverflowEncoderBufferFromEncodeHeaders) {
  config_helper_.setBufferLimits(64 * 1024, 64 * 1024);
  config_helper_.prependFilter(R"EOF(
  name: add-body-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.AddBodyFilterConfig
      where_to_add_body: ENCODE_HEADERS
      body_size: 70000
  )EOF");
  config_helper_.prependFilter(R"EOF(
  name: crash-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.CrashFilterConfig
      crash_in_encode_headers: false
      crash_in_encode_data: false
  )EOF");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  auto response = sendRequestAndWaitForResponse(default_request_headers_, 0, response_headers, 10,
                                                0, TestUtility::DefaultTimeout);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("500", response->headers().getStatusValue());
}

// Verify that when a filter encodeData callback overflows response buffer in filter manager the
// filter chain is aborted and 500 is sent to the client in case where upstream response headers
// have not yet been sent.
TEST_P(ProtocolIntegrationTest, OverflowEncoderBufferFromEncodeDataWithResponseHeadersUnsent) {
  config_helper_.setBufferLimits(64 * 1024, 64 * 1024);
  // Buffer filter will stop iteration from encodeHeaders preventing response headers from being
  // sent downstream.
  config_helper_.prependFilter(R"EOF(
  name: encoder-decoder-buffer-filter
  )EOF");
  config_helper_.prependFilter(R"EOF(
  name: crash-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.CrashFilterConfig
      crash_in_encode_headers: true
      crash_in_encode_data: true
  )EOF");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  IntegrationStreamDecoderPtr response =
      codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest(0);
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  upstream_request_->encodeHeaders(response_headers, false);
  // This much data should overflow the 64Kb response buffer.
  upstream_request_->encodeData(16 * 1024, false);
  upstream_request_->encodeData(64 * 1024, false);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("500", response->headers().getStatusValue());
}

// Verify that when a filter encodeData callback overflows response buffer in filter manager the
// filter chain is aborted and stream is reset in case where upstream response headers have already
// been sent.
TEST_P(ProtocolIntegrationTest, OverflowEncoderBufferFromEncodeData) {
  config_helper_.setBufferLimits(64 * 1024, 64 * 1024);
  // Make the add-body-filter stop iteration from encodeData. Headers should be sent to the client.
  config_helper_.prependFilter(R"EOF(
  name: add-body-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.AddBodyFilterConfig
      where_to_add_body: ENCODE_DATA
      where_to_stop_and_buffer: ENCODE_DATA
      body_size: 16384
  )EOF");
  config_helper_.prependFilter(R"EOF(
  name: crash-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.CrashFilterConfig
      crash_in_encode_headers: false
      crash_in_encode_data: true
  )EOF");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  IntegrationStreamDecoderPtr response =
      codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest(0);
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  upstream_request_->encodeHeaders(response_headers, false);
  // This much data should cause the add-body-filter to overflow response buffer
  upstream_request_->encodeData(16 * 1024, false);
  upstream_request_->encodeData(64 * 1024, false);
  ASSERT_TRUE(response->waitForReset());
  EXPECT_FALSE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

// Verify that when a filter decodeHeaders callback overflows request buffer in filter manager the
// filter chain is aborted and 413 is sent to the client.
TEST_P(DownstreamProtocolIntegrationTest, OverflowDecoderBufferFromDecodeHeaders) {
  config_helper_.setBufferLimits(64 * 1024, 64 * 1024);
  config_helper_.prependFilter(R"EOF(
  name: crash-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.CrashFilterConfig
      crash_in_decode_headers: true
  )EOF");
  config_helper_.prependFilter(R"EOF(
  name: add-body-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.AddBodyFilterConfig
      where_to_add_body: DECODE_HEADERS
      body_size: 70000
  )EOF");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("413", response->headers().getStatusValue());
}

// Verify that when a filter decodeData callback overflows request buffer in filter manager the
// filter chain is aborted and 413 is sent to the client.
TEST_P(DownstreamProtocolIntegrationTest, OverflowDecoderBufferFromDecodeData) {
  config_helper_.setBufferLimits(64 * 1024, 64 * 1024);
  config_helper_.prependFilter(R"EOF(
  name: crash-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.CrashFilterConfig
      crash_in_decode_headers: true
      crash_in_decode_data: true
  )EOF");
  // Buffer filter causes filter manager to buffer data
  config_helper_.prependFilter(R"EOF(
  name: encoder-decoder-buffer-filter
  )EOF");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":scheme", "http"},
                                                                 {":path", "/test/long/url"},
                                                                 {":authority", "sni.lyft.com"}});
  auto request_encoder = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  // This much data should overflow request buffer in filter manager
  codec_client_->sendData(*request_encoder, 16 * 1024, false);
  codec_client_->sendData(*request_encoder, 64 * 1024, false);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("413", response->headers().getStatusValue());
}

// Verify that when a filter decodeData callback overflows request buffer in filter manager the
// filter chain is aborted and 413 is sent to the client. In this test the overflow occurs after
// filter chain iteration was restarted. It is very similar to the test case above but some filter
// manager's internal state is slightly different.
TEST_P(DownstreamProtocolIntegrationTest, OverflowDecoderBufferFromDecodeDataContinueIteration) {
  config_helper_.setBufferLimits(64 * 1024, 64 * 1024);
  config_helper_.prependFilter(R"EOF(
  name: crash-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.CrashFilterConfig
      crash_in_decode_headers: false
      crash_in_decode_data: true
  )EOF");
  config_helper_.prependFilter(R"EOF(
  name: add-body-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.AddBodyFilterConfig
      where_to_add_body: DECODE_DATA
      body_size: 70000
  )EOF");
  config_helper_.prependFilter(R"EOF(
  name: encoder-decoder-buffer-filter
  )EOF");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":scheme", "http"},
                                                                 {":path", "/test/long/url"},
                                                                 {":authority", "sni.lyft.com"}});
  auto request_encoder = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  // This should cause some data to be buffered without overflowing request buffer.
  codec_client_->sendData(*request_encoder, 16 * 1024, false);
  // The buffer filter will resume filter chain iteration and the next add-body-filter filter
  // will overflow the request buffer.
  codec_client_->sendData(*request_encoder, 16 * 1024, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("413", response->headers().getStatusValue());
}

// Adding data in decodeTrailers without any data in the filter manager's request buffer should work
// as it will overflow the pending_recv_data_ which will cause downstream window updates to stop.
TEST_P(DownstreamProtocolIntegrationTest,
       OverflowDecoderBufferFromDecodeTrailersWithContinuedIteration) {
  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    return;
  }
  config_helper_.setBufferLimits(64 * 1024, 64 * 1024);
  config_helper_.prependFilter(R"EOF(
  name: add-body-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.AddBodyFilterConfig
      where_to_add_body: DECODE_TRAILERS
      body_size: 70000
  )EOF");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":scheme", "http"},
                                                                 {":path", "/test/long/url"},
                                                                 {":authority", "sni.lyft.com"}});
  auto request_encoder = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  codec_client_->sendData(*request_encoder, 1024, false);
  codec_client_->sendData(*request_encoder, 1024, false);

  codec_client_->sendTrailers(*request_encoder,
                              Http::TestRequestTrailerMapImpl{{"some", "trailer"}});

  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);
  ASSERT_TRUE(response->waitForEndStream());

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

// Adding data in decodeTrailers with some data in the filter manager's request buffer should case
// 413 as it will overflow the request buffer in filter manager.
TEST_P(DownstreamProtocolIntegrationTest, OverflowDecoderBufferFromDecodeTrailers) {
  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    return;
  }
  config_helper_.setBufferLimits(64 * 1024, 64 * 1024);
  config_helper_.prependFilter(R"EOF(
  name: crash-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.CrashFilterConfig
      crash_in_decode_headers: false
      crash_in_decode_data: true
      crash_in_decode_trailers: true
  )EOF");
  config_helper_.prependFilter(R"EOF(
  name: add-body-filter
  typed_config:
      "@type": type.googleapis.com/test.integration.filters.AddBodyFilterConfig
      where_to_add_body: DECODE_TRAILERS
      where_to_stop_and_buffer: DECODE_DATA
      body_size: 70000
  )EOF");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":scheme", "http"},
                                                                 {":path", "/test/long/url"},
                                                                 {":authority", "sni.lyft.com"}});
  auto request_encoder = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  codec_client_->sendData(*request_encoder, 1024, false);
  codec_client_->sendData(*request_encoder, 1024, false);

  codec_client_->sendTrailers(*request_encoder,
                              Http::TestRequestTrailerMapImpl{{"some", "trailer"}});

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("413", response->headers().getStatusValue());
}

TEST_P(ProtocolIntegrationTest, ResetLargeResponseUponReceivingHeaders) {
  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    return;
  }
  autonomous_upstream_ = true;
  autonomous_allow_incomplete_streams_ = true;
  initialize();

  envoy::config::core::v3::Http2ProtocolOptions http2_options =
      ::Envoy::Http2::Utility::initializeAndValidateOptions(
          envoy::config::core::v3::Http2ProtocolOptions());
  http2_options.mutable_initial_stream_window_size()->set_value(65535);
  http2_options.mutable_initial_connection_window_size()->set_value(65535);
  codec_client_ = makeRawHttpConnection(makeClientConnection(lookupPort("http")), http2_options);

  // The response is larger than the stream flow control window. So the reset of it will
  // likely be buffered QUIC stream send buffer.
  constexpr uint64_t response_size = 100 * 1024;
  auto encoder_decoder = codec_client_->startRequest(
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"content-length", "10"},
                                     {"response_size_bytes", absl::StrCat(response_size)}});
  auto& encoder = encoder_decoder.first;
  std::string data(10, 'a');
  codec_client_->sendData(encoder, data, true);

  auto response = std::move(encoder_decoder.second);
  response->waitForHeaders();
  // Reset stream while the quic server stream might have FIN buffered in its send buffer.
  codec_client_->sendReset(encoder);
  codec_client_->close();
}

TEST_P(ProtocolIntegrationTest, HeaderOnlyBytesCountUpstream) {
  if (downstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  useAccessLog("%UPSTREAM_WIRE_BYTES_SENT% %UPSTREAM_WIRE_BYTES_RECEIVED% "
               "%UPSTREAM_HEADER_BYTES_SENT% %UPSTREAM_HEADER_BYTES_RECEIVED%");
  testRouterRequestAndResponseWithBody(0, 0, false);
  const size_t wire_bytes_received =
      (GetParam().http2_implementation == Http2Impl::Oghttp2) ? 10 : 13;
  expectUpstreamBytesSentAndReceived(
      BytesCountExpectation(167, 38, 136, 18),
      BytesCountExpectation(120, wire_bytes_received, 120, wire_bytes_received),
      BytesCountExpectation(116, 5, 116, 3));
}

TEST_P(ProtocolIntegrationTest, HeaderOnlyBytesCountDownstream) {
  if (upstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  useAccessLog("%DOWNSTREAM_WIRE_BYTES_SENT% %DOWNSTREAM_WIRE_BYTES_RECEIVED% "
               "%DOWNSTREAM_HEADER_BYTES_SENT% %DOWNSTREAM_HEADER_BYTES_RECEIVED%");
  testRouterRequestAndResponseWithBody(0, 0, false);
  expectDownstreamBytesSentAndReceived(BytesCountExpectation(124, 51, 105, 19),
                                       BytesCountExpectation(68, 34, 68, 34),
                                       BytesCountExpectation(8, 10, 8, 6));
}

TEST_P(ProtocolIntegrationTest, HeaderAndBodyWireBytesCountUpstream) {
  // we only care about upstream protocol.
  if (downstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  useAccessLog("%UPSTREAM_WIRE_BYTES_SENT% %UPSTREAM_WIRE_BYTES_RECEIVED% "
               "%UPSTREAM_HEADER_BYTES_SENT% %UPSTREAM_HEADER_BYTES_RECEIVED%");
  testRouterRequestAndResponseWithBody(100, 100, false);
  const size_t header_bytes_received =
      (GetParam().http2_implementation == Http2Impl::Oghttp2) ? 10 : 13;
  expectUpstreamBytesSentAndReceived(BytesCountExpectation(306, 158, 164, 27),
                                     BytesCountExpectation(229, 122, 120, header_bytes_received),
                                     BytesCountExpectation(219, 109, 116, 3));
}

TEST_P(ProtocolIntegrationTest, HeaderAndBodyWireBytesCountDownstream) {
  // we only care about upstream protocol.
  if (upstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  useAccessLog("%DOWNSTREAM_WIRE_BYTES_SENT% %DOWNSTREAM_WIRE_BYTES_RECEIVED% "
               "%DOWNSTREAM_HEADER_BYTES_SENT% %DOWNSTREAM_HEADER_BYTES_RECEIVED%");
  testRouterRequestAndResponseWithBody(100, 100, false);
  expectDownstreamBytesSentAndReceived(BytesCountExpectation(244, 190, 114, 46),
                                       BytesCountExpectation(177, 173, 68, 34),
                                       BytesCountExpectation(111, 113, 8, 6));
}

TEST_P(ProtocolIntegrationTest, HeaderAndBodyWireBytesCountReuseDownstream) {
  // We only care about the downstream protocol.
  if (upstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }

  useAccessLog("%DOWNSTREAM_WIRE_BYTES_SENT% %DOWNSTREAM_WIRE_BYTES_RECEIVED% "
               "%DOWNSTREAM_HEADER_BYTES_SENT% %DOWNSTREAM_HEADER_BYTES_RECEIVED%");

  initialize();
  codec_client_ = makeHttpConnection(makeClientConnection(lookupPort("http")));
  const int request_size = 100;
  const int response_size = 100;

  // Send first request on the connection
  auto response_one = sendRequestAndWaitForResponse(default_request_headers_, request_size,
                                                    default_response_headers_, response_size, 0);
  checkSimpleRequestSuccess(request_size, response_size, response_one.get());
  expectDownstreamBytesSentAndReceived(BytesCountExpectation(244, 190, 114, 46),
                                       BytesCountExpectation(177, 137, 68, 34),
                                       BytesCountExpectation(111, 137, 8, 6), 0);

  // Reuse connection, send the second request on the connection.
  auto response_two = sendRequestAndWaitForResponse(default_request_headers_, request_size,
                                                    default_response_headers_, response_size, 0);
  checkSimpleRequestSuccess(request_size, response_size, response_two.get());
  expectDownstreamBytesSentAndReceived(BytesCountExpectation(244, 190, 114, 46),
                                       BytesCountExpectation(148, 137, 15, 27),
                                       BytesCountExpectation(111, 137, 8, 6), 1);
}

TEST_P(ProtocolIntegrationTest, HeaderAndBodyWireBytesCountReuseUpstream) {
  // We only care about the upstream protocol.
  if (downstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }

  useAccessLog("%UPSTREAM_WIRE_BYTES_SENT% %UPSTREAM_WIRE_BYTES_RECEIVED% "
               "%UPSTREAM_HEADER_BYTES_SENT% %UPSTREAM_HEADER_BYTES_RECEIVED%");

  initialize();
  codec_client_ = makeHttpConnection(makeClientConnection(lookupPort("http")));
  auto second_client = makeHttpConnection(makeClientConnection(lookupPort("http")));
  const int request_size = 100;
  const int response_size = 100;

  // Send to the same upstream from the two clients.
  auto response_one = sendRequestAndWaitForResponse(default_request_headers_, request_size,
                                                    default_response_headers_, response_size, 0);
  const size_t http2_header_bytes_received =
      (GetParam().http2_implementation == Http2Impl::Oghttp2) ? 10 : 13;
  expectUpstreamBytesSentAndReceived(
      BytesCountExpectation(306, 158, 164, 27),
      BytesCountExpectation(223, 122, 120, http2_header_bytes_received),
      BytesCountExpectation(223, 108, 114, 3), 0);

  // Swap clients so the other connection is used to send the request.
  std::swap(codec_client_, second_client);
  auto response_two = sendRequestAndWaitForResponse(default_request_headers_, request_size,
                                                    default_response_headers_, response_size, 0);

  const size_t http2_header_bytes_sent =
      (GetParam().http2_implementation == Http2Impl::Oghttp2) ? 54 : 58;
  expectUpstreamBytesSentAndReceived(BytesCountExpectation(306, 158, 164, 27),
                                     BytesCountExpectation(167, 119, http2_header_bytes_sent, 10),
                                     BytesCountExpectation(114, 108, 11, 3), 1);
  second_client->close();
}

TEST_P(ProtocolIntegrationTest, TrailersWireBytesCountUpstream) {
  // we only care about upstream protocol.
  if (downstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  useAccessLog("%UPSTREAM_WIRE_BYTES_SENT% %UPSTREAM_WIRE_BYTES_RECEIVED% "
               "%UPSTREAM_HEADER_BYTES_SENT% %UPSTREAM_HEADER_BYTES_RECEIVED%");
  config_helper_.addConfigModifier(setEnableDownstreamTrailersHttp1());
  config_helper_.addConfigModifier(setEnableUpstreamTrailersHttp1());

  testTrailers(10, 20, true, true);

  const size_t http2_trailer_bytes_received =
      (GetParam().http2_implementation == Http2Impl::Oghttp2) ? 49 : 52;
  expectUpstreamBytesSentAndReceived(
      BytesCountExpectation(256, 120, 204, 67),
      BytesCountExpectation(172, 81, 154, http2_trailer_bytes_received),
      BytesCountExpectation(154, 33, 142, 7));
}

TEST_P(ProtocolIntegrationTest, TrailersWireBytesCountDownstream) {
  // we only care about upstream protocol.
  if (upstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  useAccessLog("%DOWNSTREAM_WIRE_BYTES_SENT% %DOWNSTREAM_WIRE_BYTES_RECEIVED% "
               "%DOWNSTREAM_HEADER_BYTES_SENT% %DOWNSTREAM_HEADER_BYTES_RECEIVED%");
  config_helper_.addConfigModifier(setEnableDownstreamTrailersHttp1());
  config_helper_.addConfigModifier(setEnableUpstreamTrailersHttp1());

  testTrailers(10, 20, true, true);

  expectDownstreamBytesSentAndReceived(BytesCountExpectation(206, 140, 156, 84),
                                       BytesCountExpectation(136, 86, 107, 67),
                                       BytesCountExpectation(36, 26, 14, 10));
}

TEST_P(ProtocolIntegrationTest, DownstreamDisconnectBeforeRequestCompleteWireBytesCountUpstream) {
  // we only care about upstream protocol.
  if (downstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  useAccessLog("%UPSTREAM_WIRE_BYTES_SENT% %UPSTREAM_WIRE_BYTES_RECEIVED% "
               "%UPSTREAM_HEADER_BYTES_SENT% %UPSTREAM_HEADER_BYTES_RECEIVED%");

  testRouterDownstreamDisconnectBeforeRequestComplete(nullptr);

  expectUpstreamBytesSentAndReceived(BytesCountExpectation(195, 0, 164, 0),
                                     BytesCountExpectation(120, 0, 120, 0),
                                     BytesCountExpectation(120, 0, 120, 0));
}

TEST_P(ProtocolIntegrationTest, DownstreamDisconnectBeforeRequestCompleteWireBytesCountDownstream) {
  // we only care about upstream protocol.
  if (upstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  useAccessLog("%DOWNSTREAM_WIRE_BYTES_SENT% %DOWNSTREAM_WIRE_BYTES_RECEIVED% "
               "%DOWNSTREAM_HEADER_BYTES_SENT% %DOWNSTREAM_HEADER_BYTES_RECEIVED%");

  testRouterDownstreamDisconnectBeforeRequestComplete(nullptr);

  expectDownstreamBytesSentAndReceived(BytesCountExpectation(0, 79, 0, 46),
                                       BytesCountExpectation(0, 34, 0, 34),
                                       BytesCountExpectation(0, 8, 0, 6));
}

TEST_P(ProtocolIntegrationTest, UpstreamDisconnectBeforeRequestCompleteWireBytesCountUpstream) {
  // we only care about upstream protocol.
  if (downstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  useAccessLog("%UPSTREAM_WIRE_BYTES_SENT% %UPSTREAM_WIRE_BYTES_RECEIVED% "
               "%UPSTREAM_HEADER_BYTES_SENT% %UPSTREAM_HEADER_BYTES_RECEIVED%");

  testRouterUpstreamDisconnectBeforeRequestComplete();

  expectUpstreamBytesSentAndReceived(BytesCountExpectation(195, 0, 164, 0),
                                     BytesCountExpectation(120, 0, 120, 0),
                                     BytesCountExpectation(120, 0, 120, 0));
}

TEST_P(ProtocolIntegrationTest, UpstreamDisconnectBeforeResponseCompleteWireBytesCountUpstream) {
  // we only care about upstream protocol.
  if (downstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  useAccessLog("%UPSTREAM_WIRE_BYTES_SENT% %UPSTREAM_WIRE_BYTES_RECEIVED% "
               "%UPSTREAM_HEADER_BYTES_SENT% %UPSTREAM_HEADER_BYTES_RECEIVED%");

  testRouterUpstreamDisconnectBeforeResponseComplete();

  const size_t http2_header_bytes_received =
      (GetParam().http2_implementation == Http2Impl::Oghttp2) ? 10 : 13;
  expectUpstreamBytesSentAndReceived(
      BytesCountExpectation(167, 47, 136, 27),
      BytesCountExpectation(120, http2_header_bytes_received, 120, http2_header_bytes_received),
      BytesCountExpectation(113, 5, 113, 3));
}

TEST_P(DownstreamProtocolIntegrationTest, BadRequest) {
  config_helper_.disableDelayClose();
  // we only care about upstream protocol.
  if (downstreamProtocol() != Http::CodecType::HTTP1) {
    return;
  }
  useAccessLog("%DOWNSTREAM_WIRE_BYTES_SENT% %DOWNSTREAM_WIRE_BYTES_RECEIVED% "
               "%DOWNSTREAM_HEADER_BYTES_SENT% %DOWNSTREAM_HEADER_BYTES_RECEIVED%");
  initialize();
  std::string response;
  std::string full_request(100, '\r');
  full_request += "GET / HTTP/1.1\r\n path: /test/long/url\r\n"
                  "Host: host\r\ncontent-length: 0\r\n"
                  "transfer-encoding: chunked\r\n\r\n";

  sendRawHttpAndWaitForResponse(lookupPort("http"), full_request.c_str(), &response, false);

  expectUpstreamBytesSentAndReceived(BytesCountExpectation(156, 200, 117, 0),
                                     BytesCountExpectation(113, 13, 113, 0),
                                     BytesCountExpectation(156, 200, 113, 0));
}

TEST_P(DownstreamProtocolIntegrationTest, PathWithFragmentRejectedByDefault) {
  initialize();

  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));
  default_request_headers_.setPath("/some/path#fragment");
  IntegrationStreamDecoderPtr response =
      codec_client_->makeRequestWithBody(default_request_headers_, 10);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("400", response->headers().getStatusValue());
}

TEST_P(ProtocolIntegrationTest, FragmentStrippedFromPathWithOverride) {
  config_helper_.addRuntimeOverride("envoy.reloadable_features.http_reject_path_with_fragment",
                                    "false");
  initialize();

  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));
  default_request_headers_.setPath("/some/path?p1=v1#fragment");
  Http::TestRequestHeaderMapImpl expected_request_headers{default_request_headers_};
  expected_request_headers.setPath("/some/path?p1=v1");
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  auto response = sendRequestAndWaitForResponse(expected_request_headers, 0, response_headers, 0, 0,
                                                TestUtility::DefaultTimeout);
  EXPECT_TRUE(upstream_request_->complete());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().getStatusValue());
}

// Test buffering and then continuing after too many response bytes to buffer.
TEST_P(ProtocolIntegrationTest, BufferContinue) {
  // Bytes sent is configured for http/2 flow control windows.
  if (upstreamProtocol() != Http::CodecType::HTTP2) {
    return;
  }
  config_helper_.addConfigModifier(
      [&](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
              hcm) -> void {
        auto* route_config = hcm.mutable_route_config();
        auto* virtual_host = route_config->mutable_virtual_hosts(0);
        auto* header = virtual_host->mutable_response_headers_to_add()->Add()->mutable_header();
        header->set_key("foo");
        header->set_value("bar");
      });

  useAccessLog();
  config_helper_.addFilter("{ name: buffer-continue-filter }");
  config_helper_.setBufferLimits(1024, 1024);
  initialize();

  // Send the request.
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  Buffer::OwnedImpl data("HTTP body content goes here");
  codec_client_->sendData(*downstream_request, data, true);
  waitForNextUpstreamRequest();

  // Send the response headers.
  upstream_request_->encodeHeaders(default_response_headers_, false);

  // Now send an overly large response body. At some point, too much data will
  // be buffered, the stream will be reset, and the connection will disconnect.
  upstream_request_->encodeData(512, false);
  upstream_request_->encodeData(1024 * 100, false);

  if (upstreamProtocol() == Http::CodecType::HTTP1) {
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
  } else {
    ASSERT_TRUE(upstream_request_->waitForReset());
    ASSERT_TRUE(fake_upstream_connection_->close());
    ASSERT_TRUE(fake_upstream_connection_->waitForDisconnect());
  }

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("500", response->headers().getStatusValue());
}

TEST_P(DownstreamProtocolIntegrationTest, ContentLengthSmallerThanPayload) {
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"content-length", "123"}},
      1024);
  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    waitForNextUpstreamRequest();
    // HTTP/1.x requests get the payload length from Content-Length header. The remaining bytes is
    // parsed as another request.
    EXPECT_EQ(123u, upstream_request_->body().length());
    upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);
    ASSERT_TRUE(response->waitForEndStream());
    EXPECT_EQ("200", response->headers().getStatusValue());
    EXPECT_TRUE(response->complete());
  } else {
    // Inconsistency in content-length header and the actually body length should be treated as a
    // stream error.
    ASSERT_TRUE(response->waitForReset());
    EXPECT_EQ(Http::StreamResetReason::RemoteReset, response->resetReason());
  }
}

TEST_P(DownstreamProtocolIntegrationTest, ContentLengthLargerThanPayload) {
  if (downstreamProtocol() == Http::CodecType::HTTP1) {
    // HTTP/1.x request rely on Content-Length header to determine payload length. So there is no
    // inconsistency but the request will hang there waiting for the rest bytes.
    return;
  }

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                     {":path", "/test/long/url"},
                                     {":scheme", "http"},
                                     {":authority", "sni.lyft.com"},
                                     {"content-length", "1025"}},
      1024);

  // Inconsistency in content-length header and the actually body length should be treated as a
  // stream error.
  ASSERT_TRUE(response->waitForReset());
  EXPECT_EQ(Http::StreamResetReason::RemoteReset, response->resetReason());
}

class NoUdpGso : public Api::OsSysCallsImpl {
public:
  bool supportsUdpGso() const override { return false; }
};

TEST_P(DownstreamProtocolIntegrationTest, HandleDownstreamSocketFail) {
  // Make sure for HTTP/3 Envoy will use sendmsg, so the write_matcher will work.
  NoUdpGso reject_gso_;
  TestThreadsafeSingletonInjector<Api::OsSysCallsImpl> os_calls{&reject_gso_};
  ASSERT(!Api::OsSysCallsSingleton::get().supportsUdpGso());
  SocketInterfaceSwap socket_swap;

  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest();

  // Makes us have Envoy's writes to downstream return EBADF
  Network::IoSocketError* ebadf = Network::IoSocketError::getIoSocketEbadfInstance();
  socket_swap.write_matcher_->setSourcePort(lookupPort("http"));
  socket_swap.write_matcher_->setWriteOverride(ebadf);
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

  if (downstreamProtocol() == Http::CodecType::HTTP3) {
    // For HTTP/3 since the packets are black holed, there is no client side
    // indication of connection close. Wait on Envoy stats instead.
    test_server_->waitForCounterEq("http.config_test.downstream_rq_rx_reset", 1);
    codec_client_->close();
  } else {
    ASSERT_TRUE(codec_client_->waitForDisconnect());
  }
  socket_swap.write_matcher_->setWriteOverride(nullptr);
  // Shut down the server and upstreams before os_calls goes out of scope to avoid syscalls
  // during its removal.
  test_server_.reset();
  cleanupUpstreamAndDownstream();
  fake_upstreams_.clear();
}

TEST_P(ProtocolIntegrationTest, HandleUpstreamSocketFail) {
  SocketInterfaceSwap socket_swap;

  useAccessLog("%RESPONSE_CODE_DETAILS%");
  initialize();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto encoder_decoder = codec_client_->startRequest(default_request_headers_);
  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  // Make sure the headers made it through.
  waitForNextUpstreamConnection(std::vector<uint64_t>{0}, TestUtility::DefaultTimeout,
                                fake_upstream_connection_);
  AssertionResult result =
      fake_upstream_connection_->waitForNewStream(*dispatcher_, upstream_request_);
  RELEASE_ASSERT(result, result.message());

  // Makes us have Envoy's writes to upstream return EBADF
  Network::IoSocketError* ebadf = Network::IoSocketError::getIoSocketEbadfInstance();
  socket_swap.write_matcher_->setDestinationPort(fake_upstreams_[0]->localAddress()->ip()->port());
  socket_swap.write_matcher_->setWriteOverride(ebadf);

  Buffer::OwnedImpl data("HTTP body content goes here");
  codec_client_->sendData(*downstream_request, data, true);

  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_THAT(waitForAccessLog(access_log_name_),
              HasSubstr("upstream_reset_before_response_started{connection_termination}"));
  EXPECT_TRUE(response->complete());
  EXPECT_EQ("503", response->headers().getStatusValue());
  socket_swap.write_matcher_->setWriteOverride(nullptr);
  // Shut down the server before os_calls goes out of scope to avoid syscalls
  // during its removal.
  test_server_.reset();
}

TEST_P(ProtocolIntegrationTest, NoLocalInterfaceNameForUpstreamConnection) {
  config_helper_.prependFilter(R"EOF(
  name: stream-info-to-headers-filter
  )EOF");
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Send a headers only request.
  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest();

  // Make sure that the body was injected to the request.
  EXPECT_TRUE(upstream_request_->complete());

  // Send a headers only response.
  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());

  // Make sure that the local interface name was not populated. This is the runtime default.
  EXPECT_TRUE(response->headers().get(Http::LowerCaseString("local_interface_name")).empty());
}

// WIN32 fails configuration and terminates the server.
#ifndef WIN32
TEST_P(ProtocolIntegrationTest, LocalInterfaceNameForUpstreamConnection) {
  config_helper_.prependFilter(R"EOF(
  name: stream-info-to-headers-filter
  )EOF");

  config_helper_.addConfigModifier([](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
    bootstrap.mutable_static_resources()
        ->mutable_clusters(0)
        ->mutable_upstream_connection_options()
        ->set_set_local_interface_name_on_upstream_connections(true);
  });
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  // Send a headers only request.
  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest();

  // Make sure that the body was injected to the request.
  EXPECT_TRUE(upstream_request_->complete());

  // Send a headers only response.
  upstream_request_->encodeHeaders(default_response_headers_, true);
  ASSERT_TRUE(response->waitForEndStream());

  // Make sure that the local interface name was populated due to runtime override.
  // TODO: h3 upstreams don't have local interface name
  if (GetParam().upstream_protocol == Http::CodecType::HTTP3) {
    EXPECT_TRUE(response->headers().get(Http::LowerCaseString("local_interface_name")).empty());
  } else {
    EXPECT_FALSE(response->headers().get(Http::LowerCaseString("local_interface_name")).empty());
  }
}
#endif

#ifdef NDEBUG
// These tests send invalid request and response header names which violate ASSERT while creating
// such request/response headers. So they can only be run in NDEBUG mode.
TEST_P(DownstreamProtocolIntegrationTest, InvalidReqestHeaderName) {
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto encoder_decoder =
      codec_client_->startRequest(Http::TestRequestHeaderMapImpl{{":method", "POST"},
                                                                 {":path", "/test/long/url"},
                                                                 {":authority", "sni.lyft.com"},
                                                                 {"foo\nname", "foo_value"}});
  auto response = std::move(encoder_decoder.second);

  if (downstream_protocol_ == Http::CodecType::HTTP1) {
    ASSERT_TRUE(codec_client_->waitForDisconnect());
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("400", response->headers().getStatusValue());
    test_server_->waitForCounterGe("http.config_test.downstream_rq_4xx", 1);
  } else {
    ASSERT_TRUE(response->waitForReset());
    EXPECT_EQ(Http::StreamResetReason::ConnectionTermination, response->resetReason());
  }
}

TEST_P(DownstreamProtocolIntegrationTest, InvalidResponseHeaderName) {
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeHeaderOnlyRequest(default_request_headers_);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(
      Http::TestResponseHeaderMapImpl{{":status", "200"}, {"foo\rname", "foo_value"}}, false);

  ASSERT_TRUE(response->waitForEndStream());

  ASSERT_TRUE(response->complete());
  EXPECT_EQ("502", response->headers().getStatusValue());
  test_server_->waitForCounterGe("http.config_test.downstream_rq_5xx", 1);
}
#endif

} // namespace Envoy
