/* This file was generated by upbc (the upb compiler) from the input
 * file:
 *
 *     xds/type/matcher/v3/string.proto
 *
 * Do not edit -- your changes will be discarded when the file is
 * regenerated. */

#ifndef XDS_TYPE_MATCHER_V3_STRING_PROTO_UPBDEFS_H_
#define XDS_TYPE_MATCHER_V3_STRING_PROTO_UPBDEFS_H_

#include "upb/def.h"
#include "upb/port_def.inc"
#ifdef __cplusplus
extern "C" {
#endif

#include "upb/def.h"

#include "upb/port_def.inc"

extern _upb_DefPool_Init xds_type_matcher_v3_string_proto_upbdefinit;

UPB_INLINE const upb_MessageDef *xds_type_matcher_v3_StringMatcher_getmsgdef(upb_DefPool *s) {
  _upb_DefPool_LoadDefInit(s, &xds_type_matcher_v3_string_proto_upbdefinit);
  return upb_DefPool_FindMessageByName(s, "xds.type.matcher.v3.StringMatcher");
}

UPB_INLINE const upb_MessageDef *xds_type_matcher_v3_ListStringMatcher_getmsgdef(upb_DefPool *s) {
  _upb_DefPool_LoadDefInit(s, &xds_type_matcher_v3_string_proto_upbdefinit);
  return upb_DefPool_FindMessageByName(s, "xds.type.matcher.v3.ListStringMatcher");
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#include "upb/port_undef.inc"

#endif  /* XDS_TYPE_MATCHER_V3_STRING_PROTO_UPBDEFS_H_ */
