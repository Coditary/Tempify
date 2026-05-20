return {
  order = { "Project" },
  groups = {
    Project = {
      {
        key = "project_name",
        type = "string",
        prompt = "Project name",
        default = "Advanced App",
      },
      {
        key = "project_slug",
        type = "string",
        default = function(ctx)
          return slugify(ctx.values.project_name or "advanced-app")
        end,
      },
      {
        key = "use_notes",
        type = "bool",
        prompt = "Generate notes file",
        default = true,
      },
    },
  },
}
