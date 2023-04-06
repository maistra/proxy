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

#include "source/extensions/filters/http/authn/origin_authenticator.h"

#include "authentication/v1alpha1/policy.pb.h"
#include "envoy/config/core/v3/base.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/protobuf/protobuf.h"
#include "source/extensions/filters/http/authn/test_utils.h"
#include "test/mocks/http/mocks.h"
#include "test/test_common/utility.h"

namespace iaapi = istio::authentication::v1alpha1;

using istio::authn::Payload;
using istio::authn::Result;
using testing::_;
using testing::DoAll;
using testing::MockFunction;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {

const char kZeroOriginMethodPolicyBindPeer[] = R"(
  principal_binding: USE_PEER
)";

const char kZeroOriginMethodPolicyBindOrigin[] = R"(
  principal_binding: USE_ORIGIN
)";

const char kSingleOriginMethodPolicy[] = R"(
  principal_binding: USE_ORIGIN
  origins {
    jwt {
      issuer: "abc.xyz"
    }
  }
)";

const char kMultipleOriginMethodsPolicy[] = R"(
  principal_binding: USE_ORIGIN
  origins {
    jwt {
      issuer: "one"
    }
  }
  origins {
    jwt {
      issuer: "two"
    }
  }
  origins {
    jwt {
      issuer: "three"
    }
  }
)";

const char kPeerBinding[] = R"(
  principal_binding: USE_PEER
  origins {
    jwt {
      issuer: "abc.xyz"
    }
  }
)";

const char kSingleOriginMethodWithTriggerRulePolicy[] = R"(
  principal_binding: USE_ORIGIN
  origins {
    jwt {
      issuer: "abc.xyz"
      trigger_rules: {
        included_paths: {
          exact: "/allow"
        }
      }
    }
  }
)";

const char kSingleOriginMethodWithExcludeTriggerRulePolicy[] = R"(
  principal_binding: USE_ORIGIN
  origins {
    jwt {
      issuer: "abc.xyz"
      trigger_rules: {
        excluded_paths: {
          exact: "/login"
        }
      }
    }
  }
)";

const char kMultipleOriginMethodWithTriggerRulePolicy[] = R"(
  principal_binding: USE_ORIGIN
  origins {
    jwt {
      issuer: "one"
      trigger_rules: {
        excluded_paths: {
          exact: "/bad"
        }
      }
    }
  }
  origins {
    jwt {
      issuer: "two"
      trigger_rules: {
        included_paths: {
          exact: "/two"
        }
      }
    }
  }
  origins {
    jwt {
      issuer: "three"
      trigger_rules: {
        included_paths: {
          exact: "/allow"
        }
      }
    }
  }
)";

class MockOriginAuthenticator : public OriginAuthenticator {
 public:
  MockOriginAuthenticator(FilterContext* filter_context,
                          const iaapi::Policy& policy)
      : OriginAuthenticator(filter_context, policy) {}

  MOCK_CONST_METHOD2(validateX509, bool(const iaapi::MutualTls&, Payload*));
  MOCK_METHOD2(validateJwt, bool(const iaapi::Jwt&, Payload*));
};

class OriginAuthenticatorTest : public testing::TestWithParam<bool> {
 public:
  OriginAuthenticatorTest() {}
  virtual ~OriginAuthenticatorTest() {}

  void SetUp() override {
    expected_result_when_pass_ = TestUtilities::AuthNResultFromString(R"(
      principal: "foo"
      origin {
        user: "foo"
        presenter: "istio.io"
      }
    )");
    set_peer_ = GetParam();
    if (set_peer_) {
      auto peer_result = TestUtilities::CreateX509Payload("bar");
      filter_context_.setPeerResult(&peer_result);
      expected_result_when_pass_.set_peer_user("bar");
    }
    initial_result_ = filter_context_.authenticationResult();
    payload_ = new Payload();
  }

  void TearDown() override { delete (payload_); }

  void createAuthenticator() {
    authenticator_.reset(
        new StrictMock<MockOriginAuthenticator>(&filter_context_, policy_));
  }

 protected:
  std::unique_ptr<StrictMock<MockOriginAuthenticator>> authenticator_;
  // envoy::config::core::v3::Metadata metadata_;
  Envoy::Http::TestRequestHeaderMapImpl header_{};
  FilterContext filter_context_{
      envoy::config::core::v3::Metadata::default_instance(), header_, nullptr,
      istio::envoy::config::filter::http::authn::v2alpha1::FilterConfig::
          default_instance()};
  iaapi::Policy policy_;

  Payload* payload_;

  // Mock response payload.
  Payload jwt_payload_{TestUtilities::CreateJwtPayload("foo", "istio.io")};
  Payload jwt_extra_payload_{
      TestUtilities::CreateJwtPayload("bar", "istio.io")};

  // Expected result (when authentication pass with mock payload above)
  Result expected_result_when_pass_;
  // Copy of authN result (from filter context) before running authentication.
  // This should be the expected result if authn fail or do nothing.
  Result initial_result_;

  // Indicates peer is set in the authN result before running. This is set from
  // test GetParam()
  bool set_peer_;

  void setPath(const std::string& path) {
    header_.removePath();
    header_.addCopy(":path", path);
  }

  void addHeader(const std::string& key, const std::string& value) {
    header_.addCopy(key, value);
  }
};

TEST_P(OriginAuthenticatorTest, Empty) {
  createAuthenticator();
  authenticator_->run(payload_);
  if (set_peer_) {
    initial_result_.set_principal("bar");
  }
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

// It should fail if the binding is USE_ORIGIN but origin methods are empty.
TEST_P(OriginAuthenticatorTest, ZeroMethodFail) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      kZeroOriginMethodPolicyBindOrigin, &policy_));
  createAuthenticator();
  EXPECT_FALSE(authenticator_->run(payload_));
}

// It should pass if the binding is USE_PEER and origin methods are empty.
TEST_P(OriginAuthenticatorTest, ZeroMethodPass) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      kZeroOriginMethodPolicyBindPeer, &policy_));
  createAuthenticator();

  Result expected_result = TestUtilities::AuthNResultFromString(R"(
      origin {
        user: "bar"
        presenter: "istio.io"
      }
    )");
  if (set_peer_) {
    expected_result.set_principal("bar");
    expected_result.set_peer_user("bar");
  }

  EXPECT_TRUE(authenticator_->run(&jwt_extra_payload_));
  EXPECT_TRUE(TestUtility::protoEqual(expected_result,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, SingleMethodPass) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(kSingleOriginMethodPolicy,
                                                    &policy_));

  createAuthenticator();

  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(jwt_payload_), Return(true)));

  authenticator_->run(payload_);
  EXPECT_TRUE(TestUtility::protoEqual(expected_result_when_pass_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, SingleMethodFail) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(kSingleOriginMethodPolicy,
                                                    &policy_));

  createAuthenticator();

  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(jwt_payload_), Return(false)));

  authenticator_->run(payload_);
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, CORSPreflight) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(kSingleOriginMethodPolicy,
                                                    &policy_));

  createAuthenticator();

  EXPECT_CALL(*authenticator_, validateJwt(_, _)).Times(0);

  addHeader(":method", "OPTIONS");
  addHeader("origin", "example.com");
  addHeader("access-control-request-method", "GET");
  EXPECT_TRUE(authenticator_->run(payload_));
}

TEST_P(OriginAuthenticatorTest, TriggeredWithNullPath) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      kSingleOriginMethodWithTriggerRulePolicy, &policy_));

  createAuthenticator();

  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(jwt_payload_), Return(true)));

  EXPECT_TRUE(authenticator_->run(payload_));
  EXPECT_TRUE(TestUtility::protoEqual(expected_result_when_pass_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, SingleRuleTriggered) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      kSingleOriginMethodWithTriggerRulePolicy, &policy_));

  createAuthenticator();

  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(jwt_payload_), Return(true)));

  setPath("/allow");
  EXPECT_TRUE(authenticator_->run(payload_));
  EXPECT_TRUE(TestUtility::protoEqual(expected_result_when_pass_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, SingleRuleTriggeredWithComponents) {
  const std::vector<std::string> input_paths{"/allow?",
                                             "/allow?a=b&c=d",
                                             "/allow??",
                                             "/allow??",
                                             "/allow?#",
                                             "/allow#?",
                                             "/allow#a",
                                             "/allow#$"
                                             "/allow?a=b#c",
                                             "/allow#a?b=c"};
  for (const auto& path : input_paths) {
    ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
        kSingleOriginMethodWithTriggerRulePolicy, &policy_));

    createAuthenticator();

    EXPECT_CALL(*authenticator_, validateJwt(_, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<1>(jwt_payload_), Return(true)));

    setPath(path);
    EXPECT_TRUE(authenticator_->run(payload_));
    EXPECT_TRUE(TestUtility::protoEqual(
        expected_result_when_pass_, filter_context_.authenticationResult()));
  }
}

TEST_P(OriginAuthenticatorTest, SingleRuleNotTriggered) {
  const std::vector<std::string> input_paths{"/bad", "/allow$?", "/allow$#"};
  for (const auto& path : input_paths) {
    ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
        kSingleOriginMethodWithTriggerRulePolicy, &policy_));

    createAuthenticator();

    EXPECT_CALL(*authenticator_, validateJwt(_, _)).Times(0);

    setPath(path);
    EXPECT_TRUE(authenticator_->run(payload_));
    EXPECT_TRUE(TestUtility::protoEqual(
        initial_result_, filter_context_.authenticationResult()));
  }
}

TEST_P(OriginAuthenticatorTest, SingleExcludeRuleTriggeredWithQueryParam) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      kSingleOriginMethodWithExcludeTriggerRulePolicy, &policy_));

  createAuthenticator();

  EXPECT_CALL(*authenticator_, validateJwt(_, _)).Times(0);

  setPath("/login?a=b&c=d");
  EXPECT_TRUE(authenticator_->run(payload_));
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, Multiple) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      kMultipleOriginMethodsPolicy, &policy_));

  createAuthenticator();

  // First method fails, second success (thus third is ignored)
  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(2)
      .WillOnce(DoAll(SetArgPointee<1>(jwt_extra_payload_), Return(false)))
      .WillOnce(DoAll(SetArgPointee<1>(jwt_payload_), Return(true)));

  authenticator_->run(payload_);
  EXPECT_TRUE(TestUtility::protoEqual(expected_result_when_pass_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, MultipleFail) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      kMultipleOriginMethodsPolicy, &policy_));

  createAuthenticator();

  // All fail.
  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(3)
      .WillRepeatedly(
          DoAll(SetArgPointee<1>(jwt_extra_payload_), Return(false)));

  authenticator_->run(payload_);
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, MultipleRuleTriggeredValidationSucceeded) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      kMultipleOriginMethodWithTriggerRulePolicy, &policy_));

  createAuthenticator();
  // First method triggered but failed, second method not triggered, third
  // method triggered and succeeded.
  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(2)
      .WillOnce(DoAll(SetArgPointee<1>(jwt_extra_payload_), Return(false)))
      .WillOnce(DoAll(SetArgPointee<1>(jwt_payload_), Return(true)));

  setPath("/allow");
  EXPECT_TRUE(authenticator_->run(payload_));
  EXPECT_TRUE(TestUtility::protoEqual(expected_result_when_pass_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, MultipleRuleTriggeredValidationFailed) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      kMultipleOriginMethodWithTriggerRulePolicy, &policy_));

  createAuthenticator();
  // Triggered on first and second method but all failed.
  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(2)
      .WillRepeatedly(
          DoAll(SetArgPointee<1>(jwt_extra_payload_), Return(false)));

  setPath("/two");
  EXPECT_FALSE(authenticator_->run(payload_));
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, MultipleRuleNotTriggered) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(
      kMultipleOriginMethodWithTriggerRulePolicy, &policy_));

  createAuthenticator();
  EXPECT_CALL(*authenticator_, validateJwt(_, _)).Times(0);

  setPath("/bad");
  EXPECT_TRUE(authenticator_->run(payload_));
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, PeerBindingPass) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(kPeerBinding, &policy_));
  // Expected principal is from peer_user.
  expected_result_when_pass_.set_principal(initial_result_.peer_user());

  createAuthenticator();

  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(jwt_payload_), Return(true)));

  authenticator_->run(payload_);
  EXPECT_TRUE(TestUtility::protoEqual(expected_result_when_pass_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, PeerBindingFail) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(kPeerBinding, &policy_));
  createAuthenticator();

  // All fail.
  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(jwt_payload_), Return(false)));

  authenticator_->run(payload_);
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

INSTANTIATE_TEST_SUITE_P(OriginAuthenticatorTests, OriginAuthenticatorTest,
                         testing::Bool());

}  // namespace
}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
