return {
  id = "m3_conflict_child",
  name = "M3 Conflict Child",
  version = "0.1.0",
  description = "Conflict child for error strategy tests",
  source_dir = "files",
  includes = { "m3_conflict_parent" },
  merge = {
    file_conflicts = {
      ["conflict.txt"] = "error",
    },
  },
}
