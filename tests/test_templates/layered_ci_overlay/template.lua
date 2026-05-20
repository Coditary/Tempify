return {
  id = "layered_ci_overlay",
  name = "Layered CI Overlay",
  version = "0.1.0",
  description = "CI overlay layer for layered template tests",
  source_dir = "files",
  merge = {
    file_conflicts = {
      ["README.md.pbt"] = "replace",
    },
  },
}
