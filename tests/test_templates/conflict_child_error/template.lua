return {
  id = "conflict_child_error",
  name = "Conflict Child Error",
  version = "0.1.0",
  description = "Conflict child for error strategy tests",
  source_dir = "files",
  includes = { "conflict_parent_base" },
  merge = {
    file_conflicts = {
      ["conflict.txt"] = "error",
    },
  },
}
