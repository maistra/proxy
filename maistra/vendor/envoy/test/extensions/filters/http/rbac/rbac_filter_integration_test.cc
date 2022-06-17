#include "envoy/extensions/filters/http/rbac/v3/rbac.pb.h"
#include "envoy/extensions/filters/network/http_connection_manager/v3/http_connection_manager.pb.h"

#include "common/protobuf/utility.h"

#include "extensions/filters/http/well_known_names.h"

#include "test/integration/http_protocol_integration.h"

namespace Envoy {
namespace {

const std::string RBAC_CONFIG = R"EOF(
name: rbac
typed_config:
  "@type": type.googleapis.com/envoy.config.filter.http.rbac.v2.RBAC
  rules:
    policies:
      foo:
        permissions:
          - header: { name: ":method", exact_match: "GET" }
        principals:
          - any: true
)EOF";

const std::string RBAC_CONFIG_WITH_PREFIX_MATCH = R"EOF(
name: rbac
typed_config:
  "@type": type.googleapis.com/envoy.config.filter.http.rbac.v2.RBAC
  rules:
    policies:
      foo:
        permissions:
          - header: { name: ":path", prefix_match: "/foo" }
        principals:
          - any: true
)EOF";

const std::string RBAC_CONFIG_WITH_PATH_EXACT_MATCH = R"EOF(
name: rbac
typed_config:
  "@type": type.googleapis.com/envoy.config.filter.http.rbac.v2.RBAC
  rules:
    policies:
      foo:
        permissions:
          - url_path:
              path: { exact: "/allow" }
        principals:
          - any: true
)EOF";

const std::string RBAC_CONFIG_DENY_WITH_PATH_EXACT_MATCH = R"EOF(
name: rbac
typed_config:
  "@type": type.googleapis.com/envoy.extensions.filters.http.rbac.v3.RBAC
  rules:
    action: DENY
    policies:
      foo:
        permissions:
          - url_path:
              path: { exact: "/deny" }
        principals:
          - any: true
)EOF";

const std::string RBAC_CONFIG_WITH_PATH_IGNORE_CASE_MATCH = R"EOF(
name: rbac
typed_config:
  "@type": type.googleapis.com/envoy.config.filter.http.rbac.v2.RBAC
  rules:
    policies:
      foo:
        permissions:
          - url_path:
              path: { exact: "/ignore_case", ignore_case: true }
        principals:
          - any: true
)EOF";

const std::string RBAC_CONFIG_HEADER_MATCH_CONDITION = R"EOF(
name: rbac
typed_config:
  "@type": type.googleapis.com/envoy.extensions.filters.http.rbac.v3.RBAC
  rules:
    policies:
      foo:
        permissions:
          - any: true
        principals:
          - any: true
        condition:
          call_expr:
            function: _==_
            args:
            - select_expr:
                operand:
                  select_expr:
                    operand:
                      ident_expr:
                        name: request
                    field: headers
                field: xxx
            - const_expr:
               string_value: {}
)EOF";

using RBACIntegrationTest = HttpProtocolIntegrationTest;

INSTANTIATE_TEST_SUITE_P(Protocols, RBACIntegrationTest,
                         testing::ValuesIn(HttpProtocolIntegrationTest::getProtocolTestParams()),
                         HttpProtocolIntegrationTest::protocolTestParamsToString);

TEST_P(RBACIntegrationTest, Allowed) {
  config_helper_.addFilter(RBAC_CONFIG);
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "GET"},
          {":path", "/"},
          {":scheme", "http"},
          {":authority", "host"},
          {"x-forwarded-for", "10.0.0.1"},
      },
      1024);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

  response->waitForEndStream();
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
}

TEST_P(RBACIntegrationTest, Denied) {
  config_helper_.addFilter(RBAC_CONFIG);
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "POST"},
          {":path", "/"},
          {":scheme", "http"},
          {":authority", "host"},
          {"x-forwarded-for", "10.0.0.1"},
      },
      1024);
  response->waitForEndStream();
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("403", response->headers().Status()->value().getStringView());
}

TEST_P(RBACIntegrationTest, DeniedWithPrefixRule) {
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             cfg) { cfg.mutable_normalize_path()->set_value(false); });
  config_helper_.addFilter(RBAC_CONFIG_WITH_PREFIX_MATCH);
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "POST"},
          {":path", "/foo/../bar"},
          {":scheme", "http"},
          {":authority", "host"},
          {"x-forwarded-for", "10.0.0.1"},
      },
      1024);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

  response->waitForEndStream();
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
}

TEST_P(RBACIntegrationTest, RbacPrefixRuleUseNormalizePath) {
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             cfg) { cfg.mutable_normalize_path()->set_value(true); });
  config_helper_.addFilter(RBAC_CONFIG_WITH_PREFIX_MATCH);
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "POST"},
          {":path", "/foo/../bar"},
          {":scheme", "http"},
          {":authority", "host"},
          {"x-forwarded-for", "10.0.0.1"},
      },
      1024);

  response->waitForEndStream();
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("403", response->headers().Status()->value().getStringView());
}

TEST_P(RBACIntegrationTest, DeniedHeadReply) {
  config_helper_.addFilter(RBAC_CONFIG);
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "HEAD"},
          {":path", "/"},
          {":scheme", "http"},
          {":authority", "host"},
          {"x-forwarded-for", "10.0.0.1"},
      },
      1024);
  response->waitForEndStream();
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("403", response->headers().Status()->value().getStringView());
  ASSERT_TRUE(response->headers().ContentLength());
  EXPECT_NE("0", response->headers().ContentLength()->value().getStringView());
  EXPECT_THAT(response->body(), ::testing::IsEmpty());
}

TEST_P(RBACIntegrationTest, RouteOverride) {
  config_helper_.addConfigModifier(
      [](envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
             cfg) {
        envoy::extensions::filters::http::rbac::v3::RBACPerRoute per_route_config;
        TestUtility::loadFromJson("{}", per_route_config);

        auto* config = cfg.mutable_route_config()
                           ->mutable_virtual_hosts()
                           ->Mutable(0)
                           ->mutable_typed_per_filter_config();

        (*config)[Extensions::HttpFilters::HttpFilterNames::get().Rbac].PackFrom(per_route_config);
      });
  config_helper_.addFilter(RBAC_CONFIG);

  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));
  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "POST"},
          {":path", "/"},
          {":scheme", "http"},
          {":authority", "host"},
          {"x-forwarded-for", "10.0.0.1"},
      },
      1024);

  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

  response->waitForEndStream();
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
}

TEST_P(RBACIntegrationTest, PathWithQueryAndFragmentWithOverride) {
  config_helper_.addFilter(RBAC_CONFIG_WITH_PATH_EXACT_MATCH);
  config_helper_.addRuntimeOverride("envoy.reloadable_features.http_reject_path_with_fragment",
                                    "false");
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  const std::vector<std::string> paths{"/allow", "/allow?p1=v1&p2=v2", "/allow?p1=v1#seg"};

  for (const auto& path : paths) {
    auto response = codec_client_->makeRequestWithBody(
        Http::TestRequestHeaderMapImpl{
            {":method", "POST"},
            {":path", path},
            {":scheme", "http"},
            {":authority", "host"},
            {"x-forwarded-for", "10.0.0.1"},
        },
        1024);
    waitForNextUpstreamRequest();
    upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

    response->waitForEndStream();
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  }
}

TEST_P(RBACIntegrationTest, PathWithFragmentRejectedByDefault) {
  config_helper_.addFilter(RBAC_CONFIG_WITH_PATH_EXACT_MATCH);
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "POST"},
          {":path", "/allow?p1=v1#seg"},
          {":scheme", "http"},
          {":authority", "host"},
          {"x-forwarded-for", "10.0.0.1"},
      },
      1024);
  // Request should not hit the upstream
  response->waitForEndStream();
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("400", response->headers().Status()->value().getStringView());
}

// This test ensures that the exact match deny rule is not affected by fragment and query
// when Envoy is configured to strip both fragment and query.
TEST_P(RBACIntegrationTest, DenyExactMatchIgnoresQueryAndFragment) {
  config_helper_.addFilter(RBAC_CONFIG_DENY_WITH_PATH_EXACT_MATCH);
  config_helper_.addRuntimeOverride("envoy.reloadable_features.http_reject_path_with_fragment",
                                    "false");
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  const std::vector<std::string> paths{"/deny#", "/deny#fragment", "/deny?p1=v1&p2=v2",
                                       "/deny?p1=v1#seg"};

  for (const auto& path : paths) {
    printf("Testing: %s\n", path.c_str());
    auto response = codec_client_->makeRequestWithBody(
        Http::TestRequestHeaderMapImpl{
            {":method", "POST"},
            {":path", path},
            {":scheme", "http"},
            {":authority", "host"},
            {"x-forwarded-for", "10.0.0.1"},
        },
        1024);

    response->waitForEndStream();
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("403", response->headers().Status()->value().getStringView());
    if (downstreamProtocol() == Http::CodecClient::Type::HTTP1) {
      ASSERT_TRUE(codec_client_->waitForDisconnect());
      codec_client_ = makeHttpConnection(lookupPort("http"));
    }
  }
}

TEST_P(RBACIntegrationTest, PathIgnoreCase) {
  config_helper_.addFilter(RBAC_CONFIG_WITH_PATH_IGNORE_CASE_MATCH);
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  const std::vector<std::string> paths{"/ignore_case", "/IGNORE_CASE", "/ignore_CASE"};

  for (const auto& path : paths) {
    auto response = codec_client_->makeRequestWithBody(
        Http::TestRequestHeaderMapImpl{
            {":method", "POST"},
            {":path", path},
            {":scheme", "http"},
            {":authority", "host"},
            {"x-forwarded-for", "10.0.0.1"},
        },
        1024);
    waitForNextUpstreamRequest();
    upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

    response->waitForEndStream();
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  }
}

// Basic CEL match on a header value.
TEST_P(RBACIntegrationTest, HeaderMatchCondition) {
  config_helper_.addFilter(fmt::format(RBAC_CONFIG_HEADER_MATCH_CONDITION, "yyy"));
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "POST"},
          {":path", "/path"},
          {":scheme", "http"},
          {":authority", "host"},
          {"xxx", "yyy"},
      },
      1024);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

  response->waitForEndStream();
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
}

// CEL match on a header value in which the header is a duplicate. Verifies we handle string
// copying correctly inside the CEL expression.
TEST_P(RBACIntegrationTest, HeaderMatchConditionDuplicateHeaderNoMatch) {
  config_helper_.addFilter(fmt::format(RBAC_CONFIG_HEADER_MATCH_CONDITION, "yyy"));
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "POST"},
          {":path", "/path"},
          {":scheme", "http"},
          {":authority", "host"},
          {"xxx", "yyy"},
          {"xxx", "zzz"},
      },
      1024);
  response->waitForEndStream();
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("403", response->headers().Status()->value().getStringView());
}

// CEL match on a header value in which the header is a duplicate. Verifies we handle string
// copying correctly inside the CEL expression.
TEST_P(RBACIntegrationTest, HeaderMatchConditionDuplicateHeaderMatch) {
  config_helper_.addFilter(fmt::format(RBAC_CONFIG_HEADER_MATCH_CONDITION, "yyy,zzz"));
  initialize();

  codec_client_ = makeHttpConnection(lookupPort("http"));

  auto response = codec_client_->makeRequestWithBody(
      Http::TestRequestHeaderMapImpl{
          {":method", "POST"},
          {":path", "/path"},
          {":scheme", "http"},
          {":authority", "host"},
          {"xxx", "yyy"},
          {"xxx", "zzz"},
      },
      1024);
  waitForNextUpstreamRequest();
  upstream_request_->encodeHeaders(Http::TestResponseHeaderMapImpl{{":status", "200"}}, true);

  response->waitForEndStream();
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
}

} // namespace
} // namespace Envoy
