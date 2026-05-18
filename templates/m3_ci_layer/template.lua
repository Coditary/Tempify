return {
  id = "m3_ci_layer",
  name = "M3 CI Layer",
  version = "0.1.0",
  description = "CI overlay layer for M3 tests",
  source_dir = "files",
  merge = {
    file_conflicts = {
      ["README.md.pbt"] = "replace",
    },
  },
}
