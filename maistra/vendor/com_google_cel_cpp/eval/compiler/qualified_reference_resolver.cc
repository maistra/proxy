#include "eval/compiler/qualified_reference_resolver.h"

#include <cstdint>
#include <functional>
#include <string>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/public/ast_rewrite.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/source_position.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::google::api::expr::v1alpha1::Constant;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::Reference;
using ::google::api::expr::v1alpha1::SourceInfo;

// Determines if function is implemented with custom evaluation step instead of
// registered.
bool IsSpecialFunction(absl::string_view function_name) {
  return function_name == builtin::kAnd || function_name == builtin::kOr ||
         function_name == builtin::kIndex || function_name == builtin::kTernary;
}

bool OverloadExists(const Resolver& resolver, absl::string_view name,
                    const std::vector<CelValue::Type>& arguments_matcher,
                    bool receiver_style = false) {
  return !resolver.FindOverloads(name, receiver_style, arguments_matcher)
              .empty() ||
         !resolver.FindLazyOverloads(name, receiver_style, arguments_matcher)
              .empty();
}

// Return the qualified name of the most qualified matching overload, or
// nullopt if no matches are found.
absl::optional<std::string> BestOverloadMatch(const Resolver& resolver,
                                              absl::string_view base_name,
                                              int argument_count) {
  if (IsSpecialFunction(base_name)) {
    return std::string(base_name);
  }
  auto arguments_matcher = ArgumentsMatcher(argument_count);
  // Check from most qualified to least qualified for a matching overload.
  auto names = resolver.FullyQualifiedNames(base_name);
  for (auto name = names.begin(); name != names.end(); ++name) {
    if (OverloadExists(resolver, *name, arguments_matcher)) {
      if (base_name[0] == '.') {
        // Preserve leading '.' to prevent re-resolving at plan time.
        return std::string(base_name);
      }
      return *name;
    }
  }
  return absl::nullopt;
}

// Rewriter visitor for resolving references.
//
// On previsit pass, replace (possibly qualified) identifier branches with the
// canonical name in the reference map (most qualified references considered
// first).
//
// On post visit pass, update function calls to determine whether the function
// target is a namespace for the function or a receiver for the call.
class ReferenceResolver : public AstRewriterBase {
 public:
  ReferenceResolver(const google::protobuf::Map<int64_t, Reference>* reference_map,
                    const Resolver& resolver, BuilderWarnings& warnings)
      : reference_map_(reference_map),
        resolver_(resolver),
        warnings_(warnings) {}

  // Attempt to resolve references in expr. Return true if part of the
  // expression was rewritten.
  // TODO(issues/95): If possible, it would be nice to write a general utility
  // for running the preprocess steps when traversing the AST instead of having
  // one pass per transform.
  bool PreVisitRewrite(Expr* expr, const SourcePosition* position) override {
    const Reference* reference = GetReferenceForId(expr->id());

    // Fold compile time constant (e.g. enum values)
    if (reference != nullptr && reference->has_value()) {
      if (reference->value().constant_kind_case() == Constant::kInt64Value) {
        // Replace enum idents with const reference value.
        expr->mutable_const_expr()->set_int64_value(
            reference->value().int64_value());
        return true;
      } else {
        // No update if the constant reference isn't an int (an enum value).
        return false;
      }
    }

    if (reference != nullptr) {
      switch (expr->expr_kind_case()) {
        case Expr::kIdentExpr:
          return MaybeUpdateIdentNode(expr, *reference);
        case Expr::kSelectExpr:
          return MaybeUpdateSelectNode(expr, *reference);
        default:
          // Call nodes are updated on post visit so they will see any select
          // path rewrites.
          return false;
      }
    }
    return false;
  }

  bool PostVisitRewrite(Expr* expr,
                        const SourcePosition* source_position) override {
    const Reference* reference = GetReferenceForId(expr->id());
    if (expr->has_call_expr()) {
      return MaybeUpdateCallNode(expr, reference);
    }
    return false;
  }

 private:
  // Attempt to update a function call node. This disambiguates
  // receiver call verses namespaced names in parse if possible.
  //
  // TODO(issues/95): This duplicates some of the overload matching behavior
  // for parsed expressions. We should refactor to consolidate the code.
  bool MaybeUpdateCallNode(Expr* out, const Reference* reference) {
    auto* call_expr = out->mutable_call_expr();
    if (reference != nullptr && reference->overload_id_size() == 0) {
      warnings_
          .AddWarning(absl::InvalidArgumentError(
              absl::StrCat("Reference map doesn't provide overloads for ",
                           out->call_expr().function())))
          .IgnoreError();
    }
    bool receiver_style = call_expr->has_target();
    int arg_num = call_expr->args_size();
    if (receiver_style) {
      auto maybe_namespace = ToNamespace(call_expr->target());
      if (maybe_namespace.has_value()) {
        std::string resolved_name =
            absl::StrCat(*maybe_namespace, ".", call_expr->function());
        auto resolved_function =
            BestOverloadMatch(resolver_, resolved_name, arg_num);
        if (resolved_function.has_value()) {
          call_expr->set_function(*resolved_function);
          call_expr->clear_target();
          return true;
        }
      }
    } else {
      // Not a receiver style function call. Check to see if it is a namespaced
      // function using a shorthand inside the expression container.
      auto maybe_resolved_function =
          BestOverloadMatch(resolver_, call_expr->function(), arg_num);
      if (!maybe_resolved_function.has_value()) {
        warnings_
            .AddWarning(absl::InvalidArgumentError(
                absl::StrCat("No overload found in reference resolve step for ",
                             call_expr->function())))
            .IgnoreError();
      } else if (maybe_resolved_function.value() != call_expr->function()) {
        call_expr->set_function(maybe_resolved_function.value());
        return true;
      }
    }
    // For parity, if we didn't rewrite the receiver call style function,
    // check that an overload is provided in the builder.
    if (call_expr->has_target() &&
        !OverloadExists(resolver_, call_expr->function(),
                        ArgumentsMatcher(arg_num + 1),
                        /* receiver_style= */ true)) {
      warnings_
          .AddWarning(absl::InvalidArgumentError(
              absl::StrCat("No overload found in reference resolve step for ",
                           call_expr->function())))
          .IgnoreError();
    }
    return false;
  }

  // Attempt to resolve a select node. If reference is valid,
  // replace the select node with the fully qualified ident node.
  bool MaybeUpdateSelectNode(Expr* out, const Reference& reference) {
    if (out->select_expr().test_only()) {
      warnings_
          .AddWarning(
              absl::InvalidArgumentError("Reference map points to a presence "
                                         "test -- has(container.attr)"))
          .IgnoreError();
    } else if (!reference.name().empty()) {
      out->mutable_ident_expr()->set_name(reference.name());
      rewritten_reference_.insert(out->id());
      return true;
    }
    return false;
  }

  // Attempt to resolve an ident node. If reference is valid,
  // replace the node with the fully qualified ident node.
  bool MaybeUpdateIdentNode(Expr* out, const Reference& reference) {
    if (!reference.name().empty() &&
        reference.name() != out->ident_expr().name()) {
      out->mutable_ident_expr()->set_name(reference.name());
      rewritten_reference_.insert(out->id());
      return true;
    }
    return false;
  }

  // Convert a select expr sub tree into a namespace name if possible.
  // If any operand of the top element is a not a select or an ident node,
  // return nullopt.
  absl::optional<std::string> ToNamespace(const Expr& expr) {
    absl::optional<std::string> maybe_parent_namespace;
    if (rewritten_reference_.find(expr.id()) != rewritten_reference_.end()) {
      // The target expr matches a reference (resolved to an ident decl).
      // This should not be treated as a function qualifier.
      return absl::nullopt;
    }
    switch (expr.expr_kind_case()) {
      case Expr::kIdentExpr:
        return expr.ident_expr().name();
      case Expr::kSelectExpr:
        if (expr.select_expr().test_only()) {
          return absl::nullopt;
        }
        maybe_parent_namespace = ToNamespace(expr.select_expr().operand());
        if (!maybe_parent_namespace.has_value()) {
          return absl::nullopt;
        }
        return absl::StrCat(*maybe_parent_namespace, ".",
                            expr.select_expr().field());
      default:
        return absl::nullopt;
    }
  }

  // Find a reference for the given expr id.
  //
  // Returns nullptr if no reference is available.
  const Reference* GetReferenceForId(int64_t expr_id) {
    if (reference_map_ == nullptr) {
      return nullptr;
    }
    auto iter = reference_map_->find(expr_id);
    if (iter == reference_map_->end()) {
      return nullptr;
    }
    return &iter->second;
  }

  const google::protobuf::Map<int64_t, Reference>* reference_map_;
  const Resolver& resolver_;
  BuilderWarnings& warnings_;
  absl::flat_hash_set<int64_t> rewritten_reference_;
};

}  // namespace

absl::StatusOr<bool> ResolveReferences(
    const google::protobuf::Map<int64_t, google::api::expr::v1alpha1::Reference>* reference_map,
    const Resolver& resolver, const SourceInfo* source_info,
    BuilderWarnings& warnings, Expr* expr) {
  ReferenceResolver ref_resolver(reference_map, resolver, warnings);

  // Rewriting interface doesn't support failing mid traverse propagate first
  // error encountered if fail fast enabled.
  bool was_rewritten = AstRewrite(expr, source_info, &ref_resolver);
  if (warnings.fail_immediately() && !warnings.warnings().empty()) {
    return warnings.warnings().front();
  }
  return was_rewritten;
}

}  // namespace google::api::expr::runtime
