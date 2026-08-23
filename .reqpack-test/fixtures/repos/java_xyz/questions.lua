return {
  order = { "Project" },
  groups = {
    Project = {
      {
        key = "project_name",
        type = "string",
        prompt = "Project name",
        default = "Java XYZ App",
      },
      {
        key = "project_slug",
        type = "string",
        prompt = "Project slug",
        default = function(ctx)
          return slugify(ctx.values.project_name or "java-xyz-app")
        end,
      },
      {
        key = "package_name",
        type = "string",
        prompt = "Java package",
        default = "com.example.app",
      },
    },
  },
}
