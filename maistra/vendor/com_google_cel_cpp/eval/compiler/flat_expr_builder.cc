/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "eval/compiler/flat_expr_builder.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stack>
#include <string>
#include <utility>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "eval/compiler/constant_folding.h"
#include "eval/compiler/qualified_reference_resolver.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/comprehension_step.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/container_access_step.h"
#include "eval/eval/create_list_step.h"
#include "eval/eval/create_struct_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/eval/function_step.h"
#include "eval/eval/ident_step.h"
#include "eval/eval/jump_step.h"
#include "eval/eval/logic_step.h"
#include "eval/eval/select_step.h"
#include "eval/eval/shadowable_value_step.h"
#include "eval/eval/ternary_step.h"
#include "eval/public/ast_traverse.h"
#include "eval/public/ast_visitor.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/source_position.h"

namespace google::api::expr::runtime {

namespace {

using ::google::api::expr::v1alpha1::CheckedExpr;
using ::google::api::expr::v1alpha1::Constant;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::Reference;
using ::google::api::expr::v1alpha1::SourceInfo;
using Ident = ::google::api::expr::v1alpha1::Expr::Ident;
using Select = ::google::api::expr::v1alpha1::Expr::Select;
using Call = ::google::api::expr::v1alpha1::Expr::Call;
using CreateList = ::google::api::expr::v1alpha1::Expr::CreateList;
using CreateStruct = ::google::api::expr::v1alpha1::Expr::CreateStruct;
using Comprehension = ::google::api::expr::v1alpha1::Expr::Comprehension;

// Forward declare to resolve circular dependency for short_circuiting visitors.
class FlatExprVisitor;

// A convenience wrapper for offset-calculating logic.
class Jump {
 public:
  explicit Jump() : self_index_(-1), jump_step_(nullptr) {}
  explicit Jump(int self_index, JumpStepBase* jump_step)
      : self_index_(self_index), jump_step_(jump_step) {}
  void set_target(int index) {
    // 0 offset means no-op.
    jump_step_->set_jump_offset(index - self_index_ - 1);
  }
  bool exists() { return jump_step_ != nullptr; }

 private:
  int self_index_;
  JumpStepBase* jump_step_;
};

class CondVisitor {
 public:
  virtual ~CondVisitor() {}
  virtual void PreVisit(const Expr* expr) = 0;
  virtual void PostVisitArg(int arg_num, const Expr* expr) = 0;
  virtual void PostVisit(const Expr* expr) = 0;
};

// Visitor managing the "&&" and "||" operatiions.
class BinaryCondVisitor : public CondVisitor {
 public:
  explicit BinaryCondVisitor(FlatExprVisitor* visitor, bool cond_value,
                             bool short_circuiting)
      : visitor_(visitor),
        cond_value_(cond_value),
        short_circuiting_(short_circuiting) {}

  void PreVisit(const Expr* expr) override;
  void PostVisitArg(int arg_num, const Expr* expr) override;
  void PostVisit(const Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  const bool cond_value_;
  Jump jump_step_;
  bool short_circuiting_;
};

class TernaryCondVisitor : public CondVisitor {
 public:
  explicit TernaryCondVisitor(FlatExprVisitor* visitor) : visitor_(visitor) {}

  void PreVisit(const Expr* expr) override;
  void PostVisitArg(int arg_num, const Expr* expr) override;
  void PostVisit(const Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  Jump jump_to_second_;
  Jump error_jump_;
  Jump jump_after_first_;
};

class ExhaustiveTernaryCondVisitor : public CondVisitor {
 public:
  explicit ExhaustiveTernaryCondVisitor(FlatExprVisitor* visitor)
      : visitor_(visitor) {}

  void PreVisit(const Expr* expr) override;
  void PostVisitArg(int arg_num, const Expr* expr) override {}
  void PostVisit(const Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
};

// Visitor Comprehension expression.
class ComprehensionVisitor : public CondVisitor {
 public:
  explicit ComprehensionVisitor(FlatExprVisitor* visitor, bool short_circuiting,
                                bool enable_vulnerability_check)
      : visitor_(visitor),
        next_step_(nullptr),
        cond_step_(nullptr),
        short_circuiting_(short_circuiting),
        enable_vulnerability_check_(enable_vulnerability_check) {}

  void PreVisit(const Expr* expr) override;
  void PostVisitArg(int arg_num, const Expr* expr) override;
  void PostVisit(const Expr* expr) override;

 private:
  FlatExprVisitor* visitor_;
  ComprehensionNextStep* next_step_;
  ComprehensionCondStep* cond_step_;
  int next_step_pos_;
  int cond_step_pos_;
  bool short_circuiting_;
  bool enable_vulnerability_check_;
};

class FlatExprVisitor : public AstVisitor {
 public:
  FlatExprVisitor(
      const Resolver& resolver, ExecutionPath* path, bool short_circuiting,
      const absl::flat_hash_map<std::string, CelValue>& constant_idents,
      bool enable_comprehension, bool enable_comprehension_list_append,
      bool enable_comprehension_vulnerability_check,
      bool enable_wrapper_type_null_unboxing, BuilderWarnings* warnings,
      std::set<std::string>* iter_variable_names)
      : resolver_(resolver),
        flattened_path_(path),
        progress_status_(absl::OkStatus()),
        resolved_select_expr_(nullptr),
        short_circuiting_(short_circuiting),
        constant_idents_(constant_idents),
        enable_comprehension_(enable_comprehension),
        enable_comprehension_list_append_(enable_comprehension_list_append),
        enable_comprehension_vulnerability_check_(
            enable_comprehension_vulnerability_check),
        enable_wrapper_type_null_unboxing_(enable_wrapper_type_null_unboxing),
        builder_warnings_(warnings),
        iter_variable_names_(iter_variable_names) {
    GOOGLE_CHECK(iter_variable_names_);
  }

  void PreVisitExpr(const Expr* expr, const SourcePosition*) override {
    ValidateOrError(expr->expr_kind_case() != Expr::EXPR_KIND_NOT_SET,
                    "Invalid empty expression");
  }

  void PostVisitConst(const Constant* const_expr, const Expr* expr,
                      const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    auto value = ConvertConstant(const_expr);
    if (ValidateOrError(value.has_value(), "Unsupported constant type")) {
      AddStep(CreateConstValueStep(*value, expr->id()));
    }
  }

  // Ident node handler.
  // Invoked after child nodes are processed.
  void PostVisitIdent(const Ident* ident_expr, const Expr* expr,
                      const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    const std::string& path = ident_expr->name();
    if (!ValidateOrError(
            !path.empty(),
            "Invalid expression: identifier 'name' must not be empty")) {
      return;
    }

    // Automatically replace constant idents with the backing CEL values.
    auto constant = constant_idents_.find(path);
    if (constant != constant_idents_.end()) {
      AddStep(CreateConstValueStep(constant->second, expr->id(), false));
      return;
    }

    // Attempt to resolve a select expression as a namespaced identifier for an
    // enum or type constant value.
    absl::optional<CelValue> const_value = absl::nullopt;
    while (!namespace_stack_.empty()) {
      const auto& select_node = namespace_stack_.front();
      // Generate path in format "<ident>.<field 0>.<field 1>...".
      auto select_expr = select_node.first;
      auto qualified_path = absl::StrCat(path, ".", select_node.second);
      namespace_map_[select_expr] = qualified_path;

      // Attempt to find a constant enum or type value which matches the
      // qualified path present in the expression. Whether the identifier
      // can be resolved to a type instance depends on whether the option to
      // 'enable_qualified_type_identifiers' is set to true.
      const_value = resolver_.FindConstant(qualified_path, select_expr->id());
      if (const_value.has_value()) {
        AddStep(CreateShadowableValueStep(qualified_path, *const_value,
                                          select_expr->id()));
        resolved_select_expr_ = select_expr;
        namespace_stack_.clear();
        return;
      }
      namespace_stack_.pop_front();
    }

    // Attempt to resolve a simple identifier as an enum or type constant value.
    const_value = resolver_.FindConstant(path, expr->id());
    if (const_value.has_value()) {
      AddStep(CreateShadowableValueStep(path, *const_value, expr->id()));
      return;
    }

    AddStep(CreateIdentStep(ident_expr, expr->id()));
  }

  void PreVisitSelect(const Select* select_expr, const Expr* expr,
                      const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (!ValidateOrError(
            !select_expr->field().empty(),
            "Invalid expression: select 'field' must not be empty")) {
      return;
    }

    // Not exactly the cleanest solution - we peek into child of
    // select_expr.
    // Chain of multiple SELECT ending with IDENT can represent namespaced
    // entity.
    if (!select_expr->test_only() &&
        (select_expr->operand().has_ident_expr() ||
         select_expr->operand().has_select_expr())) {
      // select expressions are pushed in reverse order:
      // google.type.Expr is pushed as:
      // - field: 'Expr'
      // - field: 'type'
      // - id: 'google'
      //
      // The search order though is as follows:
      // - id: 'google.type.Expr'
      // - id: 'google.type', field: 'Expr'
      // - id: 'google', field: 'type', field: 'Expr'
      for (size_t i = 0; i < namespace_stack_.size(); i++) {
        auto ns = namespace_stack_[i];
        namespace_stack_[i] = {
            ns.first, absl::StrCat(select_expr->field(), ".", ns.second)};
      }
      namespace_stack_.push_back({expr, select_expr->field()});
    } else {
      namespace_stack_.clear();
    }
  }

  // Select node handler.
  // Invoked after child nodes are processed.
  void PostVisitSelect(const Select* select_expr, const Expr* expr,
                       const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    // Check if we are "in the middle" of namespaced name.
    // This is currently enum specific. Constant expression that corresponds
    // to resolved enum value has been already created, thus preceding chain
    // of selects is no longer relevant.
    if (resolved_select_expr_) {
      if (expr == resolved_select_expr_) {
        resolved_select_expr_ = nullptr;
      }
      return;
    }

    std::string select_path = "";
    auto it = namespace_map_.find(expr);
    if (it != namespace_map_.end()) {
      select_path = it->second;
    }

    AddStep(CreateSelectStep(select_expr, expr->id(), select_path,
                             enable_wrapper_type_null_unboxing_));
  }

  // Call node handler group.
  // We provide finer granularity for Call node callbacks to allow special
  // handling for short-circuiting
  // PreVisitCall is invoked before child nodes are processed.
  void PreVisitCall(const Call* call_expr, const Expr* expr,
                    const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    std::unique_ptr<CondVisitor> cond_visitor;
    if (call_expr->function() == builtin::kAnd) {
      cond_visitor = absl::make_unique<BinaryCondVisitor>(
          this, /* cond_value= */ false, short_circuiting_);
    } else if (call_expr->function() == builtin::kOr) {
      cond_visitor = absl::make_unique<BinaryCondVisitor>(
          this, /* cond_value= */ true, short_circuiting_);
    } else if (call_expr->function() == builtin::kTernary) {
      if (short_circuiting_) {
        cond_visitor = absl::make_unique<TernaryCondVisitor>(this);
      } else {
        cond_visitor = absl::make_unique<ExhaustiveTernaryCondVisitor>(this);
      }
    } else {
      return;
    }

    if (cond_visitor) {
      cond_visitor->PreVisit(expr);
      cond_visitor_stack_.push({expr, std::move(cond_visitor)});
    }
  }

  // Invoked after all child nodes are processed.
  void PostVisitCall(const Call* call_expr, const Expr* expr,
                     const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    auto cond_visitor = FindCondVisitor(expr);
    if (cond_visitor) {
      cond_visitor->PostVisit(expr);
      cond_visitor_stack_.pop();
      return;
    }

    // Special case for "_[_]".
    if (call_expr->function() == builtin::kIndex) {
      AddStep(CreateContainerAccessStep(call_expr, expr->id()));
      return;
    }

    // Establish the search criteria for a given function.
    absl::string_view function = call_expr->function();
    bool receiver_style = call_expr->has_target();
    size_t num_args = call_expr->args_size() + (receiver_style ? 1 : 0);
    auto arguments_matcher = ArgumentsMatcher(num_args);

    // Check to see if this is a special case of add that should really be
    // treated as a list append
    if (enable_comprehension_list_append_ &&
        call_expr->function() == builtin::kAdd && call_expr->args_size() == 2 &&
        !comprehension_stack_.empty()) {
      const Comprehension* comprehension = comprehension_stack_.top();
      absl::string_view accu_var = comprehension->accu_var();
      if (comprehension->accu_init().has_list_expr() &&
          call_expr->args(0).has_ident_expr() &&
          call_expr->args(0).ident_expr().name() == accu_var) {
        const Expr& loop_step = comprehension->loop_step();
        // Macro loop_step for a map() will contain a list concat operation:
        //   accu_var + [elem]
        if (&loop_step == expr) {
          function = builtin::kRuntimeListAppend;
        }
        // Macro loop_step for a filter() will contain a ternary:
        //   filter ? result + [elem] : result
        if (loop_step.has_call_expr() &&
            loop_step.call_expr().function() == builtin::kTernary &&
            loop_step.call_expr().args_size() == 3 &&
            &(loop_step.call_expr().args(1)) == expr) {
          function = builtin::kRuntimeListAppend;
        }
      }
    }

    // First, search for lazily defined function overloads.
    // Lazy functions shadow eager functions with the same signature.
    auto lazy_overloads = resolver_.FindLazyOverloads(
        function, receiver_style, arguments_matcher, expr->id());
    if (!lazy_overloads.empty()) {
      AddStep(CreateFunctionStep(call_expr, expr->id(), lazy_overloads));
      return;
    }

    // Second, search for eagerly defined function overloads.
    auto overloads = resolver_.FindOverloads(function, receiver_style,
                                             arguments_matcher, expr->id());
    if (overloads.empty()) {
      // Create a warning that the overload could not be found. Depending on the
      // builder_warnings configuration, this could result in termination of the
      // CelExpression creation or an inspectable warning for use within runtime
      // logging.
      auto status = builder_warnings_->AddWarning(absl::InvalidArgumentError(
          "No overloads provided for FunctionStep creation"));
      if (!status.ok()) {
        SetProgressStatusError(status);
        return;
      }
    }
    AddStep(CreateFunctionStep(call_expr, expr->id(), overloads));
  }

  void PreVisitComprehension(const Comprehension* comprehension,
                             const Expr* expr, const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (!ValidateOrError(enable_comprehension_,
                         "Comprehension support is disabled")) {
      return;
    }
    const auto& accu_var = comprehension->accu_var();
    const auto& iter_var = comprehension->iter_var();
    ValidateOrError(!accu_var.empty(),
                    "Invalid comprehension: 'accu_var' must not be empty");
    ValidateOrError(!iter_var.empty(),
                    "Invalid comprehension: 'iter_var' must not be empty");
    ValidateOrError(
        accu_var != iter_var,
        "Invalid comprehension: 'accu_var' must not be the same as 'iter_var'");
    ValidateOrError(comprehension->has_accu_init(),
                    "Invalid comprehension: 'accu_init' must be set");
    ValidateOrError(comprehension->has_loop_condition(),
                    "Invalid comprehension: 'loop_condition' must be set");
    ValidateOrError(comprehension->has_loop_step(),
                    "Invalid comprehension: 'loop_step' must be set");
    ValidateOrError(comprehension->has_result(),
                    "Invalid comprehension: 'result' must be set");
    comprehension_stack_.push(comprehension);
    cond_visitor_stack_.push(
        {expr, absl::make_unique<ComprehensionVisitor>(
                   this, short_circuiting_,
                   enable_comprehension_vulnerability_check_)});
    auto cond_visitor = FindCondVisitor(expr);
    cond_visitor->PreVisit(expr);
  }

  // Invoked after all child nodes are processed.
  void PostVisitComprehension(const Comprehension* comprehension_expr,
                              const Expr* expr,
                              const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    comprehension_stack_.pop();

    auto cond_visitor = FindCondVisitor(expr);
    cond_visitor->PostVisit(expr);
    cond_visitor_stack_.pop();

    // Save off the names of the variables we're using, such that we have a
    // full set of the names from the entire evaluation tree at the end.
    if (!comprehension_expr->accu_var().empty()) {
      iter_variable_names_->insert(comprehension_expr->accu_var());
    }
    if (!comprehension_expr->iter_var().empty()) {
      iter_variable_names_->insert(comprehension_expr->iter_var());
    }
  }

  // Invoked after each argument node processed.
  void PostVisitArg(int arg_num, const Expr* expr,
                    const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    auto cond_visitor = FindCondVisitor(expr);
    if (cond_visitor) {
      cond_visitor->PostVisitArg(arg_num, expr);
    }
  }

  // Nothing to do.
  void PostVisitTarget(const Expr* expr, const SourcePosition*) override {}

  // CreateList node handler.
  // Invoked after child nodes are processed.
  void PostVisitCreateList(const CreateList* list_expr, const Expr* expr,
                           const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }
    if (enable_comprehension_list_append_ && !comprehension_stack_.empty() &&
        &(comprehension_stack_.top()->accu_init()) == expr) {
      AddStep(CreateCreateMutableListStep(list_expr, expr->id()));
      return;
    }
    AddStep(CreateCreateListStep(list_expr, expr->id()));
  }

  // CreateStruct node handler.
  // Invoked after child nodes are processed.
  void PostVisitCreateStruct(const CreateStruct* struct_expr, const Expr* expr,
                             const SourcePosition*) override {
    if (!progress_status_.ok()) {
      return;
    }

    // If the message name is empty, this signals that a map should be created.
    auto message_name = struct_expr->message_name();
    if (message_name.empty()) {
      for (const auto& entry : struct_expr->entries()) {
        ValidateOrError(entry.has_map_key(), "Map entry missing key");
        ValidateOrError(entry.has_value(), "Map entry missing value");
      }
      AddStep(CreateCreateStructStep(struct_expr, expr->id()));
      return;
    }

    // If the message name is not empty, then the message name must be resolved
    // within the container, and if a descriptor is found, then a proto message
    // creation step will be created.
    auto type_adapter = resolver_.FindTypeAdapter(message_name, expr->id());
    if (ValidateOrError(type_adapter.has_value() &&
                            type_adapter->mutation_apis() != nullptr,
                        "Invalid struct creation: missing type info for '",
                        message_name, "'")) {
      for (const auto& entry : struct_expr->entries()) {
        ValidateOrError(entry.has_field_key(),
                        "Struct entry missing field name");
        ValidateOrError(entry.has_value(), "Struct entry missing value");
      }
      AddStep(CreateCreateStructStep(struct_expr, type_adapter->mutation_apis(),
                                     expr->id()));
    }
  }

  absl::Status progress_status() const { return progress_status_; }

  void AddStep(absl::StatusOr<std::unique_ptr<ExpressionStep>> step) {
    if (step.ok() && progress_status_.ok()) {
      flattened_path_->push_back(*std::move(step));
    } else {
      SetProgressStatusError(step.status());
    }
  }

  void AddStep(std::unique_ptr<ExpressionStep> step) {
    if (progress_status_.ok()) {
      flattened_path_->push_back(std::move(step));
    }
  }

  void SetProgressStatusError(const absl::Status& status) {
    if (progress_status_.ok() && !status.ok()) {
      progress_status_ = status;
    }
  }

  // Index of the next step to be inserted.
  int GetCurrentIndex() const { return flattened_path_->size(); }

  CondVisitor* FindCondVisitor(const Expr* expr) const {
    if (cond_visitor_stack_.empty()) {
      return nullptr;
    }

    const auto& latest = cond_visitor_stack_.top();

    return (latest.first == expr) ? latest.second.get() : nullptr;
  }

  // Tests the boolean predicate, and if false produces an InvalidArgumentError
  // which concatenates the error_message and any optional message_parts as the
  // error status message.
  template <typename... MP>
  bool ValidateOrError(bool valid_expression, absl::string_view error_message,
                       MP... message_parts) {
    if (valid_expression) {
      return true;
    }
    SetProgressStatusError(absl::InvalidArgumentError(
        absl::StrCat(error_message, message_parts...)));
    return false;
  }

 private:
  const Resolver& resolver_;
  ExecutionPath* flattened_path_;
  absl::Status progress_status_;

  std::stack<std::pair<const Expr*, std::unique_ptr<CondVisitor>>>
      cond_visitor_stack_;

  // Maps effective namespace names to Expr objects (IDENTs/SELECTs) that
  // define scopes for those namespaces.
  std::unordered_map<const Expr*, std::string> namespace_map_;
  // Tracks SELECT-...SELECT-IDENT chains.
  std::deque<std::pair<const Expr*, std::string>> namespace_stack_;

  // When multiple SELECT-...SELECT-IDENT chain is resolved as namespace, this
  // field is used as marker suppressing CelExpression creation for SELECTs.
  const Expr* resolved_select_expr_;

  bool short_circuiting_;

  const absl::flat_hash_map<std::string, CelValue>& constant_idents_;

  bool enable_comprehension_;
  bool enable_comprehension_list_append_;
  std::stack<const Comprehension*> comprehension_stack_;

  bool enable_comprehension_vulnerability_check_;
  bool enable_wrapper_type_null_unboxing_;

  BuilderWarnings* builder_warnings_;

  std::set<std::string>* iter_variable_names_;
};

void BinaryCondVisitor::PreVisit(const Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args_size() == 2,
      "Invalid argument count for a binary function call.");
}

void BinaryCondVisitor::PostVisitArg(int arg_num, const Expr* expr) {
  if (!short_circuiting_) {
    // nothing to do.
    return;
  }
  if (arg_num == 0) {
    // If first branch evaluation result is enough to determine output,
    // jump over the second branch and provide result as final output.
    auto jump_step = CreateCondJumpStep(cond_value_, true, {}, expr->id());
    if (jump_step.ok()) {
      jump_step_ = Jump(visitor_->GetCurrentIndex(), jump_step->get());
    }
    visitor_->AddStep(std::move(jump_step));
  }
}

void BinaryCondVisitor::PostVisit(const Expr* expr) {
  // TODO(issues/41): shortcircuit behavior is non-obvious: should add
  // documentation and structure the code a bit better.
  visitor_->AddStep((cond_value_) ? CreateOrStep(expr->id())
                                  : CreateAndStep(expr->id()));
  if (short_circuiting_) {
    jump_step_.set_target(visitor_->GetCurrentIndex());
  }
}

void TernaryCondVisitor::PreVisit(const Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args_size() == 3,
      "Invalid argument count for a ternary function call.");
}

void TernaryCondVisitor::PostVisitArg(int arg_num, const Expr* expr) {
  // Ternary operator "_?_:_" requires a special handing.
  // In contrary to regular function call, its execution affects the control
  // flow of the overall CEL expression.
  // If condition value (argument 0) is True, then control flow is unaffected
  // as it is passed to the first conditional branch. Then, at the end of this
  // branch, the jump is performed over the second conditional branch.
  // If condition value is False, then jump is performed and control is passed
  // to the beginning of the second conditional branch.
  // If condition value is Error, then jump is peformed to bypass both
  // conditional branches and provide Error as result of ternary operation.

  // condition argument for ternary operator
  if (arg_num == 0) {
    // Jump in case of error or non-bool
    auto error_jump = CreateBoolCheckJumpStep({}, expr->id());
    if (error_jump.ok()) {
      error_jump_ = Jump(visitor_->GetCurrentIndex(), error_jump->get());
    }
    visitor_->AddStep(std::move(error_jump));

    // Jump to the second branch of execution
    // Value is to be removed from the stack.
    auto jump_to_second = CreateCondJumpStep(false, false, {}, expr->id());
    if (jump_to_second.ok()) {
      jump_to_second_ =
          Jump(visitor_->GetCurrentIndex(), jump_to_second->get());
    }
    visitor_->AddStep(std::move(jump_to_second));
  } else if (arg_num == 1) {
    // Jump after the first and over the second branch of execution.
    // Value is to be removed from the stack.
    auto jump_after_first = CreateJumpStep({}, expr->id());
    if (jump_after_first.ok()) {
      jump_after_first_ =
          Jump(visitor_->GetCurrentIndex(), jump_after_first->get());
    }
    visitor_->AddStep(std::move(jump_after_first));

    if (visitor_->ValidateOrError(
            jump_to_second_.exists(),
            "Error configuring ternary operator: jump_to_second_ is null")) {
      jump_to_second_.set_target(visitor_->GetCurrentIndex());
    }
  }
  // Code executed after traversing the final branch of execution
  // (arg_num == 2) is placed in PostVisitCall, to make this method less
  // clattered.
}

void TernaryCondVisitor::PostVisit(const Expr*) {
  // Determine and set jump offset in jump instruction.
  if (visitor_->ValidateOrError(
          error_jump_.exists(),
          "Error configuring ternary operator: error_jump_ is null")) {
    error_jump_.set_target(visitor_->GetCurrentIndex());
  }
  if (visitor_->ValidateOrError(
          jump_after_first_.exists(),
          "Error configuring ternary operator: jump_after_first_ is null")) {
    jump_after_first_.set_target(visitor_->GetCurrentIndex());
  }
}

void ExhaustiveTernaryCondVisitor::PreVisit(const Expr* expr) {
  visitor_->ValidateOrError(
      !expr->call_expr().has_target() && expr->call_expr().args_size() == 3,
      "Invalid argument count for a ternary function call.");
}

void ExhaustiveTernaryCondVisitor::PostVisit(const Expr* expr) {
  visitor_->AddStep(CreateTernaryStep(expr->id()));
}

const Expr* Int64ConstImpl(int64_t value) {
  Constant* constant = new Constant;
  constant->set_int64_value(value);
  Expr* expr = new Expr;
  expr->set_allocated_const_expr(constant);
  return expr;
}

const Expr* MinusOne() {
  static const Expr* expr = Int64ConstImpl(-1);
  return expr;
}

const Expr* LoopStepDummy() {
  static const Expr* expr = Int64ConstImpl(-1);
  return expr;
}

const Expr* CurrentValueDummy() {
  static const Expr* expr = Int64ConstImpl(-20);
  return expr;
}

// ComprehensionAccumulationReferences recursively walks an expression to count
// the locations where the given accumulation var_name is referenced.
//
// The purpose of this function is to detect cases where the accumulation
// variable might be used in hand-rolled ASTs that cause exponential memory
// consumption. The var_name is generally not accessible by CEL expression
// writers, only by macro authors. However, a hand-rolled AST makes it possible
// to misuse the accumulation variable.
//
// The algorithm for reference counting is as follows:
//
//  * Calls - If the call is a concatenation operator, sum the number of places
//            where the variable appears within the call, as this could result
//            in memory explosion if the accumulation variable type is a list
//            or string. Otherwise, return 0.
//
//            accu: ["hello"]
//            expr: accu + accu // memory grows exponentionally
//
//  * CreateList - If the accumulation var_name appears within multiple elements
//            of a CreateList call, this means that the accumulation is
//            generating an ever-expanding tree of values that will likely
//            exhaust memory.
//
//            accu: ["hello"]
//            expr: [accu, accu] // memory grows exponentially
//
//  * CreateStruct - If the accumulation var_name as an entry within the
//            creation of a map or message value, then it's possible that the
//            comprehension is accumulating an ever-expanding tree of values.
//
//            accu: {"key": "val"}
//            expr: {1: accu, 2: accu}
//
//  * Comprehension - If the accumulation var_name is not shadowed by a nested
//            iter_var or accu_var, then it may be accmulating memory within a
//            nested context. The accumulation may occur on either the
//            comprehension loop_step or result step.
//
// Since this behavior generally only occurs within hand-rolled ASTs, it is
// very reasonable to opt-in to this check only when using human authored ASTs.
int ComprehensionAccumulationReferences(const Expr& expr,
                                        absl::string_view var_name) {
  int references = 0;
  switch (expr.expr_kind_case()) {
    case Expr::kCallExpr: {
      const auto& call = expr.call_expr();
      absl::string_view function = call.function();
      // Return the maximum reference count of each side of the ternary branch.
      if (function == builtin::kTernary && call.args_size() == 3) {
        return std::max(
            ComprehensionAccumulationReferences(call.args(1), var_name),
            ComprehensionAccumulationReferences(call.args(2), var_name));
      }
      // Return the number of times the accumulator var_name appears in the add
      // expression. There's no arg size check on the add as it may become a
      // variadic add at a future date.
      if (function == builtin::kAdd) {
        for (int i = 0; i < call.args_size(); i++) {
          references +=
              ComprehensionAccumulationReferences(call.args(i), var_name);
        }
        return references;
      }
      // Return whether the accumulator var_name is used as the operand in an
      // index expression or in the identity `dyn` function.
      if ((function == builtin::kIndex && call.args_size() == 2) ||
          (function == builtin::kDyn && call.args_size() == 1)) {
        return ComprehensionAccumulationReferences(call.args(0), var_name);
      }
      return 0;
    }
    case Expr::kComprehensionExpr: {
      const auto& comprehension = expr.comprehension_expr();
      absl::string_view accu_var = comprehension.accu_var();
      absl::string_view iter_var = comprehension.iter_var();
      // Tne accumulation or iteration variable shadows the var_name and so will
      // not manipulate the target var_name in a nested comprhension scope.
      if (accu_var == var_name || iter_var == var_name) {
        return 0;
      }
      // Count the number of times the accumulator var_name within the loop_step
      // or the nested comprehension result.
      const Expr& loop_step = comprehension.loop_step();
      const Expr& result = comprehension.result();
      return std::max(ComprehensionAccumulationReferences(loop_step, var_name),
                      ComprehensionAccumulationReferences(result, var_name));
    }
    case Expr::kListExpr: {
      // Count the number of times the accumulator var_name appears within a
      // create list expression's elements.
      const auto& list = expr.list_expr();
      for (int i = 0; i < list.elements_size(); i++) {
        references +=
            ComprehensionAccumulationReferences(list.elements(i), var_name);
      }
      return references;
    }
    case Expr::kStructExpr: {
      // Count the number of times the accumulation variable occurs within
      // entry values.
      const auto& map = expr.struct_expr();
      for (int i = 0; i < map.entries_size(); i++) {
        const auto& entry = map.entries(i);
        if (entry.has_value()) {
          references +=
              ComprehensionAccumulationReferences(entry.value(), var_name);
        }
      }
      return references;
    }
    case Expr::kSelectExpr: {
      // Test only expressions have a boolean return and thus cannot easily
      // allocate large amounts of memory.
      if (expr.select_expr().test_only()) {
        return 0;
      }
      // Return whether the accumulator var_name appears within a non-test
      // select operand.
      return ComprehensionAccumulationReferences(expr.select_expr().operand(),
                                                 var_name);
    }
    case Expr::kIdentExpr:
      // Return whether the identifier name equals the accumulator var_name.
      return expr.ident_expr().name() == var_name ? 1 : 0;
    default:
      return 0;
  }
}

void ComprehensionVisitor::PreVisit(const Expr*) {
  const Expr* dummy = LoopStepDummy();
  visitor_->AddStep(CreateConstValueStep(*ConvertConstant(&dummy->const_expr()),
                                         dummy->id(), false));
}

void ComprehensionVisitor::PostVisitArg(int arg_num, const Expr* expr) {
  const Comprehension* comprehension = &expr->comprehension_expr();
  const auto& accu_var = comprehension->accu_var();
  const auto& iter_var = comprehension->iter_var();
  // TODO(issues/20): Consider refactoring the comprehension prologue step.
  switch (arg_num) {
    case ITER_RANGE: {
      // Post-process iter_range to list its keys if it's a map.
      visitor_->AddStep(CreateListKeysStep(expr->id()));
      const Expr* minus1 = MinusOne();
      visitor_->AddStep(CreateConstValueStep(
          *ConvertConstant(&minus1->const_expr()), minus1->id(), false));
      const Expr* dummy = CurrentValueDummy();
      visitor_->AddStep(CreateConstValueStep(
          *ConvertConstant(&dummy->const_expr()), dummy->id(), false));
      break;
    }
    case ACCU_INIT: {
      next_step_pos_ = visitor_->GetCurrentIndex();
      next_step_ = new ComprehensionNextStep(accu_var, iter_var, expr->id());
      visitor_->AddStep(std::unique_ptr<ExpressionStep>(next_step_));
      break;
    }
    case LOOP_CONDITION: {
      cond_step_pos_ = visitor_->GetCurrentIndex();
      cond_step_ = new ComprehensionCondStep(accu_var, iter_var,
                                             short_circuiting_, expr->id());
      visitor_->AddStep(std::unique_ptr<ExpressionStep>(cond_step_));
      break;
    }
    case LOOP_STEP: {
      auto jump_to_next = CreateJumpStep(
          next_step_pos_ - visitor_->GetCurrentIndex() - 1, expr->id());
      if (jump_to_next.ok()) {
        visitor_->AddStep(std::move(jump_to_next));
      }
      // Set offsets.
      cond_step_->set_jump_offset(visitor_->GetCurrentIndex() - cond_step_pos_ -
                                  1);
      next_step_->set_jump_offset(visitor_->GetCurrentIndex() - next_step_pos_ -
                                  1);
      break;
    }
    case RESULT: {
      visitor_->AddStep(std::unique_ptr<ExpressionStep>(
          new ComprehensionFinish(accu_var, iter_var, expr->id())));
      next_step_->set_error_jump_offset(visitor_->GetCurrentIndex() -
                                        next_step_pos_ - 1);
      cond_step_->set_error_jump_offset(visitor_->GetCurrentIndex() -
                                        cond_step_pos_ - 1);
      break;
    }
  }
}

void ComprehensionVisitor::PostVisit(const Expr* expr) {
  if (enable_vulnerability_check_) {
    const Comprehension* comprehension = &expr->comprehension_expr();
    absl::string_view accu_var = comprehension->accu_var();
    const Expr& loop_step = comprehension->loop_step();
    visitor_->ValidateOrError(
        ComprehensionAccumulationReferences(loop_step, accu_var) < 2,
        "Comprehension contains memory exhaustion vulnerability");
  }
}

}  // namespace

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpressionImpl(
    const Expr* expr, const SourceInfo* source_info,
    const google::protobuf::Map<int64_t, Reference>* reference_map,
    std::vector<absl::Status>* warnings) const {
  ExecutionPath execution_path;
  BuilderWarnings warnings_builder(fail_on_warnings_);
  Resolver resolver(container(), GetRegistry(), GetTypeRegistry(),
                    enable_qualified_type_identifiers_);

  if (absl::StartsWith(container(), ".") || absl::EndsWith(container(), ".")) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid expression container: '", container(), "'"));
  }

  absl::flat_hash_map<std::string, CelValue> idents;

  const Expr* effective_expr = expr;
  // transformed expression preserving expression IDs
  bool rewrites_enabled = enable_qualified_identifier_rewrites_ ||
                          (reference_map != nullptr && !reference_map->empty());
  std::unique_ptr<Expr> rewrite_buffer = nullptr;

  // TODO(issues/98): A type checker may perform these rewrites, but there
  // currently isn't a signal to expose that in an expression. If that becomes
  // available, we can skip the reference resolve step here if it's already
  // done.
  if (rewrites_enabled) {
    rewrite_buffer = std::make_unique<Expr>(*expr);
    absl::StatusOr<bool> rewritten =
        ResolveReferences(reference_map, resolver, source_info,
                          warnings_builder, rewrite_buffer.get());
    if (!rewritten.ok()) {
      return rewritten.status();
    }
    if (*rewritten) {
      effective_expr = rewrite_buffer.get();
    }
    // TODO(issues/99): we could setup a check step here that confirms all of
    // references are defined before actually evaluating.
  }

  Expr const_fold_buffer;
  if (constant_folding_) {
    FoldConstants(*effective_expr, *this->GetRegistry(), constant_arena_,
                  idents, &const_fold_buffer);
    effective_expr = &const_fold_buffer;
  }

  std::set<std::string> iter_variable_names;
  FlatExprVisitor visitor(resolver, &execution_path, shortcircuiting_, idents,
                          enable_comprehension_,
                          enable_comprehension_list_append_,
                          enable_comprehension_vulnerability_check_,
                          enable_wrapper_type_null_unboxing_, &warnings_builder,
                          &iter_variable_names);

  AstTraverse(effective_expr, source_info, &visitor);

  if (!visitor.progress_status().ok()) {
    return visitor.progress_status();
  }

  std::unique_ptr<CelExpression> expression_impl =
      absl::make_unique<CelExpressionFlatImpl>(
          expr, std::move(execution_path), GetTypeRegistry(),
          comprehension_max_iterations_, std::move(iter_variable_names),
          enable_unknowns_, enable_unknown_function_results_,
          enable_missing_attribute_errors_, enable_null_coercion_,
          enable_heterogeneous_equality_, std::move(rewrite_buffer));

  if (warnings != nullptr) {
    *warnings = std::move(warnings_builder).warnings();
  }
  return std::move(expression_impl);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const Expr* expr,
                                  const SourceInfo* source_info,
                                  std::vector<absl::Status>* warnings) const {
  return CreateExpressionImpl(expr, source_info, /*reference_map=*/nullptr,
                              warnings);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const Expr* expr,
                                  const SourceInfo* source_info) const {
  return CreateExpressionImpl(expr, source_info, /*reference_map=*/nullptr,
                              /*warnings=*/nullptr);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const CheckedExpr* checked_expr,
                                  std::vector<absl::Status>* warnings) const {
  return CreateExpressionImpl(&checked_expr->expr(),
                              &checked_expr->source_info(),
                              &checked_expr->reference_map(), warnings);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
FlatExprBuilder::CreateExpression(const CheckedExpr* checked_expr) const {
  return CreateExpressionImpl(&checked_expr->expr(),
                              &checked_expr->source_info(),
                              &checked_expr->reference_map(),
                              /*warnings=*/nullptr);
}

}  // namespace google::api::expr::runtime
