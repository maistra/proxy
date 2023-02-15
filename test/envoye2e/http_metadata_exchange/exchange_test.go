// Copyright 2020 Istio Authors
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
	"encoding/base64"
	"testing"
	"time"

	pstruct "github.com/golang/protobuf/ptypes/struct"
	"google.golang.org/protobuf/proto"

	"istio.io/proxy/test/envoye2e"
	"istio.io/proxy/test/envoye2e/driver"
)

func EncodeMetadata(t *testing.T, p *driver.Params) string {
	pb := &pstruct.Struct{}
	err := p.FillYAML("{"+p.Vars["ClientMetadata"]+"}", pb)
	if err != nil {
		t.Fatal(err)
	}
	bytes, err := proto.Marshal(pb)
	if err != nil {
		t.Fatal(err)
	}
	return base64.RawStdEncoding.EncodeToString(bytes)
}

func TestHTTPExchange(t *testing.T) {
	params := driver.NewTestParams(t, map[string]string{}, envoye2e.ProxyE2ETests)
	params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
	params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
	params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/mx_inbound.yaml.tmpl")
	params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/mx_outbound.yaml.tmpl")
	if err := (&driver.Scenario{
		Steps: []driver.Step{
			&driver.XDS{},
			&driver.Update{Node: "server", Version: "0", Listeners: []string{driver.LoadTestData("testdata/listener/server.yaml.tmpl")}},
			&driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
			&driver.Sleep{Duration: 1 * time.Second},
			&driver.HTTPCall{
				Port: params.Ports.ServerPort,
				Body: "hello, world!",
				ResponseHeaders: map[string]string{
					"x-envoy-peer-metadata-id": driver.None,
					"x-envoy-peer-metadata":    driver.None,
				},
			},
			&driver.HTTPCall{
				Port: params.Ports.ServerPort,
				Body: "hello, world!",
				RequestHeaders: map[string]string{
					"x-envoy-peer-metadata-id": "client",
				},
				ResponseHeaders: map[string]string{
					"x-envoy-peer-metadata-id": "server",
					"x-envoy-peer-metadata":    driver.None,
				},
			},
			&driver.HTTPCall{
				Port: params.Ports.ServerPort,
				Body: "hello, world!",
				RequestHeaders: map[string]string{
					"x-envoy-peer-metadata-id": "client",
					"x-envoy-peer-metadata":    EncodeMetadata(t, params),
				},
				ResponseHeaders: map[string]string{
					"x-envoy-peer-metadata-id": "server",
					"x-envoy-peer-metadata":    driver.Any,
				},
			},
		},
	}).Run(params); err != nil {
		t.Fatal(err)
	}
}
