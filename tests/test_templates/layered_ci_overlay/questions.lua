return {
  order = { "CI" },
  groups = {
    CI = {
      {
        key = "include_ci",
        type = "bool",
        prompt = "Include CI",
        default = true,
      },
      {
        key = "ci_provider",
        type = "choice",
        prompt = "CI provider",
        choices = { "github", "gitlab" },
        default = "github",
        condition = function(ctx)
          return ctx.values.include_ci == "true"
        end,
      },
    },
  },
}
