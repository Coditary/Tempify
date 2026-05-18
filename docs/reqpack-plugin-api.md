# ReqPack Tempify Plugin Notes

Quick reference for this concrete plugin.
Source of truth for ReqPack Lua runtime remains `ReqPack.wiki/Extending-Writing-Lua-Plugins.md`.

## This Plugin Uses

- `context.repositories` for Tempify catalog sources
- `context.fs.get_tmp_dir()` for isolated temp roots
- `context.net.download(...)` for remote catalog fetch fallback path
- `context.events.*` for ReqPack-visible install/search/info/list/update/remove/outdated results
- `context.artifacts.register(...)` for installed template roots
- `reqpack.exec.run(...)` in `plugin.init()` for `git` detection

## Tempify-Specific State Files

Plugin keeps Tempify-compatible state under shared data root:

- `templates/<id>/`
- `index/templates.json`
- `index/reqpack-available.json`
- `index/reqpack-installed.json`

## Catalog Winner Rules

- higher repository `priority` wins
- equal priority falls back to lexical `repo.id`
- one winner per template `id`

## Test Fixture Flags

Core plugin tests use lightweight plugin flags to keep state hermetic:

- `data-root=@tmp/<suffix>`: place Tempify store under ReqPack-created temp dir
- `seed-from=<path>`: copy seeded store fixture before action runs
- `repo=<id>|<priority>|<path-or-url>`: override `context.repositories` with inline fixture repos

These flags exist mainly for `rqp test-plugin` coverage.

## Verification

```bash
rqp test-plugin --plugin . --preset core
```

## ReqPack Contract Reference

Remaining sections below preserve general ReqPack Lua/plugin-test notes used by this repo.

## Required Files

Expected bundle layout:

```text
<plugin-id>/
  metadata.json
  reqpack.lua
  run.lua
  scripts/
    install.lua
    remove.lua
  .reqpack-test/
    core/
```

Files and roles:

- `metadata.json`: bundle metadata. `name` becomes plugin id used for discovery.
- `reqpack.lua`: bundle manifest. Keep `apiVersion = 1`; add ReqPack-side `depends` here.
- `run.lua`: real wrapper implementation loaded by `LuaBridge`.
- `scripts/install.lua` and `scripts/remove.lua`: required for bundle validity even when wrapper logic lives in `run.lua`.
- `.reqpack-test/core/*.lua`: hermetic test cases used by `rqp test-plugin --preset core`.
- `README.md`: quickstart for people opening template repo first.

## Runtime Lifecycle

Actual timing matters for wrapper authors:

1. ReqPack validates bundle layout from `metadata.json` and `reqpack.lua`.
2. First plugin construction executes `run.lua` immediately.
3. During construction ReqPack may read:
   - `plugin.getName()`
   - `plugin.getVersion()`
   - `plugin.getSecurityMetadata()`
   - `plugin.fileExtensions`
4. Only after contract validation does ReqPack call optional `plugin.init()`.
5. Action/query methods run later as planner and executor need them.
6. Optional `plugin.shutdown()` runs when plugin registry shuts down.

Implications:

- top-level `run.lua` code runs before `plugin.init()`
- metadata methods should stay side-effect free
- `plugin.fileExtensions` must be populated before `init()` if local-target detection depends on it
- `getCategories()` can also be used on constructed plugin before `init()`

## Required Plugin Contract

Main script must expose global `plugin` table.

Required methods:

```lua
function plugin.getName() end
function plugin.getVersion() end
function plugin.getRequirements() end
function plugin.getCategories() end
function plugin.getMissingPackages(packages) end
function plugin.install(context, packages) end
function plugin.installLocal(context, path) end
function plugin.remove(context, packages) end
function plugin.update(context, packages) end
function plugin.list(context) end
function plugin.search(context, prompt) end
function plugin.info(context, packageName) end
```

Useful optional methods:

```lua
function plugin.init() end
function plugin.shutdown() end
function plugin.outdated(context) end
function plugin.resolvePackage(context, package) end
function plugin.resolveProxyRequest(context, request) end
function plugin.getSecurityMetadata() end
function plugin.pack(context, projectPath, outputPath, flags) end
```

Optional static data:

```lua
plugin.fileExtensions = { ".rpm", ".deb" }
```

Return rules:

- boolean-style action methods treat no return as success
- query methods treat no return as empty result
- `getMissingPackages()` should return only packages that still need work

## Runtime Globals

ReqPack injects these globals before executing `run.lua`:

```lua
REQPACK_PLUGIN_ID
REQPACK_PLUGIN_DIR
REQPACK_PLUGIN_SCRIPT
reqpack
```

`print(...)` is also redirected into ReqPack output.

### `reqpack`

Current global namespace:

```lua
local result = reqpack.exec.run("command -v your-tool >/dev/null 2>&1")
local host = reqpack.host
```

Important differences from `context.exec.run(...)`:

- only plain command-string overload exists
- output is tied to plugin scope, not current item id
- `reqpack.host` is bridge-global host snapshot captured when plugin bridge is created

## `context` Surface

ReqPack passes `context` into action methods and most optional runtime hooks.

### `context.plugin`

```lua
context.plugin.id
context.plugin.dir
context.plugin.script
```

### `context.flags`

Array of request/runtime flags currently active for this plugin call.

### `context.host`

Per-call host snapshot. Same shape as `reqpack.host`, but created for current call instead of bridge construction.

Top-level sections:

```lua
context.host.platform
context.host.os
context.host.kernel
context.host.cpu
context.host.memory
context.host.gpus
context.host.storage
context.host.cache
```

Examples inside those tables include fields such as:

```lua
context.host.platform.osFamily
context.host.platform.arch
context.host.os.id
context.host.cpu.logicalCores
context.host.memory.totalBytes
context.host.cache.expiresAtEpoch
```

Use `context.host` when you want freshest host view during action execution.

### `context.proxy`

Available when current system has proxy config.

```lua
context.proxy.default
context.proxy.targets
context.proxy.options
```

### `context.repositories`

Array of repository entries for current ecosystem. Each entry can expose fields such as:

```lua
repo.id
repo.url
repo.priority
repo.enabled
repo.type
repo.auth
repo.validation
repo.scope
```

### `context.log`

```lua
context.log.debug("...")
context.log.info("...")
context.log.warn("...")
context.log.error("...")
```

### `context.tx`

```lua
context.tx.status(42)
context.tx.progress(50)
context.tx.progress({ percent = 50, current = 10, currentUnit = "MB" })
context.tx.begin_step("install packages")
context.tx.commit()
context.tx.success()
context.tx.failed("install failed")
```

Notes:

- `progress(50)` is valid shorthand for percent updates
- table payloads can include `percent`, `current`, `currentUnit`, `total`, `totalUnit`, `speed`, `speedUnit`

### `context.events`

Use these to tell ReqPack what happened:

```lua
context.events.installed(payload)
context.events.deleted(payload)
context.events.updated(payload)
context.events.listed(payload)
context.events.searched(payload)
context.events.informed(payload)
context.events.outdated(payload)
context.events.unavailable(payload)
```

Payloads are serialized into text records. Use deterministic tables so tests stay stable.

### `context.artifacts`

```lua
context.artifacts.register({ type = "file", path = "/tmp/out" })
```

Use this when wrapper produces artifacts, especially in optional `plugin.pack()` flows.

### `context.exec`

Overloads:

```lua
local result = context.exec.run("command")
local result = context.exec.run("command", rules)
```

Return shape visible in Lua:

```lua
result.success
result.exitCode
result.stdout
result.stderr
```

Important behavior:

- shell command runs as `/bin/sh -c <command>`
- stdout/stderr transcript is merged into `result.stdout`
- on failure without runner read error, merged transcript is copied into `result.stderr`
- use this form inside action methods so output stays tied to current item when ReqPack has one

### `context.fs`

```lua
local tmpDir = context.fs.get_tmp_dir()
```

ReqPack deletes these temp directories during plugin shutdown.

### `context.net`

```lua
local ok = context.net.download(url, destinationPath)
```

Return type is boolean only.
Lua side does not receive full `DownloadResult` object.

### Summary

Prefer:

- `context.exec.run(...)` inside action methods
- `reqpack.exec.run(...)` for top-level checks such as `plugin.init()`
- `context.host` for call-time host decisions
- `reqpack.host` only when bridge-time snapshot is enough

## Exec Rules For `context.exec.run(command, rules)`

Use exec rules when wrapper must react to command output while command is still running.

Top-level shape:

```lua
local rules = {
  initial = "default",
  rules = {
    {
      state = "default",
      source = "line",
      regex = "^Loaded (.+)$",
      repeat = true,
      stop = false,
      actions = {
        { type = "log", level = "info", message = "${1}" },
      },
    },
  },
}
```

Schema rules that matter most for template authors:

- top-level keys allowed only: `initial`, `rules`
- `rules` must be contiguous 1-based array-style table
- each rule must include `source`, `regex`, `actions`
- `source` must be `line` or `screen`
- `actions` must be non-empty contiguous 1-based array-style table

Common action types:

- `log`
- `status`
- `progress`
- `begin_step`
- `success`
- `failed`
- `event`
- `artifact`
- `send` for PTY-driven commands
- `state` for internal rule-state tracking

Runner selection:

- empty ruleset: plain shell runner
- rules with no `screen` rule and no `send` action: line runner
- any `screen` rule or any `send` action: PTY runner

Important placeholders:

- `${0}` full regex match
- `${1}`, `${2}`, ... capture groups
- missing capture resolves to empty string

If rule shape is malformed, command fails immediately with `success = false` and `exitCode = 1`.

## Return Shapes You Usually Build

### `getMissingPackages(packages)`

Return only packages that still need work.

Examples:

- install: package not installed yet
- remove: package currently installed
- update: package has newer version available

Lazy `return packages` works, but planning quality gets worse.

### `list`, `search`, `outdated`

Return arrays of package info tables.

Common fields:

```lua
{
  name = "curl",
  version = "8.0.1",
  latestVersion = "8.1.0",
  type = "package",
  summary = "Transfer tool",
  description = "Longer description",
  architecture = "x86_64",
}
```

### `info`

Return one package info table.

ReqPack accepts many more fields than template uses, including `homepage`, `license`, `dependencies`, `provides`, `tags`, and `extraFields`.
If `summary` is empty, ReqPack copies `description` into it.

## Optional Hooks

### `plugin.resolvePackage(context, package)`

Use this when exact version lookup is possible and you want better SBOM/audit coverage.

### `plugin.resolveProxyRequest(context, request)`

Must return table like:

```lua
{
  targetSystem = "maven",
  packages = { "org.junit:junit:4.13" },
  flags = { "arch=noarch" },
}
```

Supported keys:

- `targetSystem` required
- `packages` optional string array
- `localPath` optional string
- `flags` optional string array

Important rules:

- `packages` and `localPath` are mutually exclusive
- returning `nil` is treated as resolution failure, not pass-through

### `plugin.getSecurityMetadata()`

Optional table used for trust, thin-layer exec policy, and vulnerability mapping.
ReqPack may read it before `plugin.init()`.

## Testing With `rqp test-plugin`

Core commands:

```bash
rqp test-plugin --plugin . --preset core
rqp test-plugin --plugin . --case ./.reqpack-test/core/info.lua
rqp test-plugin --plugin . --cases ./.reqpack-test/core --report ./plugin-test-report.json
```

Case files return Lua table with:

- `name`
- `request`
- `fakeExec`
- `expect`

### `request`

Typical fields:

```lua
request = {
  action = "install",
  system = "demo",
  prompt = "curl",
  localPath = "/tmp/demo.tgz",
  packages = {
    { name = "curl", version = "8.0.0" }
  },
}
```

Use `localPath` with `action = "install"` to test `plugin.installLocal(context, path)`.

### `fakeExec`

`fakeExec` is ordered array of substring-match rules.

```lua
fakeExec = {
  {
    match = "demo-pm install curl",
    exitCode = 0,
    stdout = "done\n",
    stderr = "",
    success = true,
  }
}
```

Behavior:

- first rule whose `match` string appears inside executed command wins
- if `success` is omitted, runner derives it from `exitCode == 0`
- if no rule matches command, test runner fails command with `exitCode = 127`
- unmatched command stderr is `no fakeExec rule matched command: ...`

This affects both:

- `context.exec.run(...)`
- `reqpack.exec.run(...)`

If `plugin.init()` runs binary checks, include matching fake-exec rules in test cases or presets.

### `expect`

Common expectation keys:

```lua
expect = {
  success = true,
  commands = { "demo-pm install curl" },
  stdout = { "done\n" },
  stderr = {},
  events = { "installed", "success" },
  eventPayloads = {
    success = "ok",
  },
  resultCount = 1,
  resultName = "curl",
  resultVersion = "8.0.0",
}
```

Template ships starter core cases for:

- `install`
- `installLocal`
- `remove`
- `update`
- `list`
- `search`
- `info`
- `outdated`

## Wrapper Author Checklist

1. Edit `metadata.json` first.
2. Replace all `template` placeholders in code and tests.
3. Keep `reqpack.lua.depends` aligned with real ReqPack-side dependencies.
4. Add deterministic binary check in `plugin.init()` if needed.
5. Implement `getMissingPackages()` with real installed-state logic.
6. Use `context.exec.run(...)` in action methods.
7. Emit `context.tx.*` and `context.events.*` for visible behavior.
8. Update `.reqpack-test/core/*.lua` before expanding behavior further.
9. Run `rqp test-plugin --plugin . --preset core`.

## Full Docs

Read full repo docs when template-local reference still is not enough:

- `ReqPack.wiki/Extending-Writing-Lua-Plugins.md`
- `ReqPack.wiki/Extending-Testing-Lua-Plugins.md`
