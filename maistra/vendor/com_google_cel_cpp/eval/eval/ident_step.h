#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_IDENT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_IDENT_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Factory method for Ident - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStep(
    const google::api::expr::v1alpha1::Expr::Ident* ident, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_IDENT_STEP_H_
