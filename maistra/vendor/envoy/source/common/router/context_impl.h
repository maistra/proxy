#pragma once

#include "envoy/router/context.h"
#include "envoy/router/router.h"
#include "envoy/stats/stats_macros.h"

namespace Envoy {
namespace Router {

/**
 * All router filter stats. @see stats_macros.h
 */
#define ALL_ROUTER_STATS(COUNTER, GAUGE, HISTOGRAM, TEXT_READOUT, STATNAME)                        \
  COUNTER(no_cluster)                                                                              \
  COUNTER(no_route)                                                                                \
  COUNTER(passthrough_internal_redirect_bad_location)                                              \
  COUNTER(passthrough_internal_redirect_no_route)                                                  \
  COUNTER(passthrough_internal_redirect_predicate)                                                 \
  COUNTER(passthrough_internal_redirect_too_many_redirects)                                        \
  COUNTER(passthrough_internal_redirect_unsafe_scheme)                                             \
  COUNTER(rq_direct_response)                                                                      \
  COUNTER(rq_redirect)                                                                             \
  COUNTER(rq_reset_after_downstream_response_started)                                              \
  COUNTER(rq_total)                                                                                \
  STATNAME(retry)

MAKE_STAT_NAMES_STRUCT(StatNames, ALL_ROUTER_STATS);

/**
 * Captures router-related structures with cardinality of one per server.
 */
class ContextImpl : public Context {
public:
  explicit ContextImpl(Stats::SymbolTable& symbol_table);
  ~ContextImpl() override = default;

  const StatNames& statNames() const override { return stat_names_; }
  const VirtualClusterStatNames& virtualClusterStatNames() const override {
    return virtual_cluster_stat_names_;
  }

private:
  const StatNames stat_names_;
  const VirtualClusterStatNames virtual_cluster_stat_names_;
};

} // namespace Router
} // namespace Envoy
