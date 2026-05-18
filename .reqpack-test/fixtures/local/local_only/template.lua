return {
  id = "local_only",
  name = "Local Only",
  version = "0.2.0",
  description = "Local template install fixture",
  source_dir = "files",
  output = {
    path = "{{ project_slug }}",
    overwrite = false,
  },
}
