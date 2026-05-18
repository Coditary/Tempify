return {
  name = "tempify update outdated template",
  request = {
    action = "update",
    system = "tempify",
    flags = {
      "data-root=@tmp/store",
      "seed-from=.reqpack-test/fixtures/store/basic_cpp-v1",
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
    events = { "updated", "success" },
  }
}
