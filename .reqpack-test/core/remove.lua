return {
  name = "tempify remove installed template",
  request = {
    action = "remove",
    system = "tempify",
    flags = {
      "data-root=@tmp/store",
      "seed-from=.reqpack-test/fixtures/store/basic_cpp-v1",
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
    events = { "deleted", "success" },
  }
}
