return {
  order = { "Project" },
  groups = {
    Project = {
      {
        key = "project_name",
        type = "string",
        prompt = "Project name",
        default = "Layered Base",
      },
      {
        key = "project_slug",
        type = "string",
        default = function(ctx)
          return slugify(ctx.values.project_name or "layered-base")
        end,
      },
      {
        key = "language_standard",
        type = "choice",
        prompt = "Language standard",
        choices = { "c++20", "c++23" },
        default = "c++20",
      },
    },
  },
}
