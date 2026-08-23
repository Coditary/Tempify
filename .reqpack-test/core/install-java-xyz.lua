return {
  name = "tempify install java-xyz from local registry",
  request = {
    action = "install",
    system = "tempify",
    flags = {
      "data-root=@tmp/store",
      "repo=tempify-default|0|.reqpack-test/fixtures/catalogs/java_xyz/templates.json",
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
