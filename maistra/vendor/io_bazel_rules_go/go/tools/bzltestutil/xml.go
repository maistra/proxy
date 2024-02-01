// Copyright 2020 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package bzltestutil

import (
	"encoding/json"
	"encoding/xml"
	"fmt"
	"io"
	"path"
	"sort"
	"strings"
	"time"
)

type xmlTestSuites struct {
	XMLName xml.Name       `xml:"testsuites"`
	Suites  []xmlTestSuite `xml:"testsuite"`
}

type xmlTestSuite struct {
	XMLName   xml.Name      `xml:"testsuite"`
	TestCases []xmlTestCase `xml:"testcase"`
	Errors    int           `xml:"errors,attr"`
	Failures  int           `xml:"failures,attr"`
	Skipped   int           `xml:"skipped,attr"`
	Tests     int           `xml:"tests,attr"`
	Time      string        `xml:"time,attr"`
	Name      string        `xml:"name,attr"`
}

type xmlTestCase struct {
	XMLName   xml.Name    `xml:"testcase"`
	Classname string      `xml:"classname,attr"`
	Name      string      `xml:"name,attr"`
	Time      string      `xml:"time,attr"`
	Failure   *xmlMessage `xml:"failure,omitempty"`
	Error     *xmlMessage `xml:"error,omitempty"`
	Skipped   *xmlMessage `xml:"skipped,omitempty"`
}

type xmlMessage struct {
	Message  string `xml:"message,attr"`
	Type     string `xml:"type,attr"`
	Contents string `xml:",chardata"`
}

// jsonEvent as encoded by the test2json package.
type jsonEvent struct {
	Time    *time.Time
	Action  string
	Package string
	Test    string
	Elapsed *float64
	Output  string
}

type testCase struct {
	state    string
	output   strings.Builder
	duration *float64
}

// json2xml converts test2json's output into an xml output readable by Bazel.
// http://windyroad.com.au/dl/Open%20Source/JUnit.xsd
func json2xml(r io.Reader, pkgName string) ([]byte, error) {
	var pkgDuration *float64
	testcases := make(map[string]*testCase)
	testCaseByName := func(name string) *testCase {
		if name == "" {
			return nil
		}
		if _, ok := testcases[name]; !ok {
			testcases[name] = &testCase{}
		}
		return testcases[name]
	}

	dec := json.NewDecoder(r)
	for {
		var e jsonEvent
		if err := dec.Decode(&e); err == io.EOF {
			break
		} else if err != nil {
			return nil, fmt.Errorf("error decoding test2json output: %s", err)
		}
		switch s := e.Action; s {
		case "run":
			if c := testCaseByName(e.Test); c != nil {
				c.state = s
			}
		case "output":
			if c := testCaseByName(e.Test); c != nil {
				c.output.WriteString(e.Output)
			}
		case "skip":
			if c := testCaseByName(e.Test); c != nil {
				c.output.WriteString(e.Output)
				c.state = s
				c.duration = e.Elapsed
			}
		case "fail":
			if c := testCaseByName(e.Test); c != nil {
				c.state = s
				c.duration = e.Elapsed
			} else {
				pkgDuration = e.Elapsed
			}
		case "pass":
			if c := testCaseByName(e.Test); c != nil {
				c.duration = e.Elapsed
				c.state = s
			} else {
				pkgDuration = e.Elapsed
			}
		}
	}

	return xml.MarshalIndent(toXML(pkgName, pkgDuration, testcases), "", "\t")
}

func toXML(pkgName string, pkgDuration *float64, testcases map[string]*testCase) *xmlTestSuites {
	cases := make([]string, 0, len(testcases))
	for k := range testcases {
		cases = append(cases, k)
	}
	sort.Strings(cases)
	suite := xmlTestSuite{
		Name: pkgName,
	}
	if pkgDuration != nil {
		suite.Time = fmt.Sprintf("%.3f", *pkgDuration)
	}
	for _, name := range cases {
		c := testcases[name]
		suite.Tests++
		newCase := xmlTestCase{
			Name:      name,
			Classname: path.Base(pkgName),
		}
		if c.duration != nil {
			newCase.Time = fmt.Sprintf("%.3f", *c.duration)
		}
		switch c.state {
		case "skip":
			suite.Skipped++
			newCase.Skipped = &xmlMessage{
				Message:  "Skipped",
				Contents: c.output.String(),
			}
		case "fail":
			suite.Failures++
			newCase.Failure = &xmlMessage{
				Message:  "Failed",
				Contents: c.output.String(),
			}
		case "pass":
			break
		default:
			suite.Errors++
			newCase.Error = &xmlMessage{
				Message:  "No pass/skip/fail event found for test",
				Contents: c.output.String(),
			}
		}
		suite.TestCases = append(suite.TestCases, newCase)
	}
	return &xmlTestSuites{Suites: []xmlTestSuite{suite}}
}
