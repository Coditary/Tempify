return {
  order = { "Project" },
  groups = {
    Project = {
      {
        key = "project_name",
        type = "string",
        prompt = "Project name",
        default = "M3 Base",
      },
      {
        key = "project_slug",
        type = "string",
        default = function(ctx)
          return slugify(ctx.values.project_name or "m3-base")
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
