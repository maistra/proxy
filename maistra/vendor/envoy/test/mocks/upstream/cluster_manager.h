#pragma once

#include "envoy/upstream/cluster_manager.h"

#include "test/mocks/config/mocks.h"
#include "test/mocks/grpc/mocks.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/tcp/mocks.h"

#include "cluster_manager_factory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "thread_local_cluster.h"

namespace Envoy {
namespace Upstream {
using ::testing::NiceMock;
class MockClusterManager : public ClusterManager {
public:
  explicit MockClusterManager(TimeSource& time_source);
  MockClusterManager();
  ~MockClusterManager() override;

  ClusterUpdateCallbacksHandlePtr
  addThreadLocalClusterUpdateCallbacks(ClusterUpdateCallbacks& callbacks) override {
    return ClusterUpdateCallbacksHandlePtr{addThreadLocalClusterUpdateCallbacks_(callbacks)};
  }

  ClusterManagerFactory& clusterManagerFactory() override { return cluster_manager_factory_; }

  void initializeClusters(const std::vector<std::string>& active_cluster_names,
                          const std::vector<std::string>& warming_cluster_names);

  void initializeThreadLocalClusters(const std::vector<std::string>& cluster_names);

  // Upstream::ClusterManager
  MOCK_METHOD(bool, addOrUpdateCluster,
              (const envoy::config::cluster::v3::Cluster& cluster,
               const std::string& version_info));
  MOCK_METHOD(void, setPrimaryClustersInitializedCb, (PrimaryClustersReadyCallback));
  MOCK_METHOD(void, setInitializedCb, (InitializationCompleteCallback));
  MOCK_METHOD(void, initializeSecondaryClusters,
              (const envoy::config::bootstrap::v3::Bootstrap& bootstrap));
  MOCK_METHOD(ClusterInfoMaps, clusters, ());

  MOCK_METHOD(const ClusterSet&, primaryClusters, ());
  MOCK_METHOD(ThreadLocalCluster*, getThreadLocalCluster, (absl::string_view cluster));
  MOCK_METHOD(bool, removeCluster, (const std::string& cluster));
  MOCK_METHOD(void, shutdown, ());
  MOCK_METHOD(const envoy::config::core::v3::BindConfig&, bindConfig, (), (const));
  MOCK_METHOD(Config::GrpcMuxSharedPtr, adsMux, ());
  MOCK_METHOD(Grpc::AsyncClientManager&, grpcAsyncClientManager, ());
  MOCK_METHOD(const std::string, versionInfo, (), (const));
  MOCK_METHOD(const absl::optional<std::string>&, localClusterName, (), (const));
  MOCK_METHOD(ClusterUpdateCallbacksHandle*, addThreadLocalClusterUpdateCallbacks_,
              (ClusterUpdateCallbacks & callbacks));
  MOCK_METHOD(Config::SubscriptionFactory&, subscriptionFactory, ());
  const ClusterStatNames& clusterStatNames() const override { return cluster_stat_names_; }
  const ClusterLoadReportStatNames& clusterLoadReportStatNames() const override {
    return cluster_load_report_stat_names_;
  }
  const ClusterCircuitBreakersStatNames& clusterCircuitBreakersStatNames() const override {
    return cluster_circuit_breakers_stat_names_;
  }
  const ClusterRequestResponseSizeStatNames& clusterRequestResponseSizeStatNames() const override {
    return cluster_request_response_size_stat_names_;
  }
  const ClusterTimeoutBudgetStatNames& clusterTimeoutBudgetStatNames() const override {
    return cluster_timeout_budget_stat_names_;
  }
  MOCK_METHOD(void, drainConnections, (const std::string& cluster));
  MOCK_METHOD(void, drainConnections, ());
  MOCK_METHOD(void, checkActiveStaticCluster, (const std::string& cluster));

  NiceMock<MockThreadLocalCluster> thread_local_cluster_;
  envoy::config::core::v3::BindConfig bind_config_;
  std::shared_ptr<NiceMock<Config::MockGrpcMux>> ads_mux_;
  NiceMock<Grpc::MockAsyncClientManager> async_client_manager_;
  absl::optional<std::string> local_cluster_name_;
  NiceMock<MockClusterManagerFactory> cluster_manager_factory_;
  NiceMock<Config::MockSubscriptionFactory> subscription_factory_;
  absl::flat_hash_map<std::string, std::unique_ptr<MockCluster>> active_clusters_;
  absl::flat_hash_map<std::string, std::unique_ptr<MockCluster>> warming_clusters_;
  Stats::TestUtil::TestSymbolTable symbol_table_;
  ClusterStatNames cluster_stat_names_;
  ClusterLoadReportStatNames cluster_load_report_stat_names_;
  ClusterCircuitBreakersStatNames cluster_circuit_breakers_stat_names_;
  ClusterRequestResponseSizeStatNames cluster_request_response_size_stat_names_;
  ClusterTimeoutBudgetStatNames cluster_timeout_budget_stat_names_;
};
} // namespace Upstream

} // namespace Envoy
