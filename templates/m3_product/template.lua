return {
  id = "m3_product",
  name = "M3 Product",
  version = "0.1.0",
  description = "Combined product template for M3 tests",
  source_dir = "files",
  includes = { "m3_lang_base", "m3_ci_layer" },
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
