/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include "extensions/stats/plugin.h"

#include <iterator>

#include "absl/strings/ascii.h"
#include "absl/time/time.h"
#include "extensions/common/util.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "contrib/proxy_expr.h"
#include "proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {

#include "contrib/proxy_expr.h"

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Stats {

const uint32_t kDefaultTCPReportDurationMilliseconds = 15000;  // 15s

using ::nlohmann::json;
using ::Wasm::Common::GetFromFbStringView;
using ::Wasm::Common::JsonArrayIterate;
using ::Wasm::Common::JsonGetField;
using ::Wasm::Common::JsonObjectIterate;
using ::Wasm::Common::JsonValueAs;
using ::Wasm::Common::Protocol;

namespace {

void map_node(IstioDimensions& instance, bool is_source,
              const ::Wasm::Common::FlatNode& node) {
  // Ensure all properties are set (and cleared when necessary).
  if (is_source) {
    instance[source_workload] = GetFromFbStringView(node.workload_name());
    instance[source_workload_namespace] =
        GetFromFbStringView(node.namespace_());
    instance[source_cluster] = GetFromFbStringView(node.cluster_id());

    auto source_labels = node.labels();
    if (source_labels) {
      auto app_iter = source_labels->LookupByKey("app");
      auto app = app_iter ? app_iter->value() : nullptr;
      instance[source_app] = GetFromFbStringView(app);

      auto version_iter = source_labels->LookupByKey("version");
      auto version = version_iter ? version_iter->value() : nullptr;
      instance[source_version] = GetFromFbStringView(version);

      auto canonical_name = source_labels->LookupByKey(
          ::Wasm::Common::kCanonicalServiceLabelName.data());
      auto name =
          canonical_name ? canonical_name->value() : node.workload_name();
      instance[source_canonical_service] = GetFromFbStringView(name);

      auto rev = source_labels->LookupByKey(
          ::Wasm::Common::kCanonicalServiceRevisionLabelName.data());
      if (rev) {
        instance[source_canonical_revision] = GetFromFbStringView(rev->value());
      } else {
        instance[source_canonical_revision] = ::Wasm::Common::kLatest.data();
      }
    } else {
      instance[source_app] = "";
      instance[source_version] = "";
      instance[source_canonical_service] = "";
      instance[source_canonical_revision] = ::Wasm::Common::kLatest.data();
    }
  } else {
    instance[destination_workload] = GetFromFbStringView(node.workload_name());
    instance[destination_workload_namespace] =
        GetFromFbStringView(node.namespace_());
    instance[destination_cluster] = GetFromFbStringView(node.cluster_id());

    auto destination_labels = node.labels();
    if (destination_labels) {
      auto app_iter = destination_labels->LookupByKey("app");
      auto app = app_iter ? app_iter->value() : nullptr;
      instance[destination_app] = GetFromFbStringView(app);

      auto version_iter = destination_labels->LookupByKey("version");
      auto version = version_iter ? version_iter->value() : nullptr;
      instance[destination_version] = GetFromFbStringView(version);

      auto canonical_name = destination_labels->LookupByKey(
          ::Wasm::Common::kCanonicalServiceLabelName.data());
      auto name =
          canonical_name ? canonical_name->value() : node.workload_name();
      instance[destination_canonical_service] = GetFromFbStringView(name);

      auto rev = destination_labels->LookupByKey(
          ::Wasm::Common::kCanonicalServiceRevisionLabelName.data());
      if (rev) {
        instance[destination_canonical_revision] =
            GetFromFbStringView(rev->value());
      } else {
        instance[destination_canonical_revision] =
            ::Wasm::Common::kLatest.data();
      }
    } else {
      instance[destination_app] = "";
      instance[destination_version] = "";
      instance[destination_canonical_service] = "";
      instance[destination_canonical_revision] = ::Wasm::Common::kLatest.data();
    }

    instance[destination_service_namespace] =
        GetFromFbStringView(node.namespace_());
  }
}

// Called during request processing.
void map_peer(IstioDimensions& instance, bool outbound,
              const ::Wasm::Common::FlatNode& peer_node) {
  map_node(instance, !outbound, peer_node);
}

void map_unknown_if_empty(IstioDimensions& instance) {
#define SET_IF_EMPTY(name)      \
  if (instance[name].empty()) { \
    instance[name] = unknown;   \
  }
  STD_ISTIO_DIMENSIONS(SET_IF_EMPTY)
#undef SET_IF_EMPTY
}

// maps from request context to dimensions.
// local node derived dimensions are already filled in.
void map_request(IstioDimensions& instance,
                 const ::Wasm::Common::RequestInfo& request) {
  instance[source_principal] = request.source_principal;
  instance[destination_principal] = request.destination_principal;
  instance[destination_service] = request.destination_service_host;
  instance[destination_service_name] = request.destination_service_name;
  instance[request_protocol] =
      ::Wasm::Common::ProtocolString(request.request_protocol);
  instance[response_code] = std::to_string(request.response_code);
  instance[response_flags] = request.response_flag;
  instance[connection_security_policy] = absl::AsciiStrToLower(std::string(
      ::Wasm::Common::AuthenticationPolicyString(request.service_auth_policy)));
}

// maps peer_node and request to dimensions.
void map(IstioDimensions& instance, bool outbound,
         const ::Wasm::Common::FlatNode& peer_node,
         const ::Wasm::Common::RequestInfo& request) {
  map_peer(instance, outbound, peer_node);
  map_request(instance, request);
  map_unknown_if_empty(instance);
  if (request.request_protocol == Protocol::GRPC) {
    instance[grpc_response_status] = std::to_string(request.grpc_status);
  } else {
    instance[grpc_response_status] = "";
  }
}

}  // namespace

// Ordered dimension list is used by the metrics API.
const std::vector<MetricTag>& PluginRootContext::defaultTags() {
  static const std::vector<MetricTag> default_tags = {
#define DEFINE_METRIC_TAG(name) {#name, MetricTag::TagType::String},
      STD_ISTIO_DIMENSIONS(DEFINE_METRIC_TAG)
#undef DEFINE_METRIC_TAG
  };
  return default_tags;
}

const std::vector<MetricFactory>& PluginRootContext::defaultMetrics() {
  static const std::vector<MetricFactory> default_metrics = {
      // HTTP, HTTP/2, and GRPC metrics
      MetricFactory{"requests_total", MetricType::Counter,
                    [](::Wasm::Common::RequestInfo&) -> uint64_t { return 1; },
                    static_cast<uint32_t>(Protocol::HTTP) |
                        static_cast<uint32_t>(Protocol::GRPC),
                    count_standard_labels, /* recurrent */ false},
      MetricFactory{"request_duration_milliseconds", MetricType::Histogram,
                    [](::Wasm::Common::RequestInfo& request_info) -> uint64_t {
                      return request_info.duration /* in nanoseconds */ /
                             1000000;
                    },
                    static_cast<uint32_t>(Protocol::HTTP) |
                        static_cast<uint32_t>(Protocol::GRPC),
                    count_standard_labels, /* recurrent */ false},
      MetricFactory{"request_bytes", MetricType::Histogram,
                    [](::Wasm::Common::RequestInfo& request_info) -> uint64_t {
                      return request_info.request_size;
                    },
                    static_cast<uint32_t>(Protocol::HTTP) |
                        static_cast<uint32_t>(Protocol::GRPC),
                    count_standard_labels, /* recurrent */ false},
      MetricFactory{"response_bytes", MetricType::Histogram,
                    [](::Wasm::Common::RequestInfo& request_info) -> uint64_t {
                      return request_info.response_size;
                    },
                    static_cast<uint32_t>(Protocol::HTTP) |
                        static_cast<uint32_t>(Protocol::GRPC),
                    count_standard_labels, /* recurrent */ false},

      // GRPC streaming metrics.
      // These metrics are dimensioned by peer labels as a minimum.
      // TODO: consider adding connection security policy
      MetricFactory{"request_messages_total", MetricType::Counter,
                    [](::Wasm::Common::RequestInfo& request_info) -> uint64_t {
                      uint64_t out = request_info.request_message_count -
                                     request_info.last_request_message_count;
                      request_info.last_request_message_count =
                          request_info.request_message_count;
                      return out;
                    },
                    static_cast<uint32_t>(Protocol::GRPC), count_peer_labels,
                    /* recurrent */ true},
      MetricFactory{"response_messages_total", MetricType::Counter,
                    [](::Wasm::Common::RequestInfo& request_info) -> uint64_t {
                      uint64_t out = request_info.response_message_count -
                                     request_info.last_response_message_count;
                      request_info.last_response_message_count =
                          request_info.response_message_count;
                      return out;
                    },
                    static_cast<uint32_t>(Protocol::GRPC), count_peer_labels,
                    /* recurrent */ true},

      // TCP metrics.
      MetricFactory{"tcp_sent_bytes_total", MetricType::Counter,
                    [](::Wasm::Common::RequestInfo& request_info) -> uint64_t {
                      uint64_t out = 0;
                      std::swap(out, request_info.tcp_sent_bytes);
                      return out;
                    },
                    static_cast<uint32_t>(Protocol::TCP), count_tcp_labels,
                    /* recurrent */ true},
      MetricFactory{"tcp_received_bytes_total", MetricType::Counter,
                    [](::Wasm::Common::RequestInfo& request_info) -> uint64_t {
                      uint64_t out = 0;
                      std::swap(out, request_info.tcp_received_bytes);
                      return out;
                    },
                    static_cast<uint32_t>(Protocol::TCP), count_tcp_labels,
                    /* recurrent */ true},
      MetricFactory{"tcp_connections_opened_total", MetricType::Counter,
                    [](::Wasm::Common::RequestInfo& request_info) -> uint64_t {
                      uint8_t out = 0;
                      std::swap(out, request_info.tcp_connections_opened);
                      return out;
                    },
                    static_cast<uint32_t>(Protocol::TCP), count_tcp_labels,
                    /* recurrent */ true},
      MetricFactory{"tcp_connections_closed_total", MetricType::Counter,
                    [](::Wasm::Common::RequestInfo& request_info) -> uint64_t {
                      return request_info.tcp_connections_closed;
                    },
                    static_cast<uint32_t>(Protocol::TCP), count_tcp_labels,
                    /* recurrent */ false},
  };
  return default_metrics;
}

bool PluginRootContext::initializeDimensions(const json& j) {
  // Clean-up existing expressions.
  cleanupExpressions();

  // Maps metric factory name to a factory instance
  Map<std::string, MetricFactory> factories;
  // Maps metric factory name to a list of tags.
  Map<std::string, std::vector<MetricTag>> metric_tags;
  // Maps metric factory name to a map from a tag name to an optional index.
  // Empty index means the tag needs to be removed.
  Map<std::string, Map<std::string, std::optional<size_t>>> metric_indexes;

  // Seed the common metric tags with the default set.
  const std::vector<MetricTag>& default_tags = defaultTags();
  for (const auto& factory : defaultMetrics()) {
    factories[factory.name] = factory;
    metric_tags[factory.name] = std::vector<MetricTag>(
        default_tags.begin(), default_tags.begin() + factory.count_labels);
    for (size_t i = 0; i < factory.count_labels; i++) {
      metric_indexes[factory.name][default_tags[i].name] = i;
    }
  }

  // Process the metric definitions (overriding existing).
  if (!JsonArrayIterate(j, "definitions", [&](const json& definition) -> bool {
        auto name = JsonGetField<std::string>(definition, "name").value_or("");
        auto value =
            JsonGetField<std::string>(definition, "value").value_or("");
        if (name.empty() || value.empty()) {
          LOG_WARN("empty name or value in  'definitions'");
          return false;
        }
        auto token = addIntExpression(value);
        if (!token.has_value()) {
          LOG_WARN(absl::StrCat("failed to construct expression: ", value));
          return false;
        }
        auto& factory = factories[name];
        factory.name = name;
        factory.extractor = [token, name,
                             value](::Wasm::Common::RequestInfo&) -> uint64_t {
          int64_t result = 0;
          if (!evaluateExpression(token.value(), &result)) {
            LOG_TRACE(absl::StrCat("Failed to evaluate expression: <", value,
                                   "> for dimension:<", name, ">"));
          }
          return result;
        };
        factory.type = MetricType::Counter;
        factory.recurrent = false;
        factory.protocols = static_cast<uint32_t>(Protocol::HTTP) |
                            static_cast<uint32_t>(Protocol::GRPC);
        auto type =
            JsonGetField<std::string_view>(definition, "type").value_or("");
        if (type == "GAUGE") {
          factory.type = MetricType::Gauge;
        } else if (type == "HISTOGRAM") {
          factory.type = MetricType::Histogram;
        }
        return true;
      })) {
    LOG_WARN("failed to parse 'definitions'");
  }

  // Process the dimension overrides.
  if (!JsonArrayIterate(j, "metrics", [&](const json& metric) -> bool {
        // Sort tag override tags to keep the order of tags deterministic.
        std::vector<std::string> tags;
        if (!JsonObjectIterate(metric, "dimensions",
                               [&](std::string dim) -> bool {
                                 tags.push_back(dim);
                                 return true;
                               })) {
          LOG_WARN("failed to parse 'metric.dimensions'");
          return false;
        }
        std::sort(tags.begin(), tags.end());

        auto name = JsonGetField<std::string>(metric, "name").value_or("");
        for (auto factory_it = factories.begin();
             factory_it != factories.end();) { /*do not advance iterator here*/
          if (!name.empty() && name != factory_it->first) {
            std::advance(factory_it, 1);
            continue;
          }

          bool drop = JsonGetField<bool>(metric, "drop").value_or(false);
          if (drop) {
            factory_it = factories.erase(factory_it);
            continue;
          }

          auto& indexes = metric_indexes[factory_it->first];

          // Process tag deletions.
          if (!JsonArrayIterate(
                  metric, "tags_to_remove", [&](const json& tag) -> bool {
                    auto tag_string = JsonValueAs<std::string>(tag);
                    if (tag_string.second !=
                        Wasm::Common::JsonParserResultDetail::OK) {
                      LOG_WARN(
                          absl::StrCat("unexpected tag to remove", tag.dump()));
                      return false;
                    }
                    auto it = indexes.find(tag_string.first.value());
                    if (it != indexes.end()) {
                      it->second = {};
                    }
                    return true;
                  })) {
            LOG_WARN("failed to parse 'tags_to_remove'");
            return false;
          }

          // Process tag overrides.
          for (const auto& tag : tags) {
            auto expr_string =
                JsonValueAs<std::string>(metric["dimensions"][tag]);
            if (expr_string.second !=
                Wasm::Common::JsonParserResultDetail::OK) {
              LOG_WARN("failed to parse 'dimensions' value");
              return false;
            }
            auto expr_index = addStringExpression(expr_string.first.value());
            std::optional<size_t> value = {};
            if (expr_index.has_value()) {
              value = count_standard_labels + expr_index.value();
            }
            auto it = indexes.find(tag);
            if (it != indexes.end()) {
              it->second = value;
            } else {
              metric_tags[factory_it->first].push_back(
                  {tag, MetricTag::TagType::String});
              indexes[tag] = value;
            }
          }
          std::advance(factory_it, 1);
        }
        return true;
      })) {
    LOG_WARN("failed to parse 'metrics'");
  }

  // Local data does not change, so populate it on config load.
  istio_dimensions_.resize(count_standard_labels + expressions_.size());
  istio_dimensions_[reporter] = outbound_ ? source : destination;

  const auto& local_node =
      *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(local_node_info_.data());
  map_node(istio_dimensions_, outbound_, local_node);

  // Instantiate stat factories using the new dimensions
  auto field_separator = JsonGetField<std::string>(j, "field_separator")
                             .value_or(default_field_separator);
  auto value_separator = JsonGetField<std::string>(j, "value_separator")
                             .value_or(default_value_separator);

  // Note that stat prefix is hard-coded here, because registration must be done
  // in the main thread at start-up.
  auto stat_prefix = absl::StrCat(default_stat_prefix, "_");

  stats_ = std::vector<StatGen>();
  std::vector<MetricTag> tags;
  std::vector<size_t> indexes;
  for (const auto& factory_it : factories) {
    tags.clear();
    indexes.clear();
    size_t size = metric_tags[factory_it.first].size();
    tags.reserve(size);
    indexes.reserve(size);
    for (const auto& tag : metric_tags[factory_it.first]) {
      auto index = metric_indexes[factory_it.first][tag.name];
      if (index.has_value()) {
        tags.push_back(tag);
        indexes.push_back(index.value());
      }
    }
    stats_.emplace_back(stat_prefix, factory_it.second, tags, indexes,
                        field_separator, value_separator);
  }

  Metric build(MetricType::Gauge, absl::StrCat(stat_prefix, "build"),
               {MetricTag{"component", MetricTag::TagType::String},
                MetricTag{"tag", MetricTag::TagType::String}});
  std::string istio_version =
      flatbuffers::GetString(local_node.istio_version());
  istio_version = (istio_version == "") ? absl::StrCat(unknown, ";")
                                        : absl::StrCat(istio_version, ";");
  build.record(1, "proxy", istio_version);
  return true;
}

// onConfigure == false makes the proxy crash.
// Only policy plugins should return false.
bool PluginRootContext::onConfigure(size_t size) {
  initialized_ = configure(size);
  return true;
}

bool PluginRootContext::configure(size_t configuration_size) {
  auto configuration_data = getBufferBytes(WasmBufferType::PluginConfiguration,
                                           0, configuration_size);
  local_node_info_ = ::Wasm::Common::extractLocalNodeFlatBuffer();

  auto result = ::Wasm::Common::JsonParse(configuration_data->view());
  if (!result.has_value()) {
    LOG_WARN(absl::StrCat(
        "cannot parse plugin configuration JSON string: ",
        ::Wasm::Common::toAbslStringView(configuration_data->view())));
    return false;
  }

  auto j = result.value();
  use_host_header_fallback_ =
      !JsonGetField<bool>(j, "disable_host_header_fallback").value_or(false);

  if (!initializeDimensions(j)) {
    return false;
  }

  auto mode = JsonGetField<std::string_view>(j, "metadata_mode").value_or("");
  if (mode == "UPSTREAM_HOST_METADATA_MODE") {
    metadata_mode_ = MetadataMode::kHostMetadataMode;
  } else if (mode == "CLUSTER_METADATA_MODE") {
    metadata_mode_ = MetadataMode::kClusterMetadataMode;
  } else {
    metadata_mode_ = MetadataMode::kLocalNodeMetadataMode;
  }

  // TODO: rename to reporting_duration
  uint32_t tcp_report_duration_milis = kDefaultTCPReportDurationMilliseconds;
  auto tcp_reporting_duration_field =
      JsonGetField<std::string>(j, "tcp_reporting_duration");
  absl::Duration duration;
  if (tcp_reporting_duration_field.detail() ==
      ::Wasm::Common::JsonParserResultDetail::OK) {
    if (absl::ParseDuration(tcp_reporting_duration_field.value(), &duration)) {
      tcp_report_duration_milis = uint32_t(duration / absl::Milliseconds(1));
    } else {
      LOG_WARN(absl::StrCat("failed to parse 'tcp_reporting_duration': ",
                            tcp_reporting_duration_field.value()));
    }
  }
  proxy_set_tick_period_milliseconds(tcp_report_duration_milis);

  return true;
}

void PluginRootContext::cleanupExpressions() {
  for (const auto& expression : expressions_) {
    exprDelete(expression.token);
  }
  expressions_.clear();
  input_expressions_.clear();
  for (uint32_t token : int_expressions_) {
    exprDelete(token);
  }
  int_expressions_.clear();
}

std::optional<size_t> PluginRootContext::addStringExpression(
    const std::string& input) {
  auto it = input_expressions_.find(input);
  if (it == input_expressions_.end()) {
    uint32_t token = 0;
    if (createExpression(input, &token) != WasmResult::Ok) {
      LOG_WARN(absl::StrCat("cannot create an expression: " + input));
      return {};
    }
    size_t result = expressions_.size();
    input_expressions_[input] = result;
    expressions_.push_back({token, input});
    return result;
  }
  return it->second;
}

std::optional<uint32_t> PluginRootContext::addIntExpression(
    const std::string& input) {
  uint32_t token = 0;
  if (createExpression(input, &token) != WasmResult::Ok) {
    LOG_WARN(absl::StrCat("cannot create a value expression: " + input));
    return {};
  }
  int_expressions_.push_back(token);
  return token;
}

bool PluginRootContext::onDone() {
  cleanupExpressions();
  if (!request_queue_.empty()) {
    LOG_CRITICAL(absl::StrCat("Request queue is not empty, dropping requests: ",
                              request_queue_.size()));
  }
  return true;
}

void PluginRootContext::onTick() {
  if (request_queue_.empty()) {
    return;
  }
  for (auto const& item : request_queue_) {
    // requestinfo is null, so continue.
    if (item.second == nullptr) {
      continue;
    }
    Context* context = getContext(item.first);
    if (context == nullptr) {
      continue;
    }
    context->setEffectiveContext();
    report(*item.second, false);
  }
}

void PluginRootContext::report(::Wasm::Common::RequestInfo& request_info,
                               bool end_stream) {
  if (!initialized_) {
    LOG_TRACE("stats plugin not initialized properly (wrong json config?)");
    return;
  }

  // HTTP peer metadata should be done by the time report is called for a
  // request info. TCP metadata might still be awaiting.
  // Upstream host should be selected for metadata fallback.
  Wasm::Common::PeerNodeInfo peer_node_info(peer_metadata_id_key_,
                                            peer_metadata_key_);
  if (request_info.request_protocol == Protocol::TCP) {
    // For TCP, if peer metadata is not available, peer id is set as not found.
    // Otherwise, we wait for metadata exchange to happen before we report any
    // metric, until the end.
    if (peer_node_info.maybeWaiting() && !end_stream) {
      return;
    }
    ::Wasm::Common::populateTCPRequestInfo(outbound_, &request_info);
  } else {
    // Populate HTTP request info fully only at the end of the stream because
    // onTick context has no access to request/response headers but can read
    // from filter state.
    if (end_stream) {
      ::Wasm::Common::populateHTTPRequestInfo(
          outbound_, useHostHeaderFallback(), &request_info);
    } else {
      ::Wasm::Common::populateRequestInfo(outbound_, useHostHeaderFallback(),
                                          &request_info);
      if (request_info.request_protocol == Protocol::GRPC) {
        ::Wasm::Common::populateGRPCInfo(&request_info);
      }
    }
  }

  // handle server-side (inbound) waypoint proxies specially
  if (!outbound_ && (metadata_mode_ == MetadataMode::kHostMetadataMode ||
                     metadata_mode_ == MetadataMode::kClusterMetadataMode)) {
    // in waypoint proxy or ztunnel Server mode, we must remap the "local" node
    // info per request as the proxy is no longer serving a single workload
    auto detached = Wasm::Common::extractEmptyNodeFlatBuffer();

    flatbuffers::FlatBufferBuilder fbb;
    if (metadata_mode_ == MetadataMode::kHostMetadataMode) {
      if (Wasm::Common::extractPeerMetadataFromUpstreamHostMetadata(fbb)) {
        detached = fbb.Release();
      }
    } else {
      if (Wasm::Common::extractPeerMetadataFromUpstreamClusterMetadata(fbb)) {
        detached = fbb.Release();
      }
    }

    const auto& node =
        *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(detached.data());
    map_node(istio_dimensions_, false, node);
  }

  map(istio_dimensions_, outbound_, peer_node_info.get(), request_info);

  for (size_t i = 0; i < expressions_.size(); i++) {
    if (!evaluateExpression(expressions_[i].token,
                            &istio_dimensions_.at(count_standard_labels + i))) {
      LOG_TRACE(absl::StrCat("Failed to evaluate expression: <",
                             expressions_[i].expression, ">"));
      istio_dimensions_[count_standard_labels + i] = "unknown";
    }
  }

  auto stats_it = metrics_.find(istio_dimensions_);
  if (stats_it != metrics_.end()) {
    for (auto& stat : stats_it->second) {
      if (end_stream || stat.recurrent_) {
        stat.record(request_info);
      }
      LOG_DEBUG(
          absl::StrCat("metricKey cache hit ", ", stat=", stat.metric_id_));
    }
    cache_hits_accumulator_++;
    if (cache_hits_accumulator_ == 100) {
      incrementMetric(cache_hits_, cache_hits_accumulator_);
      cache_hits_accumulator_ = 0;
    }
    return;
  }

  std::vector<SimpleStat> stats;
  for (auto& statgen : stats_) {
    if (!statgen.matchesProtocol(request_info.request_protocol)) {
      continue;
    }
    auto stat = statgen.resolve(istio_dimensions_);
    LOG_DEBUG(absl::StrCat("metricKey cache miss ",
                           ::Wasm::Common::toAbslStringView(statgen.name()),
                           " ", ", stat=", stat.metric_id_,
                           ", recurrent=", stat.recurrent_));
    if (end_stream || stat.recurrent_) {
      stat.record(request_info);
    }
    stats.push_back(stat);
  }

  incrementMetric(cache_misses_, 1);
  metrics_.try_emplace(istio_dimensions_, stats);
}

void PluginRootContext::addToRequestQueue(
    uint32_t context_id, ::Wasm::Common::RequestInfo* request_info) {
  request_queue_[context_id] = request_info;
}

void PluginRootContext::deleteFromRequestQueue(uint32_t context_id) {
  request_queue_.erase(context_id);
}

#ifdef NULL_PLUGIN
NullPluginRegistry* context_registry_{};

RegisterNullVmPluginFactory register_stats_filter("envoy.wasm.stats", []() {
  return std::make_unique<NullPlugin>(context_registry_);
});

#endif

}  // namespace Stats

#ifdef NULL_PLUGIN
// WASM_EPILOG
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
