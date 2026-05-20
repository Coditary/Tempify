return {
  name = "tempify install java-xyz from local registry",
  request = {
    action = "install",
    system = "tempify",
    flags = {
      "data-root=@tmp/store",
    },
    packages = {
      { name = "java-xyz" }
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
