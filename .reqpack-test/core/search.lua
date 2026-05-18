return {
  name = "tempify search merged catalog",
  request = {
    action = "search",
    system = "tempify",
    flags = {
      "data-root=@tmp/store",
      "repo=secondary|1|.reqpack-test/fixtures/catalogs/secondary/templates.json",
      "repo=primary|10|.reqpack-test/fixtures/catalogs/primary/templates.json",
    },
    prompt = "starter",
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
    events = { "searched" },
    resultCount = 1,
    resultName = "basic_cpp",
    resultVersion = "1.1.0",
  }
}
