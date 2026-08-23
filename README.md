# Tempify

Tempify generates project templates from Lua manifests, interactive questions, optional hooks, and embedded Prebyte rendering.

**Documentation:** [GitHub Wiki](https://github.com/Coditary/Tempify/wiki)

| Topic | Wiki page |
|-------|-----------|
| First render | [Getting Started](https://github.com/Coditary/Tempify/wiki/Getting-Started) |
| CLI & flags | [CLI Reference](https://github.com/Coditary/Tempify/wiki/CLI-Reference) |
| Write templates | [Your First Template](https://github.com/Coditary/Tempify/wiki/Your-First-Template) |
| Update projects | [Reapply & Diff](https://github.com/Coditary/Tempify/wiki/Reapply) |
| Install via ReqPack | [Installation](https://github.com/Coditary/Tempify/wiki/Installation) |
| Build & tests | [Development & Build](https://github.com/Coditary/Tempify/wiki/Development-and-Build) |

Prebyte is not vendored in this repo. On first configure, Tempify fetches a pinned Prebyte release from GitHub unless you set `TEMPIFY_PREBYTE_SOURCE_DIR`.

## Quick start

```bash
git clone https://github.com/Coditary/Tempify.git
cd Tempify
cmake -S . -B build
cmake --build build

./build/tempify list
./build/tempify basic_cpp my-app \
  --set project_name="My App" \
  --set name_slug=my-app \
  --set namespace=app_ns \
  --set include_ci=false \
  --set author=Me
```

Install locally:

```bash
cmake --install build --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
```

## Requirements

- C++23 compiler
- CMake 3.31+
- CLI11
- `git` (for first configure without a local Prebyte checkout)

Local Prebyte override:

```bash
cmake -S . -B build -DTEMPIFY_PREBYTE_SOURCE_DIR=/absolute/path/to/Prebyte
```

CMake presets (recommended for development):

```bash
cmake --preset dev
cmake --build --preset dev
make test
```

## ReqPack install

Release `.rqp` packages install the CLI, runtime, and bundled `tempify` plugin for template distribution.

```bash
rqp install rqp:tempify-cli --non-interactive
rqp install tempify basic_cpp
```

Details: [Installation](https://github.com/Coditary/Tempify/wiki/Installation) · [Releases](https://github.com/Coditary/Tempify/wiki/Releases) · [ReqPack Plugin](https://github.com/Coditary/Tempify/wiki/ReqPack-Plugin)

## Template layout (minimal)

```text
templates/<template-id>/
  template.lua
  questions.lua
  files/
```

Reference templates: `tests/test_templates/` ([Example Catalog](https://github.com/Coditary/Tempify/wiki/Example-Catalog))

## Development

```bash
make test
```

Test layout and fuzzing: [Tests & Quality](https://github.com/Coditary/Tempify/wiki/Tests-and-Quality)

ReqPack plugin checks:

```bash
rqp test-plugin --plugin . --preset core
```

## License

See repository license file.
