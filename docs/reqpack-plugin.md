# Tempify ReqPack Plugin

ReqPack plugin for Tempify templates.

It treats Tempify templates like ReqPack-managed packages:

- reads one or more template catalogs from `context.repositories`
- merges duplicate template ids by repository priority
- installs templates into Tempify shared store
- keeps Tempify `index/templates.json` in sync
- exposes `search`, `info`, `list`, `install`, `installLocal`, `remove`, `update`, and `outdated`

## Scope

Current plugin scope is intentionally small:

- catalog format is JSON
- template sources use `source.type = "git"`
- local directory sources also work for tests and local development
- plugin manages Tempify templates, not Tempify binary installation
- state lives in Tempify data root under `templates/` and `index/`

## Catalog Format

Each repository entry points at JSON with shape:

```json
{
  "schemaVersion": 1,
  "templates": [
    {
      "id": "basic_cpp",
      "name": "Basic C++ App",
      "description": "Small starter template",
      "version": "1.1.0",
      "tags": ["cpp", "starter"],
      "source": {
        "type": "git",
        "url": "https://example.test/templates/basic-cpp.git",
        "ref": "v1.1.0",
        "subdir": "template"
      }
    }
  ]
}
```

Required fields:

- `id`
- `version`
- `source.type`
- `source.url`

## Repository Merge Rules

- plugin reads active `context.repositories` for system `tempify`
- higher `priority` wins for duplicate template ids
- same priority breaks ties by `repo.id`
- merged winner set is cached in `index/reqpack-available.json`

## Store Layout

Installed templates land in Tempify shared store:

```text
$XDG_DATA_HOME/tempify/
  templates/<id>/
  index/templates.json
  index/reqpack-available.json
  index/reqpack-installed.json
```

Fallback root is `$HOME/.local/share/tempify`.

## Testing

Run full plugin conformance suite from plugin root:

```bash
rqp test-plugin --plugin . --preset core
```

Core tests use local fixture catalogs and seeded Tempify store snapshots under `.reqpack-test/fixtures/`.

## Files

- `run.lua`: plugin implementation
- `metadata.json`: bundle metadata, plugin id `tempify`
- `reqpack.lua`: ReqPack bundle manifest
- `scripts/install.lua`, `scripts/remove.lua`: required hook stubs
- `.reqpack-test/core/*.lua`: hermetic ReqPack plugin cases
- `.reqpack-test/fixtures/`: local catalogs, repos, and seeded store fixtures
- `docs/reqpack-plugin-api.md`: plugin-specific runtime notes

## Notes

- `plugin.init()` currently requires `git` on host
- catalog refresh happens lazily during query and mutate actions
- local fixture flags like `data-root=@tmp/store` exist mainly for hermetic plugin tests
