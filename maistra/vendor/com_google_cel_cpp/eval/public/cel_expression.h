#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPRESSION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPRESSION_H_

#include <functional>
#include <memory>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "eval/public/activation.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelEvaluationListener is the callback that is passed to (and called by)
// CelEvaluation::Trace. It gets an expression node ID from the original
// expression, its value and the arena object. If an expression node
// is evaluated multiple times (e.g. as a part of Comprehension.loop_step)
// then the order of the callback invocations is guaranteed to correspond
// the order of variable sub-elements (e.g. the order of elements returned
// by Comprehension.iter_range).
using CelEvaluationListener = std::function<absl::Status(
    int64_t expr_id, const CelValue&, google::protobuf::Arena*)>;

// An opaque state used for evaluation of a cell expression.
class CelEvaluationState {
 public:
  virtual ~CelEvaluationState() = default;
};

// Base interface for expression evaluating objects.
class CelExpression {
 public:
  virtual ~CelExpression() = default;

  // Initializes the state
  virtual std::unique_ptr<CelEvaluationState> InitializeState(
      google::protobuf::Arena* arena) const = 0;

  // Evaluates expression and returns value.
  // activation contains bindings from parameter names to values
  // arena parameter specifies Arena object where output result and
  // internal data will be allocated.
  virtual absl::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                            google::protobuf::Arena* arena) const = 0;

  // Evaluates expression and returns value.
  // activation contains bindings from parameter names to values
  // state must be non-null and created prior to calling Evaluate by
  // InitializeState.
  virtual absl::StatusOr<CelValue> Evaluate(
      const BaseActivation& activation, CelEvaluationState* state) const = 0;

  // Trace evaluates expression calling the callback on each sub-tree.
  virtual absl::StatusOr<CelValue> Trace(
      const BaseActivation& activation, google::protobuf::Arena* arena,
      CelEvaluationListener callback) const = 0;

  // Trace evaluates expression calling the callback on each sub-tree.
  // state must be non-null and created prior to calling Evaluate by
  // InitializeState.
  virtual absl::StatusOr<CelValue> Trace(
      const BaseActivation& activation, CelEvaluationState* state,
      CelEvaluationListener callback) const = 0;
};

// Base class for Expression Builder implementations
// Provides user with factory to register extension functions.
// ExpressionBuilder MUST NOT be destroyed before CelExpression objects
// it built.
class CelExpressionBuilder {
 public:
  CelExpressionBuilder()
      : func_registry_(absl::make_unique<CelFunctionRegistry>()),
        type_registry_(absl::make_unique<CelTypeRegistry>()),
        container_("") {}

  virtual ~CelExpressionBuilder() {}

  // Creates CelExpression object from AST tree.
  // expr specifies root of AST tree
  virtual absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info) const = 0;

  // Creates CelExpression object from AST tree.
  // expr specifies root of AST tree.
  // non-fatal build warnings are written to warnings if encountered.
  virtual absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info,
      std::vector<absl::Status>* warnings) const = 0;

  // Creates CelExpression object from a checked expression.
  // This includes an AST, source info, type hints and ident hints.
  // checked_expr ptr must outlive any expressions that are built from it.
  virtual absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::CheckedExpr* checked_expr) const {
    // Default implementation just passes through the expr and source info.
    return CreateExpression(&checked_expr->expr(),
                            &checked_expr->source_info());
  }

  // Creates CelExpression object from a checked expression.
  // This includes an AST, source info, type hints and ident hints.
  // checked_expr ptr must outlive any expressions that are built from it.
  // non-fatal build warnings are written to warnings if encountered.
  virtual absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::CheckedExpr* checked_expr,
      std::vector<absl::Status>* warnings) const {
    // Default implementation just passes through the expr and source_info.
    return CreateExpression(&checked_expr->expr(), &checked_expr->source_info(),
                            warnings);
  }

  // CelFunction registry. Extension function should be registered with it
  // prior to expression creation.
  CelFunctionRegistry* GetRegistry() const { return func_registry_.get(); }

  // CEL Type registry. Provides a means to resolve the CEL built-in types to
  // CelValue instances, and to extend the set of types and enums known to
  // expressions by registering them ahead of time.
  CelTypeRegistry* GetTypeRegistry() const { return type_registry_.get(); }

  // Add Enum to the list of resolvable by the builder.
  void ABSL_DEPRECATED("Use GetTypeRegistry()->Register() instead")
      AddResolvableEnum(const google::protobuf::EnumDescriptor* enum_descriptor) {
    type_registry_->Register(enum_descriptor);
  }

  void set_container(std::string container) {
    container_ = std::move(container);
  }

  absl::string_view container() const { return container_; }

 private:
  std::unique_ptr<CelFunctionRegistry> func_registry_;
  std::unique_ptr<CelTypeRegistry> type_registry_;
  std::string container_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPRESSION_H_
