return {
  name = "tempify install local template",
  request = {
    action = "install",
    system = "tempify",
    flags = {
      "data-root=@tmp/store",
    },
    localPath = ".reqpack-test/fixtures/local/local_only",
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
