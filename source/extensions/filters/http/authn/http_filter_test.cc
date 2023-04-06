/* Copyright 2018 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "source/extensions/filters/http/authn/http_filter.h"

#include "envoy/config/filter/http/authn/v2alpha1/config.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/common/base64.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/stream_info/stream_info_impl.h"
#include "source/extensions/common/authn.h"
#include "source/extensions/filters/http/authn/authenticator_base.h"
#include "source/extensions/filters/http/authn/test_utils.h"
#include "test/mocks/http/mocks.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/utility.h"

using Envoy::Http::Istio::AuthN::AuthenticatorBase;
using Envoy::Http::Istio::AuthN::FilterContext;
using istio::authn::Payload;
using istio::authn::Result;
using istio::envoy::config::filter::http::authn::v2alpha1::FilterConfig;
using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::NiceMock;
using testing::ReturnRef;
using testing::StrictMock;

namespace iaapi = istio::authentication::v1alpha1;

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {

const char ingoreBothPolicy[] = R"(
  peer_is_optional: true
  origin_is_optional: true
)";

// Create a fake authenticator for test. This authenticator do nothing except
// making the authentication fail.
std::unique_ptr<AuthenticatorBase> createAlwaysFailAuthenticator(
    FilterContext *filter_context) {
  class _local : public AuthenticatorBase {
   public:
    _local(FilterContext *filter_context) : AuthenticatorBase(filter_context) {}
    bool run(Payload *) override { return false; }
  };
  return std::make_unique<_local>(filter_context);
}

// Create a fake authenticator for test. This authenticator do nothing except
// making the authentication successful.
std::unique_ptr<AuthenticatorBase> createAlwaysPassAuthenticator(
    FilterContext *filter_context) {
  class _local : public AuthenticatorBase {
   public:
    _local(FilterContext *filter_context) : AuthenticatorBase(filter_context) {}
    bool run(Payload *) override {
      // Set some data to verify authentication result later.
      auto payload = TestUtilities::CreateX509Payload(
          "cluster.local/sa/test_user/ns/test_ns/");
      filter_context()->setPeerResult(&payload);
      return true;
    }
  };
  return std::make_unique<_local>(filter_context);
}

class MockAuthenticationFilter : public AuthenticationFilter {
 public:
  // We'll use fake authenticator for test, so policy is not really needed. Use
  // default config for simplicity.
  MockAuthenticationFilter(const FilterConfig &filter_config)
      : AuthenticationFilter(filter_config) {}

  ~MockAuthenticationFilter(){};

  MOCK_METHOD1(createPeerAuthenticator,
               std::unique_ptr<AuthenticatorBase>(FilterContext *));
  MOCK_METHOD1(createOriginAuthenticator,
               std::unique_ptr<AuthenticatorBase>(FilterContext *));
};

class AuthenticationFilterTest : public testing::Test {
 public:
  AuthenticationFilterTest()
      : request_headers_{{":method", "GET"}, {":path", "/"}} {}
  ~AuthenticationFilterTest() {}

  void SetUp() override {
    filter_.setDecoderFilterCallbacks(decoder_callbacks_);
  }

 protected:
  FilterConfig filter_config_ = FilterConfig::default_instance();

  Http::TestRequestHeaderMapImpl request_headers_;
  StrictMock<MockAuthenticationFilter> filter_{filter_config_};
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
};

TEST_F(AuthenticationFilterTest, PeerFail) {
  // Peer authentication fail, request should be rejected with 401. No origin
  // authentiation needed.
  EXPECT_CALL(filter_, createPeerAuthenticator(_))
      .Times(1)
      .WillOnce(Invoke(createAlwaysFailAuthenticator));
  Envoy::Event::SimulatedTimeSystem test_time;
  StreamInfo::StreamInfoImpl stream_info(Http::Protocol::Http2,
                                         test_time.timeSystem(), nullptr);
  EXPECT_CALL(decoder_callbacks_, streamInfo())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(stream_info));
  EXPECT_CALL(decoder_callbacks_, encodeHeaders_(_, _))
      .Times(1)
      .WillOnce(testing::Invoke([](Http::ResponseHeaderMap &headers, bool) {
        EXPECT_EQ("401", headers.Status()->value().getStringView());
      }));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_.decodeHeaders(request_headers_, true));
  EXPECT_FALSE(Utils::Authentication::GetResultFromMetadata(
      stream_info.dynamicMetadata()));
}

TEST_F(AuthenticationFilterTest, PeerPassOriginFail) {
  // Peer pass thus origin authentication must be called. Final result should
  // fail as origin authn fails.
  EXPECT_CALL(filter_, createPeerAuthenticator(_))
      .Times(1)
      .WillOnce(Invoke(createAlwaysPassAuthenticator));
  EXPECT_CALL(filter_, createOriginAuthenticator(_))
      .Times(1)
      .WillOnce(Invoke(createAlwaysFailAuthenticator));
  Envoy::Event::SimulatedTimeSystem test_time;
  StreamInfo::StreamInfoImpl stream_info(Http::Protocol::Http2,
                                         test_time.timeSystem(), nullptr);
  EXPECT_CALL(decoder_callbacks_, streamInfo())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(stream_info));
  EXPECT_CALL(decoder_callbacks_, encodeHeaders_(_, _))
      .Times(1)
      .WillOnce(testing::Invoke([](Http::ResponseHeaderMap &headers, bool) {
        EXPECT_EQ("401", headers.Status()->value().getStringView());
      }));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_.decodeHeaders(request_headers_, true));
  EXPECT_FALSE(Utils::Authentication::GetResultFromMetadata(
      stream_info.dynamicMetadata()));
}

TEST_F(AuthenticationFilterTest, AllPass) {
  EXPECT_CALL(filter_, createPeerAuthenticator(_))
      .Times(1)
      .WillOnce(Invoke(createAlwaysPassAuthenticator));
  EXPECT_CALL(filter_, createOriginAuthenticator(_))
      .Times(1)
      .WillOnce(Invoke(createAlwaysPassAuthenticator));
  Envoy::Event::SimulatedTimeSystem test_time;
  StreamInfo::StreamInfoImpl stream_info(Http::Protocol::Http2,
                                         test_time.timeSystem(), nullptr);
  EXPECT_CALL(decoder_callbacks_, streamInfo())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(stream_info));

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_.decodeHeaders(request_headers_, true));

  EXPECT_EQ(1, stream_info.dynamicMetadata().filter_metadata_size());
  const auto *data = Utils::Authentication::GetResultFromMetadata(
      stream_info.dynamicMetadata());
  ASSERT_TRUE(data);

  ProtobufWkt::Struct expected_data;
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"(
       fields {
         key: "source.namespace"
         value {
           string_value: "test_ns"
         }
       }
       fields {
         key: "source.principal"
         value {
           string_value: "cluster.local/sa/test_user/ns/test_ns/"
         }
       }
       fields {
         key: "source.user"
         value {
           string_value: "cluster.local/sa/test_user/ns/test_ns/"
         }
       })",
                                                    &expected_data));
  EXPECT_TRUE(TestUtility::protoEqual(expected_data, *data));
}

TEST_F(AuthenticationFilterTest, IgnoreBothFail) {
  iaapi::Policy policy_;
  ASSERT_TRUE(
      Protobuf::TextFormat::ParseFromString(ingoreBothPolicy, &policy_));
  *filter_config_.mutable_policy() = policy_;
  StrictMock<MockAuthenticationFilter> filter(filter_config_);
  filter.setDecoderFilterCallbacks(decoder_callbacks_);

  EXPECT_CALL(filter, createPeerAuthenticator(_))
      .Times(1)
      .WillOnce(Invoke(createAlwaysFailAuthenticator));
  EXPECT_CALL(filter, createOriginAuthenticator(_))
      .Times(1)
      .WillOnce(Invoke(createAlwaysFailAuthenticator));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter.decodeHeaders(request_headers_, true));
}

TEST_F(AuthenticationFilterTest, IgnoreBothPass) {
  iaapi::Policy policy_;
  ASSERT_TRUE(
      Protobuf::TextFormat::ParseFromString(ingoreBothPolicy, &policy_));
  *filter_config_.mutable_policy() = policy_;
  StrictMock<MockAuthenticationFilter> filter(filter_config_);
  filter.setDecoderFilterCallbacks(decoder_callbacks_);

  EXPECT_CALL(filter, createPeerAuthenticator(_))
      .Times(1)
      .WillOnce(Invoke(createAlwaysPassAuthenticator));
  EXPECT_CALL(filter, createOriginAuthenticator(_))
      .Times(1)
      .WillOnce(Invoke(createAlwaysPassAuthenticator));
  Envoy::Event::SimulatedTimeSystem test_time;
  StreamInfo::StreamInfoImpl stream_info(Http::Protocol::Http2,
                                         test_time.timeSystem(), nullptr);
  EXPECT_CALL(decoder_callbacks_, streamInfo())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(stream_info));

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter.decodeHeaders(request_headers_, true));

  EXPECT_EQ(1, stream_info.dynamicMetadata().filter_metadata_size());
  const auto *data = Utils::Authentication::GetResultFromMetadata(
      stream_info.dynamicMetadata());
  ASSERT_TRUE(data);

  ProtobufWkt::Struct expected_data;
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"(
       fields {
         key: "source.namespace"
         value {
           string_value: "test_ns"
         }
       }
       fields {
         key: "source.principal"
         value {
           string_value: "cluster.local/sa/test_user/ns/test_ns/"
         }
       }
       fields {
         key: "source.user"
         value {
           string_value: "cluster.local/sa/test_user/ns/test_ns/"
         }
       })",
                                                    &expected_data));
  EXPECT_TRUE(TestUtility::protoEqual(expected_data, *data));
}

}  // namespace
}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
