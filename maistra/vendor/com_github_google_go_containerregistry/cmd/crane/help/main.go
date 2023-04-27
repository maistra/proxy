package main

import (
	"fmt"
	"os"

	"github.com/google/go-containerregistry/cmd/crane/cmd"
	"github.com/spf13/cobra"
	"github.com/spf13/cobra/doc"
)

var dir string
var root = &cobra.Command{
	Use:   "gendoc",
	Short: "Generate crane's help docs",
	Args:  cobra.NoArgs,
	RunE: func(*cobra.Command, []string) error {
		return doc.GenMarkdownTree(cmd.Root, dir)
	},
}

func init() {
	root.Flags().StringVarP(&dir, "dir", "d", ".", "Path to directory in which to generate docs")
}

func main() {
	if err := root.Execute(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}
