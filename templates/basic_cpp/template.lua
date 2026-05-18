return {
  id = "basic_cpp",
  name = "Basic C++ App",
  version = "0.1.0",
  description = "Small C++ starter built through Tempify and Prebyte",
  source_dir = "files",
  includes = { "base_cpp_common" },
  output = {
    path = "{{ project_slug }}",
    overwrite = false,
  },
}
