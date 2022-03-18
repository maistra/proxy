/* Copyright 2018 The Bazel Authors. All rights reserved.

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

package golang

import (
	"io/ioutil"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"

	"github.com/bazelbuild/bazel-gazelle/config"
	"github.com/bazelbuild/bazel-gazelle/language"
	"github.com/bazelbuild/bazel-gazelle/language/proto"
	"github.com/bazelbuild/bazel-gazelle/merger"
	"github.com/bazelbuild/bazel-gazelle/rule"
	"github.com/bazelbuild/bazel-gazelle/walk"
	bzl "github.com/bazelbuild/buildtools/build"
)

func TestGenerateRules(t *testing.T) {
	if runtime.GOOS == "windows" {
		// TODO(jayconrod): set up testdata directory on windows before running test
		if _, err := os.Stat("testdata"); os.IsNotExist(err) {
			t.Skip("testdata missing on windows due to lack of symbolic links")
		} else if err != nil {
			t.Fatal(err)
		}
	}

	c, langs, cexts := testConfig(
		t,
		"-build_file_name=BUILD.old",
		"-go_prefix=example.com/repo",
		"-repo_root=testdata")

	var loads []rule.LoadInfo
	for _, lang := range langs {
		loads = append(loads, lang.Loads()...)
	}
	walk.Walk(c, cexts, []string{"testdata"}, walk.VisitAllUpdateSubdirsMode, func(dir, rel string, c *config.Config, update bool, oldFile *rule.File, subdirs, regularFiles, genFiles []string) {
		t.Run(rel, func(t *testing.T) {
			var empty, gen []*rule.Rule
			for _, lang := range langs {
				res := lang.GenerateRules(language.GenerateArgs{
					Config:       c,
					Dir:          dir,
					Rel:          rel,
					File:         oldFile,
					Subdirs:      subdirs,
					RegularFiles: regularFiles,
					GenFiles:     genFiles,
					OtherEmpty:   empty,
					OtherGen:     gen})
				empty = append(empty, res.Empty...)
				gen = append(gen, res.Gen...)
			}
			isTest := false
			for _, name := range regularFiles {
				if name == "BUILD.want" {
					isTest = true
					break
				}
			}
			if !isTest {
				// GenerateRules may have side effects, so we need to run it, even if
				// there's no test.
				return
			}
			f := rule.EmptyFile("test", "")
			for _, r := range gen {
				r.Insert(f)
			}
			convertImportsAttrs(f)
			merger.FixLoads(f, loads)
			f.Sync()
			got := string(bzl.Format(f.File))
			wantPath := filepath.Join(dir, "BUILD.want")
			wantBytes, err := ioutil.ReadFile(wantPath)
			if err != nil {
				t.Fatalf("error reading %s: %v", wantPath, err)
			}
			want := string(wantBytes)
			want = strings.ReplaceAll(want, "\r\n", "\n")

			if got != want {
				t.Errorf("GenerateRules %q: got:\n%s\nwant:\n%s", rel, got, want)
			}
		})
	})
}

func TestGenerateRulesEmpty(t *testing.T) {
	c, langs, _ := testConfig(t, "-go_prefix=example.com/repo")
	goLang := langs[1].(*goLang)
	res := goLang.GenerateRules(language.GenerateArgs{
		Config: c,
		Dir:    "./foo",
		Rel:    "foo"})
	if len(res.Gen) > 0 {
		t.Errorf("got %d generated rules; want 0", len(res.Gen))
	}
	f := rule.EmptyFile("test", "")
	for _, r := range res.Empty {
		r.Insert(f)
	}
	f.Sync()
	got := strings.TrimSpace(string(bzl.Format(f.File)))
	want := strings.TrimSpace(`
filegroup(name = "go_default_library_protos")

go_proto_library(name = "foo_go_proto")

go_library(name = "foo")

go_binary(name = "foo")

go_test(name = "foo_test")
`)
	if got != want {
		t.Errorf("got:\n%s\nwant:\n%s", got, want)
	}
}

func TestGenerateRulesEmptyLegacyProto(t *testing.T) {
	c, langs, _ := testConfig(t, "-proto=legacy")
	goLang := langs[len(langs)-1].(*goLang)
	res := goLang.GenerateRules(language.GenerateArgs{
		Config: c,
		Dir:    "./foo",
		Rel:    "foo"})
	for _, e := range res.Empty {
		if kind := e.Kind(); kind == "proto_library" || kind == "go_proto_library" || kind == "go_grpc_library" {
			t.Errorf("deleted rule %s ; should not delete in legacy proto mode", kind)
		}
	}
}

func TestGenerateRulesEmptyPackageProto(t *testing.T) {
	c, langs, _ := testConfig(t, "-proto=package", "-go_prefix=example.com/repo")
	oldContent := []byte(`
proto_library(
    name = "dead_proto",
    srcs = ["dead.proto"],
)
`)
	old, err := rule.LoadData("BUILD.bazel", "", oldContent)
	if err != nil {
		t.Fatal(err)
	}
	var empty []*rule.Rule
	for _, lang := range langs {
		res := lang.GenerateRules(language.GenerateArgs{
			Config:     c,
			Dir:        "./foo",
			Rel:        "foo",
			File:       old,
			OtherEmpty: empty})
		empty = append(empty, res.Empty...)
	}
	f := rule.EmptyFile("test", "")
	for _, r := range empty {
		r.Insert(f)
	}
	f.Sync()
	got := strings.TrimSpace(string(bzl.Format(f.File)))
	want := strings.TrimSpace(`
proto_library(name = "dead_proto")

go_proto_library(name = "dead_go_proto")

filegroup(name = "go_default_library_protos")

go_proto_library(name = "foo_go_proto")

go_library(name = "foo")

go_binary(name = "foo")

go_test(name = "foo_test")
`)
	if got != want {
		t.Errorf("got:\n%s\nwant:\n%s", got, want)
	}
}

func TestGenerateRulesPrebuiltGoProtoRules(t *testing.T) {
	for _, protoFlag := range []string{
		"-proto=default",
		"-proto=package",
	} {
		t.Run("with flag: "+protoFlag, func(t *testing.T) {
			c, langs, _ := testConfig(t, protoFlag)
			goLang := langs[len(langs)-1].(*goLang)

			res := goLang.GenerateRules(language.GenerateArgs{
				Config:   c,
				Dir:      "./foo",
				Rel:      "foo",
				OtherGen: prebuiltProtoRules(),
			})

			if len(res.Gen) != 0 {
				t.Errorf("got %d generated rules; want 0", len(res.Gen))
			}
			f := rule.EmptyFile("test", "")
			for _, r := range res.Gen {
				r.Insert(f)
			}
			f.Sync()
			got := strings.TrimSpace(string(bzl.Format(f.File)))
			want := strings.TrimSpace(`
		`)
			if got != want {
				t.Errorf("got:\n%s\nwant:\n%s", got, want)
			}
		})
	}
}

// Test generated files that have been consumed by other rules should not be
// added to the go_default_library rule
func TestConsumedGenFiles(t *testing.T) {
	args := language.GenerateArgs{
		RegularFiles: []string{"regular.go"},
		GenFiles:     []string{"mocks.go"},
		Config: &config.Config{
			Exts: make(map[string]interface{}),
		},
	}
	otherRule := rule.NewRule("go_library", "go_mock_library")
	otherRule.SetAttr("srcs", []string{"mocks.go"})
	args.OtherGen = append(args.OtherGen, otherRule)

	gl := goLang{
		goPkgRels: make(map[string]bool),
	}
	gl.Configure(args.Config, "", nil)
	res := gl.GenerateRules(args)
	got := res.Gen[0].AttrStrings("srcs")
	want := []string{"regular.go"}
	if len(got) != len(want) || got[0] != want[0] {
		t.Errorf("got:\n%s\nwant:\n%s", got, want)
	}
}

func prebuiltProtoRules() []*rule.Rule {
	protoRule := rule.NewRule("proto_library", "foo_proto")
	protoRule.SetAttr("srcs", []string{"foo.proto"})
	protoRule.SetAttr("visibility", []string{"//visibility:public"})
	protoRule.SetPrivateAttr(proto.PackageKey,
		proto.Package{
			Name: "foo",
			Files: map[string]proto.FileInfo{
				"foo.proto": {},
			},
			Imports: map[string]bool{},
			Options: map[string]string{},
		},
	)

	goProtoRule := rule.NewRule("go_proto_library", "foo_go_proto")
	goProtoRule.SetAttr("compilers", []string{"@io_bazel_rules_go//proto:go_proto"})
	goProtoRule.SetAttr("importpath", "hello/world/foo")
	goProtoRule.SetAttr("proto", ":foo_proto")
	protoRule.SetAttr("visibility", []string{"//visibility:public"})

	return []*rule.Rule{protoRule, goProtoRule}
}

// convertImportsAttrs copies private attributes to regular attributes, which
// will later be written out to build files. This allows tests to check the
// values of private attributes with simple string comparison.
func convertImportsAttrs(f *rule.File) {
	for _, r := range f.Rules {
		v := r.PrivateAttr(config.GazelleImportsKey)
		if v != nil {
			r.SetAttr(config.GazelleImportsKey, v)
		}
	}
}
