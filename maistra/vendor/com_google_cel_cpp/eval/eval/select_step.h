#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_SELECT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_SELECT_STEP_H_

#include <cstdint>
#include <memory>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateSelectStep(
    const google::api::expr::v1alpha1::Expr::Select* select_expr, int64_t expr_id,
    absl::string_view select_path);

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateSelectStep(
    const google::api::expr::v1alpha1::Expr::Select* select_expr, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_SELECT_STEP_H_
