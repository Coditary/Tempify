return {
  name = "tempify info merged catalog template",
  request = {
    action = "info",
    system = "tempify",
    flags = {
      "data-root=@tmp/store",
      "repo=secondary|1|.reqpack-test/fixtures/catalogs/secondary/templates.json",
      "repo=primary|10|.reqpack-test/fixtures/catalogs/primary/templates.json",
    },
    prompt = "basic_cpp",
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
    events = { "informed" },
    resultCount = 1,
    resultName = "basic_cpp",
    resultVersion = "1.1.0",
  }
}
