/* Copyright 2022 The Bazel Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package merger_test

import (
	"strings"
	"testing"

	"github.com/bazelbuild/bazel-gazelle/merger"
	"github.com/bazelbuild/bazel-gazelle/rule"
	bzl "github.com/bazelbuild/buildtools/build"
	"github.com/google/go-cmp/cmp"
)

func TestFixLoads(t *testing.T) {
	knownLoads := []rule.LoadInfo{
		{
			Name: "@bar",
			Symbols: []string{
				"magic",
			},
		},
		{
			Name: "@foo",
			Symbols: []string{
				"foo_binary",
				"foo_library",
				"foo_test",
			},
		},
	}

	type testCase struct {
		input string
		want  string
	}

	for name, tc := range map[string]testCase{
		"correct": {
			input: `load("@foo", "foo_binary", "foo_library")

foo_binary(name = "a")

foo_library(name = "a_lib")
`,
			want: `load("@foo", "foo_binary", "foo_library")

foo_binary(name = "a")

foo_library(name = "a_lib")
`,
		},
		"correct with macro": {
			input: `load("@bar", "magic")
load("@foo", "foo_binary", "foo_library")

foo_binary(name = "a")

foo_library(
	name = "a_lib",
	deps = [
		"//a/b:c",
		magic("baz"),
	],
)
		`,
			want: `load("@bar", "magic")
load("@foo", "foo_binary", "foo_library")

foo_binary(name = "a")

foo_library(
    name = "a_lib",
    deps = [
        "//a/b:c",
        magic("baz"),
    ],
)
`,
		},
		"missing macro load": {
			input: `load("@foo", "foo_binary", "foo_library")

foo_binary(name = "a")

foo_library(
    name = "a_lib",
    deps = [
        "//a/b:c",
        magic("baz"),
    ],
)
`,
			want: `load("@bar", "magic")
load("@foo", "foo_binary", "foo_library")

foo_binary(name = "a")

foo_library(
    name = "a_lib",
    deps = [
        "//a/b:c",
        magic("baz"),
    ],
)
`,
		},
		"unused macro": {
			input: `load("@bar", "magic")
			load("@foo", "foo_binary")
	
foo_binary(name = "a")
`,
			want: `load("@foo", "foo_binary")

foo_binary(name = "a")
`,
		},
		"missing kind load symbol": {
			input: `load("@foo", "foo_binary")

foo_binary(name = "a")

foo_library(name = "a_lib")
`,
			want: `load("@foo", "foo_binary", "foo_library")

foo_binary(name = "a")

foo_library(name = "a_lib")
`,
		},
		"missing kind load": {
			input: `foo_binary(name = "a")

foo_library(name = "a_lib")
`,
			want: `load("@foo", "foo_binary", "foo_library")

foo_binary(name = "a")

foo_library(name = "a_lib")
`,
		},
		"unused kind load symbol": {
			input: `load("@foo", "foo_binary", "foo_library", "foo_test")

foo_binary(name = "a")

foo_library(name = "a_lib")
`,
			want: `load("@foo", "foo_binary", "foo_library")

foo_binary(name = "a")

foo_library(name = "a_lib")
`,
		},
	} {
		t.Run(name, func(t *testing.T) {
			f, err := rule.LoadData("", "", []byte(tc.input))
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}

			merger.FixLoads(f, knownLoads)
			f.Sync()

			want := strings.TrimSpace(tc.want)
			got := strings.TrimSpace(string(bzl.Format(f.File)))
			if diff := cmp.Diff(want, got); diff != "" {
				t.Errorf("FixLoads() mismatch (-want +got):\n%s", diff)
			}
		})
	}
}
