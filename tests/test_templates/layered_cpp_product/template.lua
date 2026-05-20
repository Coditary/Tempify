return {
  id = "layered_cpp_product",
  name = "Layered C++ Product",
  version = "0.1.0",
  description = "Combined product template for layered template tests",
  source_dir = "files",
  includes = { "layered_cpp_base", "layered_ci_overlay" },
  merge = {
    file_conflicts = {
      ["src/main.cpp.pbt"] = "replace",
    },
    drop_paths = { "base-only.txt" },
  },
  output = {
    path = "{{ project_slug }}",
    overwrite = false,
  },
}
