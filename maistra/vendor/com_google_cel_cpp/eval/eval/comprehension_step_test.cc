#include "eval/eval/comprehension_step.h"

#include <cstddef>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/ident_step.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using ::google::protobuf::ListValue;
using ::google::protobuf::Struct;
using ::google::protobuf::Arena;
using testing::Eq;
using testing::SizeIs;

using IdentExpr = google::api::expr::v1alpha1::Expr::Ident;
using Expr = google::api::expr::v1alpha1::Expr;

IdentExpr CreateIdent(const std::string& var) {
  IdentExpr expr;
  expr.set_name(var);
  return expr;
}

class ListKeysStepTest : public testing::Test {
 public:
  ListKeysStepTest() {}

  std::unique_ptr<CelExpressionFlatImpl> MakeExpression(
      ExecutionPath&& path, bool unknown_attributes = false) {
    return std::make_unique<CelExpressionFlatImpl>(
        &dummy_expr_, std::move(path), 0, std::set<std::string>(),
        unknown_attributes, unknown_attributes);
  }

 private:
  Expr dummy_expr_;
};

MATCHER_P(CelStringValue, val, "") {
  const CelValue& to_match = arg;
  absl::string_view value = val;
  return to_match.IsString() && to_match.StringOrDie().value() == value;
}

TEST_F(ListKeysStepTest, ListPassedThrough) {
  ExecutionPath path;
  IdentExpr ident = CreateIdent("var");
  auto result = CreateIdentStep(&ident, 0);
  ASSERT_OK(result);
  path.push_back(std::move(result.value()));
  result = CreateListKeysStep(1);
  ASSERT_OK(result);
  path.push_back(std::move(result.value()));

  auto expression = MakeExpression(std::move(path));

  Activation activation;
  Arena arena;
  ListValue value;
  value.add_values()->set_number_value(1.0);
  value.add_values()->set_number_value(2.0);
  value.add_values()->set_number_value(3.0);
  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&value, &arena));

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsList());
  EXPECT_THAT(*eval_result->ListOrDie(), SizeIs(3));
}

TEST_F(ListKeysStepTest, MapToKeyList) {
  ExecutionPath path;
  IdentExpr ident = CreateIdent("var");
  auto result = CreateIdentStep(&ident, 0);
  ASSERT_OK(result);
  path.push_back(std::move(result.value()));
  result = CreateListKeysStep(1);
  ASSERT_OK(result);
  path.push_back(std::move(result.value()));

  auto expression = MakeExpression(std::move(path));

  Activation activation;
  Arena arena;
  Struct value;
  (*value.mutable_fields())["key1"].set_number_value(1.0);
  (*value.mutable_fields())["key2"].set_number_value(2.0);
  (*value.mutable_fields())["key3"].set_number_value(3.0);

  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&value, &arena));

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsList());
  EXPECT_THAT(*eval_result->ListOrDie(), SizeIs(3));
  std::vector<CelValue> keys;
  keys.reserve(eval_result->ListOrDie()->size());
  for (int i = 0; i < eval_result->ListOrDie()->size(); i++) {
    keys.push_back(eval_result->ListOrDie()->operator[](i));
  }
  EXPECT_THAT(keys, testing::UnorderedElementsAre(CelStringValue("key1"),
                                                  CelStringValue("key2"),
                                                  CelStringValue("key3")));
}

TEST_F(ListKeysStepTest, MapPartiallyUnknown) {
  ExecutionPath path;
  IdentExpr ident = CreateIdent("var");
  auto result = CreateIdentStep(&ident, 0);
  ASSERT_OK(result);
  path.push_back(std::move(result.value()));
  result = CreateListKeysStep(1);
  ASSERT_OK(result);
  path.push_back(std::move(result.value()));

  auto expression =
      MakeExpression(std::move(path), /*unknown_attributes=*/true);

  Activation activation;
  Arena arena;
  Struct value;
  (*value.mutable_fields())["key1"].set_number_value(1.0);
  (*value.mutable_fields())["key2"].set_number_value(2.0);
  (*value.mutable_fields())["key3"].set_number_value(3.0);

  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&value, &arena));
  activation.set_unknown_attribute_patterns({CelAttributePattern(
      "var",
      {CelAttributeQualifierPattern::Create(CelValue::CreateStringView("key2")),
       CelAttributeQualifierPattern::Create(CelValue::CreateStringView("foo")),
       CelAttributeQualifierPattern::CreateWildcard()})});

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsUnknownSet());
  const auto& attrs =
      eval_result->UnknownSetOrDie()->unknown_attributes().attributes();

  EXPECT_THAT(attrs, SizeIs(1));
  EXPECT_THAT(attrs.at(0)->variable().ident_expr().name(), Eq("var"));
  EXPECT_THAT(attrs.at(0)->qualifier_path(), SizeIs(0));
}

TEST_F(ListKeysStepTest, ErrorPassedThrough) {
  ExecutionPath path;
  IdentExpr ident = CreateIdent("var");
  auto result = CreateIdentStep(&ident, 0);
  ASSERT_OK(result);
  path.push_back(std::move(result.value()));
  result = CreateListKeysStep(1);
  ASSERT_OK(result);
  path.push_back(std::move(result.value()));

  auto expression = MakeExpression(std::move(path));

  Activation activation;
  Arena arena;

  // Var not in activation, turns into cel error at eval time.
  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsError());
  EXPECT_THAT(eval_result->ErrorOrDie()->message(),
              testing::HasSubstr("\"var\""));
  EXPECT_EQ(eval_result->ErrorOrDie()->code(), absl::StatusCode::kUnknown);
}

TEST_F(ListKeysStepTest, UnknownSetPassedThrough) {
  ExecutionPath path;
  IdentExpr ident = CreateIdent("var");
  auto result = CreateIdentStep(&ident, 0);
  ASSERT_OK(result);
  path.push_back(std::move(result.value()));
  result = CreateListKeysStep(1);
  ASSERT_OK(result);
  path.push_back(std::move(result.value()));

  auto expression =
      MakeExpression(std::move(path), /*unknown_attributes=*/true);

  Activation activation;
  Arena arena;

  activation.set_unknown_attribute_patterns({CelAttributePattern("var", {})});

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsUnknownSet());
  EXPECT_THAT(eval_result->UnknownSetOrDie()->unknown_attributes().attributes(),
              SizeIs(1));
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
