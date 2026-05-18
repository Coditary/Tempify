return {
  name = "tempify outdated templates",
  request = {
    action = "outdated",
    system = "tempify",
    flags = {
      "data-root=@tmp/store",
      "seed-from=.reqpack-test/fixtures/store/basic_cpp-v1",
      "repo=primary|10|.reqpack-test/fixtures/catalogs/primary/templates.json",
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
    events = { "outdated" },
    resultCount = 1,
    resultName = "basic_cpp",
    resultVersion = "1.0.0",
  }
}
