return {
  name = "tempify list installed templates",
  request = {
    action = "list",
    system = "tempify",
    flags = {
      "data-root=@tmp/store",
      "seed-from=.reqpack-test/fixtures/store/basic_cpp-v1",
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
    events = { "listed" },
    resultCount = 1,
    resultName = "basic_cpp",
    resultVersion = "1.0.0",
  }
}
