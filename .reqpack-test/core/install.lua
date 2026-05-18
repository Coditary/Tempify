return {
  name = "tempify install catalog template",
  request = {
    action = "install",
    system = "tempify",
    flags = {
      "data-root=@tmp/store",
      "repo=secondary|1|.reqpack-test/fixtures/catalogs/secondary/templates.json",
      "repo=primary|10|.reqpack-test/fixtures/catalogs/primary/templates.json",
    },
    packages = {
      { name = "basic_cpp" }
    },
  },
  fakeExec = {
    {
      match = "command -v '",
      exitCode = 0,
      stdout = "",
      stderr = "",
      success = true,
    },
  },
  expect = {
    success = true,
    events = { "installed", "success" },
  }
}
