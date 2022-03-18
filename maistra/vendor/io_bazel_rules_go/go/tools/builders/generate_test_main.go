/* Copyright 2016 The Bazel Authors. All rights reserved.

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

// Bare bones Go testing support for Bazel.

package main

import (
	"flag"
	"fmt"
	"go/ast"
	"go/doc"
	"go/parser"
	"go/token"
	"os"
	"sort"
	"strings"
	"text/template"
)

type Import struct {
	Name string
	Path string
}

type TestCase struct {
	Package string
	Name    string
}

type Example struct {
	Package   string
	Name      string
	Output    string
	Unordered bool
}

// Cases holds template data.
type Cases struct {
	Imports    []*Import
	Tests      []TestCase
	Benchmarks []TestCase
	Examples   []Example
	TestMain   string
	Coverage   bool
	Pkgname    string
}

const testMainTpl = `
package main

// This package must be initialized before packages being tested.
// NOTE: this relies on the order of package initialization, which is the spec
// is somewhat unclear about-- it only clearly guarantees that imported packages
// are initialized before their importers, though in practice (and implied) it
// also respects declaration order, which we're relying on here.
import "github.com/bazelbuild/rules_go/go/tools/bzltestutil"

import (
	"flag"
	"log"
	"os"
	"os/exec"
{{if .TestMain}}
	"reflect"
{{end}}
	"strconv"
	"testing"
	"testing/internal/testdeps"

{{if .Coverage}}
	"github.com/bazelbuild/rules_go/go/tools/coverdata"
{{end}}

{{range $p := .Imports}}
	{{$p.Name}} "{{$p.Path}}"
{{end}}
)

var allTests = []testing.InternalTest{
{{range .Tests}}
	{"{{.Name}}", {{.Package}}.{{.Name}} },
{{end}}
}

var benchmarks = []testing.InternalBenchmark{
{{range .Benchmarks}}
	{"{{.Name}}", {{.Package}}.{{.Name}} },
{{end}}
}

var examples = []testing.InternalExample{
{{range .Examples}}
	{Name: "{{.Name}}", F: {{.Package}}.{{.Name}}, Output: {{printf "%q" .Output}}, Unordered: {{.Unordered}} },
{{end}}
}

func testsInShard() []testing.InternalTest {
	totalShards, err := strconv.Atoi(os.Getenv("TEST_TOTAL_SHARDS"))
	if err != nil || totalShards <= 1 {
		return allTests
	}
	shardIndex, err := strconv.Atoi(os.Getenv("TEST_SHARD_INDEX"))
	if err != nil || shardIndex < 0 {
		return allTests
	}
	tests := []testing.InternalTest{}
	for i, t := range allTests {
		if i % totalShards == shardIndex {
			tests = append(tests, t)
		}
	}
	return tests
}

func main() {
	if bzltestutil.ShouldWrap() {
		err := bzltestutil.Wrap("{{.Pkgname}}")
		if xerr, ok := err.(*exec.ExitError); ok {
			os.Exit(xerr.ExitCode())
		} else if err != nil {
			log.Print(err)
			os.Exit(bzltestutil.TestWrapperAbnormalExit)
		} else {
			os.Exit(0)
		}
	}

	m := testing.MainStart(testdeps.TestDeps{}, testsInShard(), benchmarks, examples)

	if filter := os.Getenv("TESTBRIDGE_TEST_ONLY"); filter != "" {
		flag.Lookup("test.run").Value.Set(filter)
	}

	{{if .Coverage}}
	if len(coverdata.Cover.Counters) > 0 {
		testing.RegisterCover(coverdata.Cover)
	}
	if coverageDat, ok := os.LookupEnv("COVERAGE_OUTPUT_FILE"); ok {
		if testing.CoverMode() != "" {
			flag.Lookup("test.coverprofile").Value.Set(coverageDat)
		}
	}
	{{end}}

	{{if not .TestMain}}
	os.Exit(m.Run())
	{{else}}
	{{.TestMain}}(m)
	{{/* See golang.org/issue/34129 and golang.org/cl/219639 */}}
	os.Exit(int(reflect.ValueOf(m).Elem().FieldByName("exitCode").Int()))
	{{end}}
}
`

func genTestMain(args []string) error {
	// Prepare our flags
	args, err := expandParamsFiles(args)
	if err != nil {
		return err
	}
	imports := multiFlag{}
	sources := multiFlag{}
	flags := flag.NewFlagSet("GoTestGenTest", flag.ExitOnError)
	goenv := envFlags(flags)
	out := flags.String("output", "", "output file to write. Defaults to stdout.")
	coverage := flags.Bool("coverage", false, "whether coverage is supported")
	pkgname := flags.String("pkgname", "", "package name of test")
	flags.Var(&imports, "import", "Packages to import")
	flags.Var(&sources, "src", "Sources to process for tests")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if err := goenv.checkFlags(); err != nil {
		return err
	}
	// Process import args
	importMap := map[string]*Import{}
	for _, imp := range imports {
		parts := strings.Split(imp, "=")
		if len(parts) != 2 {
			return fmt.Errorf("Invalid import %q specified", imp)
		}
		i := &Import{Name: parts[0], Path: parts[1]}
		importMap[i.Name] = i
	}
	// Process source args
	sourceList := []string{}
	sourceMap := map[string]string{}
	for _, s := range sources {
		parts := strings.Split(s, "=")
		if len(parts) != 2 {
			return fmt.Errorf("Invalid source %q specified", s)
		}
		sourceList = append(sourceList, parts[1])
		sourceMap[parts[1]] = parts[0]
	}

	// filter our input file list
	filteredSrcs, err := filterAndSplitFiles(sourceList)
	if err != nil {
		return err
	}
	goSrcs := filteredSrcs.goSrcs

	outFile := os.Stdout
	if *out != "" {
		var err error
		outFile, err = os.Create(*out)
		if err != nil {
			return fmt.Errorf("os.Create(%q): %v", *out, err)
		}
		defer outFile.Close()
	}

	cases := Cases{
		Coverage: *coverage,
		Pkgname:  *pkgname,
	}

	testFileSet := token.NewFileSet()
	pkgs := map[string]bool{}
	for _, f := range goSrcs {
		parse, err := parser.ParseFile(testFileSet, f.filename, nil, parser.ParseComments)
		if err != nil {
			return fmt.Errorf("ParseFile(%q): %v", f.filename, err)
		}
		pkg := sourceMap[f.filename]
		if strings.HasSuffix(parse.Name.String(), "_test") {
			pkg += "_test"
		}
		for _, e := range doc.Examples(parse) {
			if e.Output == "" && !e.EmptyOutput {
				continue
			}
			cases.Examples = append(cases.Examples, Example{
				Name:      "Example" + e.Name,
				Package:   pkg,
				Output:    e.Output,
				Unordered: e.Unordered,
			})
			pkgs[pkg] = true
		}
		for _, d := range parse.Decls {
			fn, ok := d.(*ast.FuncDecl)
			if !ok {
				continue
			}
			if fn.Recv != nil {
				continue
			}
			if fn.Name.Name == "TestMain" {
				// TestMain is not, itself, a test
				pkgs[pkg] = true
				cases.TestMain = fmt.Sprintf("%s.%s", pkg, fn.Name.Name)
				continue
			}

			// Here we check the signature of the Test* function. To
			// be considered a test:

			// 1. The function should have a single argument.
			if len(fn.Type.Params.List) != 1 {
				continue
			}

			// 2. The function should return nothing.
			if fn.Type.Results != nil {
				continue
			}

			// 3. The only parameter should have a type identified as
			//    *<something>.T
			starExpr, ok := fn.Type.Params.List[0].Type.(*ast.StarExpr)
			if !ok {
				continue
			}
			selExpr, ok := starExpr.X.(*ast.SelectorExpr)
			if !ok {
				continue
			}

			// We do not descriminate on the referenced type of the
			// parameter being *testing.T. Instead we assert that it
			// should be *<something>.T. This is because the import
			// could have been aliased as a different identifier.

			if strings.HasPrefix(fn.Name.Name, "Test") {
				if selExpr.Sel.Name != "T" {
					continue
				}
				pkgs[pkg] = true
				cases.Tests = append(cases.Tests, TestCase{
					Package: pkg,
					Name:    fn.Name.Name,
				})
			}
			if strings.HasPrefix(fn.Name.Name, "Benchmark") {
				if selExpr.Sel.Name != "B" {
					continue
				}
				pkgs[pkg] = true
				cases.Benchmarks = append(cases.Benchmarks, TestCase{
					Package: pkg,
					Name:    fn.Name.Name,
				})
			}
		}
	}

	for name := range importMap {
		// Set the names for all unused imports to "_"
		if !pkgs[name] {
			importMap[name].Name = "_"
		}
		cases.Imports = append(cases.Imports, importMap[name])
	}
	sort.Slice(cases.Imports, func(i, j int) bool {
		return cases.Imports[i].Name < cases.Imports[j].Name
	})
	tpl := template.Must(template.New("source").Parse(testMainTpl))
	if err := tpl.Execute(outFile, &cases); err != nil {
		return fmt.Errorf("template.Execute(%v): %v", cases, err)
	}
	return nil
}
