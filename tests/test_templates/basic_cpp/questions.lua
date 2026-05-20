return {
  order = { "Project", "CI" },
  groups = {
    Project = {
      {
        key = "project_name",
        type = "string",
        prompt = "Project name",
        default = "My App",
        help = "Display name used in generated files.",
        validate = function(ctx)
          if #ctx.value < 3 then
            return "Project name must have at least 3 characters."
          end
          return true
        end,
      },
      {
        key = "project_slug",
        type = "string",
        prompt = "Project slug",
        default = function(ctx)
          return slugify(ctx.values.project_name or "my-app")
        end,
        alias = { "name_slug" },
        help = "Used for folder names and CMake target. Press Enter to accept generated slug.",
        validate = function(ctx)
          if not string.match(ctx.value, "^[a-z0-9%-]+$") then
            return "Slug must contain only lowercase letters, digits, and '-'."
          end
          return true
        end,
      },
      {
        key = "namespace",
        type = "string",
        prompt = "C++ namespace",
        default = "sample",
        validate = function(ctx)
          if not string.match(ctx.value, "^[A-Za-z_][A-Za-z0-9_]*$") then
            return "Namespace must be valid C++ identifier."
          end
          return true
        end,
      },
    },
    CI = {
      {
        key = "include_ci",
        type = "bool",
        prompt = "Include CI config?",
        default = true,
        help = "If yes, Tempify asks for provider metadata now. File generation for CI templates comes later.",
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
      {
        key = "docs_url",
        type = "string",
        prompt = "Documentation URL",
        optional = true,
        help = "Optional. Enter '-' to skip.",
        condition = function(ctx)
          return ctx.values.include_ci == "true"
        end,
        validate = function(ctx)
          if ctx.value == "" then
            return true
          end
          if not string.match(ctx.value, "^https?://") then
            return "Documentation URL must start with http:// or https://"
          end
          return true
        end,
      },
    },
  },
}
