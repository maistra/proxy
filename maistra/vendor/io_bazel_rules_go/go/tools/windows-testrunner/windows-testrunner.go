package main

import (
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"

	"gopkg.in/yaml.v2"
)

func main() {
	log.SetFlags(0)
	log.SetPrefix("testrunner: ")
	if err := run(os.Args[1:]); err != nil {
		log.Fatal(err)
	}
}

func run(args []string) error {
	if len(args) != 1 {
		return fmt.Errorf("want 1 arg; got %d", len(args))
	}

	testPath := args[0]
	testData, err := ioutil.ReadFile(testPath)
	if err != nil {
		return err
	}
	var config interface{}
	if err := yaml.Unmarshal(testData, &config); err != nil {
		return err
	}

	platform := config.(map[interface{}]interface{})["platforms"].(map[interface{}]interface{})["windows"].(map[interface{}]interface{})
	var flags, buildTargets, testTargets []string
	for _, f := range platform["build_flags"].([]interface{}) {
		flags = append(flags, f.(string))
	}
	for _, t := range platform["build_targets"].([]interface{}) {
		buildTargets = append(buildTargets, t.(string))
	}
	for _, t := range platform["test_targets"].([]interface{}) {
		testTargets = append(testTargets, t.(string))
	}

	buildCmd := exec.Command("bazel", "build")
	buildCmd.Args = append(buildCmd.Args, flags...)
	buildCmd.Args = append(buildCmd.Args, buildTargets...)
	buildCmd.Stdout = os.Stdout
	buildCmd.Stderr = os.Stderr
	if err := buildCmd.Run(); err != nil {
		return err
	}

	testCmd := exec.Command("bazel", "test")
	testCmd.Args = append(testCmd.Args, flags...)
	testCmd.Args = append(testCmd.Args, testTargets...)
	testCmd.Stdout = os.Stdout
	testCmd.Stderr = os.Stderr
	if err := testCmd.Run(); err != nil {
		return err
	}

	return nil
}
