#include "eval/eval/attribute_trail.h"

#include "absl/status/status.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// Creates AttributeTrail with attribute path incremented by "qualifier".
AttributeTrail AttributeTrail::Step(CelAttributeQualifier qualifier,
                                    google::protobuf::Arena* arena) const {
  // Cannot continue void trail
  if (empty()) return AttributeTrail();

  std::vector<CelAttributeQualifier> qualifiers = attribute_->qualifier_path();
  qualifiers.push_back(qualifier);
  return AttributeTrail(google::protobuf::Arena::Create<CelAttribute>(
      arena, attribute_->variable(), std::move(qualifiers)));
}

}  // namespace google::api::expr::runtime
