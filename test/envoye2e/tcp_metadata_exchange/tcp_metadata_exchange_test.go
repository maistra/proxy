// Copyright 2019 Istio Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package client_test

import (
	"testing"
	"time"

	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
)

var Runtimes = []struct {
	WasmRuntime string
}{
	{
		WasmRuntime: "envoy.wasm.runtime.null",
	},
	{
		// native
	},
}

func TestTCPMetadataExchange(t *testing.T) {
	for _, runtime := range Runtimes {
		t.Run(runtime.WasmRuntime, func(t *testing.T) {
			params := driver.NewTestParams(t, map[string]string{
				"DisableDirectResponse": "true",
				"AlpnProtocol":          "mx-protocol",
				"StatsConfig":           driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
			}, envoye2e.ProxyE2ETests)
			params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
			params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
			params.Vars["ServerNetworkFilters"] = params.LoadTestData("testdata/filters/server_mx_network_filter.yaml.tmpl") + "\n" +
				params.LoadTestData("testdata/filters/server_stats_network_filter.yaml.tmpl")
			params.Vars["ClientUpstreamFilters"] = params.LoadTestData("testdata/filters/client_mx_network_filter.yaml.tmpl")
			params.Vars["ClientNetworkFilters"] = params.LoadTestData("testdata/filters/client_stats_network_filter.yaml.tmpl")
			params.Vars["ClientClusterTLSContext"] = params.LoadTestData("testdata/transport_socket/client.yaml.tmpl")
			params.Vars["ServerListenerTLSContext"] = params.LoadTestData("testdata/transport_socket/server.yaml.tmpl")

			if err := (&driver.Scenario{
				Steps: []driver.Step{
					&driver.XDS{},
					&driver.Update{
						Node:      "client",
						Version:   "0",
						Clusters:  []string{params.LoadTestData("testdata/cluster/tcp_client.yaml.tmpl")},
						Listeners: []string{params.LoadTestData("testdata/listener/tcp_client.yaml.tmpl")},
					},
					&driver.Update{
						Node:      "server",
						Version:   "0",
						Clusters:  []string{params.LoadTestData("testdata/cluster/tcp_server.yaml.tmpl")},
						Listeners: []string{params.LoadTestData("testdata/listener/tcp_server.yaml.tmpl")},
					},
					&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
					&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
					&driver.Sleep{Duration: 1 * time.Second},
					&driver.TCPServer{Prefix: "hello"},
					&driver.Repeat{
						N:    10,
						Step: &driver.TCPConnection{},
					},
					&driver.Stats{AdminPort: params.Ports.ClientAdmin, Matchers: map[string]driver.StatMatcher{
						"istio_tcp_connections_closed_total": &driver.ExactStat{Metric: "testdata/metric/tcp_client_connection_close.yaml.tmpl"},
						"istio_tcp_connections_opened_total": &driver.ExactStat{Metric: "testdata/metric/tcp_client_connection_open.yaml.tmpl"},
						"istio_tcp_received_bytes_total":     &driver.ExactStat{Metric: "testdata/metric/tcp_client_received_bytes.yaml.tmpl"},
						"istio_tcp_sent_bytes_total":         &driver.ExactStat{Metric: "testdata/metric/tcp_client_sent_bytes.yaml.tmpl"},
					}},
					&driver.Stats{AdminPort: params.Ports.ServerAdmin, Matchers: map[string]driver.StatMatcher{
						"istio_tcp_connections_closed_total":          &driver.ExactStat{Metric: "testdata/metric/tcp_server_connection_close.yaml.tmpl"},
						"istio_tcp_connections_opened_total":          &driver.ExactStat{Metric: "testdata/metric/tcp_server_connection_open.yaml.tmpl"},
						"istio_tcp_received_bytes_total":              &driver.ExactStat{Metric: "testdata/metric/tcp_server_received_bytes.yaml.tmpl"},
						"istio_tcp_sent_bytes_total":                  &driver.ExactStat{Metric: "testdata/metric/tcp_server_sent_bytes.yaml.tmpl"},
						"envoy_metadata_exchange_alpn_protocol_found": &driver.ExactStat{Metric: "testdata/metric/tcp_server_mx_stats_alpn_found.yaml.tmpl"},
						"envoy_metadata_exchange_metadata_added":      &driver.ExactStat{Metric: "testdata/metric/tcp_server_mx_stats_metadata_added.yaml.tmpl"},
					}},
				},
			}).Run(params); err != nil {
				t.Fatal(err)
			}
		})
	}
}

func TestTCPMetadataExchangeNoAlpn(t *testing.T) {
	for _, runtime := range Runtimes {
		t.Run(runtime.WasmRuntime, func(t *testing.T) {
			params := driver.NewTestParams(t, map[string]string{
				"DisableDirectResponse": "true",
				"AlpnProtocol":          "some-protocol",
				"StatsConfig":           driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
			}, envoye2e.ProxyE2ETests)
			params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
			params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
			params.Vars["ServerNetworkFilters"] = params.LoadTestData("testdata/filters/server_mx_network_filter.yaml.tmpl") + "\n" +
				params.LoadTestData("testdata/filters/server_stats_network_filter.yaml.tmpl")
			params.Vars["ClientUpstreamFilters"] = params.LoadTestData("testdata/filters/client_mx_network_filter.yaml.tmpl")
			params.Vars["ClientNetworkFilters"] = params.LoadTestData("testdata/filters/client_stats_network_filter.yaml.tmpl")
			params.Vars["ClientClusterTLSContext"] = params.LoadTestData("testdata/transport_socket/client.yaml.tmpl")
			params.Vars["ServerListenerTLSContext"] = params.LoadTestData("testdata/transport_socket/server.yaml.tmpl")

			if err := (&driver.Scenario{
				Steps: []driver.Step{
					&driver.XDS{},
					&driver.Update{
						Node:      "client",
						Version:   "0",
						Clusters:  []string{params.LoadTestData("testdata/cluster/tcp_client.yaml.tmpl")},
						Listeners: []string{params.LoadTestData("testdata/listener/tcp_client.yaml.tmpl")},
					},
					&driver.Update{
						Node:      "server",
						Version:   "0",
						Clusters:  []string{params.LoadTestData("testdata/cluster/tcp_server.yaml.tmpl")},
						Listeners: []string{params.LoadTestData("testdata/listener/tcp_server.yaml.tmpl")},
					},
					&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
					&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
					&driver.Sleep{Duration: 1 * time.Second},
					&driver.TCPServer{Prefix: "hello"},
					&driver.Repeat{
						N:    10,
						Step: &driver.TCPConnection{},
					},
					&driver.Stats{AdminPort: params.Ports.ServerAdmin, Matchers: map[string]driver.StatMatcher{
						"istio_tcp_connections_opened_total":              &driver.ExactStat{Metric: "testdata/metric/tcp_server_connection_open_without_mx.yaml.tmpl"},
						"envoy_metadata_exchange_alpn_protocol_not_found": &driver.ExactStat{Metric: "testdata/metric/tcp_server_mx_stats_alpn_not_found.yaml.tmpl"},
					}},
				},
			}).Run(params); err != nil {
				t.Fatal(err)
			}
		})
	}
}
