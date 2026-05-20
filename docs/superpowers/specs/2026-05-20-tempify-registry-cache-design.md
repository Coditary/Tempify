# Tempify Registry Cache And ReqPack Install Design

Status: Proposed
Date: 2026-05-20

## Goal

Support this flow:

- `rqp install tempify <template-id>` installs one Tempify template into shared store.
- `tempify list` shows local templates and registry-backed templates from a local available-cache.
- `tempify info <template-id>` can show metadata for cached registry templates even when they are not installed yet.

## Decisions

- ReqPack Tempify plugin stays install engine for registry-backed templates.
- ReqPack-distributed Tempify CLI package must use package id `tempify-cli`, while template/plugin system stays `tempify`, to avoid ReqPack package/system name collision.
- Tempify ships one built-in default template repository source instead of mutating user ReqPack config.
- User-configured ReqPack repositories remain additive and may override built-in entries by priority.
- Existing Tempify cache files remain source of truth:
  - `index/reqpack-available.json` for available templates
  - `index/reqpack-installed.json` for installed templates
  - `index/templates.json` for Tempify shared-store index
- `tempify` CLI reads available-cache for discovery only.
- `tempify <template-id> <target>` remains local-only render flow. It does not auto-install missing templates.
- Registry schema stays aligned with current plugin catalog schema instead of inventing a second registry format.

## Non-Goals

- No automatic template install during render.
- No new remote database or service.
- No hidden network activity during render.
- No replacement of ReqPack repository handling.
- No large refactor of Tempify catalog loading for commands that need a real local template root.

## Repositories

### `tempify-registry`

Purpose: small remote catalog repo for available Tempify templates.

Initial contents:

- `templates.json`
- `README.md`

Initial `templates.json` shape:

```json
{
  "schemaVersion": 1,
  "templates": [
    {
      "id": "java-xyz",
      "name": "Java XYZ",
      "description": "Small Java starter template",
      "version": "0.1.0",
      "tags": ["java", "starter"],
      "source": {
        "type": "git",
        "url": "../tempify-templates",
        "subdir": "java-xyz"
      }
    }
  ]
}
```

Notes:

- Local-dev variant uses relative path to sibling repo for fast testing.
- Published variant can switch `source.url` to GitHub repo URL later without schema change.
- Registry intentionally stays flat and small. One `templates.json` file is enough for first pass.

## Built-In Default Repository Source

Tempify package should ship a small built-in repository-source manifest used by the Tempify ReqPack plugin.

Purpose:

- make `rqp install tempify <template-id>` work out of the box
- avoid writing into user `~/.config/reqpack/config.lua`
- still allow users to add their own registries through normal ReqPack config
- keep published ReqPack CLI package entry separate as `tempify-cli` so `tempify` keeps resolving to Tempify plugin system

Suggested installed asset:

```text
<tempify-runtime>/share/tempify/default-template-repositories.json
```

Suggested shape:

```json
{
  "schemaVersion": 1,
  "repositories": [
    {
      "id": "tempify-default",
      "url": "https://github.com/Coditary/tempify-registry/raw/main/templates.json",
      "priority": 0,
      "enabled": true
    }
  ]
}
```

Local development variant may point this built-in file at local `tempify-registry/templates.json`.

### `tempify-templates`

Purpose: source repo that holds actual template content.

Initial contents:

- `java-xyz/`
  - `template.lua`
  - `questions.lua`
  - `files/README.md.pbt`

Initial template should be minimal and renderable, similar in spirit to existing test starter templates:

- id: `java-xyz`
- version: `0.1.0`
- one or two simple questions
- one generated README file

## Current State

Relevant existing behavior:

- ReqPack Tempify plugin already refreshes and persists available template data in `index/reqpack-available.json`.
- ReqPack Tempify plugin already installs templates into shared store and updates `index/templates.json` and `index/reqpack-installed.json`.
- Tempify CLI currently only sees:
  - workspace templates
  - installed shared-store templates
- Tempify CLI currently throws `Template not found: <id>` when a template is neither local nor installed.
- Plugin `getMissingPackages()` currently returns every request, which means planning/install behavior is less accurate than it should be.

## Desired User Flows

### 1. Install One Template Through ReqPack

User runs:

```bash
rqp install tempify java-xyz --non-interactive
```

Expected behavior:

1. ReqPack chooses `tempify` plugin.
2. Plugin builds effective repository list from:
   - built-in Tempify default repository source
   - user-provided ReqPack repositories for system `tempify`
3. Plugin refreshes registry catalog from that effective repository list.
4. Plugin updates local available-cache at `index/reqpack-available.json`.
5. Plugin resolves `java-xyz`.
6. Plugin fetches template source from `tempify-templates`.
7. Plugin installs template into Tempify shared store.
8. Plugin updates installed-cache and shared index.
9. Template becomes renderable through normal `tempify` commands.

### 2. List Templates Through Tempify

User runs:

```bash
tempify list
tempify list --json
```

Expected behavior:

- Workspace templates are shown.
- Installed shared templates are shown.
- Registry-backed templates from `index/reqpack-available.json` are shown even if not installed.
- One visible record per template id.
- Priority stays:
  - workspace
  - installed shared template
  - available cached template

Status model:

- `workspace`
- `installed`
- `available`

Fresh-state behavior:

- If `index/reqpack-available.json` is missing, `tempify list` still works for workspace and installed templates.
- Registry-backed templates are simply absent until ReqPack has populated the available-cache.
- No network fetch is attempted from `tempify list` in first pass.

### 3. Inspect Template Metadata Through Tempify

User runs:

```bash
tempify info java-xyz
tempify info java-xyz --json
```

Expected behavior:

- If template exists locally or is installed, current rich manifest-based info flow stays primary.
- If template is not installed but exists in available-cache, Tempify returns cache-backed metadata instead of failing immediately.
- Cache-backed info is metadata-only. It does not attempt to load `template.lua` from a remote source.

### 4. Render Installed Template

User runs:

```bash
tempify java-xyz my-app --set project_name="My App"
```

Expected behavior:

- Works only after template exists locally or in shared store.
- Missing template still errors cleanly.
- User installs missing template through ReqPack, not through render.

## Data Model

### Existing Files Reused

Shared data root:

```text
$XDG_DATA_HOME/tempify/
  templates/
  index/
    templates.json
    reqpack-available.json
    reqpack-installed.json
```

No new persistent file format is required for first pass.

### Available Cache Record

Tempify C++ side should consume the existing plugin-written data from `reqpack-available.json`.

Fields used by Tempify CLI:

- `id`
- `name`
- `description`
- `version`
- `tags`
- `source`
- `repository`

Tempify CLI does not need full source-resolution logic. It only needs enough metadata to show visible templates and fallback info.

## Plugin Changes

### Keep Existing Install Flow

ReqPack plugin already has correct high-level install behavior:

- refresh catalog
- resolve requested id
- fetch template source
- validate manifest
- install into shared store
- update installed/shared indexes

This design keeps that path as install source of truth.

### Improve `getMissingPackages()`

Implement real missing-package detection using installed state:

- if requested template id is not installed, return it as missing
- if requested version is specified and installed version differs, return it as missing
- if installed version already satisfies request, do not return it

Reason:

- better ReqPack planning
- avoids unnecessary reinstalls
- aligns plugin behavior with local installed database idea

### Keep Available Cache Fresh

`refresh_catalog()` remains responsible for updating `index/reqpack-available.json`.

First pass behavior stays simple:

- plugin merges built-in default repositories with `context.repositories`
- user repositories can override built-in ids through higher priority
- successful catalog refresh overwrites available-cache
- install/search/info/list/update/outdated actions can refresh cache through existing plugin flow
- Tempify CLI only reads cache; it does not own refresh in first pass

### Repository Merge Rules

Plugin effective repository inputs should be:

1. built-in default Tempify repositories
2. user-configured ReqPack repositories from `context.repositories`

Rules:

- dedupe by repository `id` + `url` is not required in first pass; simple append is acceptable
- template winner rules stay unchanged after catalog load:
  - higher repository `priority` wins
  - equal priority falls back to lexical `repo.id`
- built-in default repository should use low priority so user sources can override it cleanly

## Tempify CLI Changes

### New Read Model For Available Templates

Add a small C++ reader for `index/reqpack-available.json`.

This should stay separate from `LocalTemplateStore` because:

- `LocalTemplateStore` represents installed shared templates with real paths
- available-cache records may not exist on disk locally
- render/validate/inspect/test commands still require a real local root

Suggested small structs:

```cpp
struct AvailableTemplateRecord {
    std::string id;
    std::string name;
    std::string description;
    std::string version;
    std::vector<std::string> tags;
    std::string source_url;
    std::string source_ref;
    std::string source_subdir;
    std::string repository_id;
};
```

```cpp
enum class VisibleTemplateStatus {
    Workspace,
    Installed,
    Available,
};
```

### `tempify list`

Current local catalog build stays intact for commands that need real roots.

For listing only:

1. build local catalog as today
2. load available-cache records
3. merge by id using priority order
4. print visible records with status

Text output should add status column so cached-only records are not misleading.

Example text output:

```text
basic_cpp	Basic C++ App	Small C++ starter	workspace
java-xyz	Java XYZ	Small Java starter template	available
```

JSON output should preserve current shape and add fields instead of replacing it.

Additional fields per item:

- `status`
- `installed`
- `root` nullable or omitted when not local

### `tempify info`

Behavior order:

1. direct local path if provided
2. local/workspace/shared template via current catalog
3. available-cache fallback
4. unavailable error

Cache-backed text info should remain brief and explicit, for example:

- template id
- name
- description
- version
- availability: `available (registry cache)`
- repository id
- source URL/ref/subdir

Cache-backed JSON info should use a metadata-only shape and include:

- `template_id`
- `name`
- `description`
- `version`
- `availability`
- `repository`
- `source_url`
- `source_ref`
- `source_subdir`

### No Change To Render Resolution

`resolve_template_root()` should stay strict and local-only.

Reason:

- keeps render deterministic
- avoids hidden installs
- preserves existing behavior for validate/inspect/lint/test/render flows

## Output And UX Rules

- `tempify list` must never claim a cached-only template is installed.
- `tempify info` must clearly distinguish installed/local metadata from cache-only metadata.
- duplicate ids must still prefer local materialized templates over cached available records.
- workspace override behavior remains unchanged.

## Local Development Setup

Local development should work without GitHub publishing.

Expected setup:

1. `tempify-registry/templates.json` points at `../tempify-templates`
2. Tempify built-in default repository source points at local `tempify-registry/templates.json`
3. developer runs `rqp install tempify java-xyz`
4. shared store under local Tempify data root receives installed template

This gives a fast edit-test loop for both repos before moving sources to GitHub.

## Testing

### Template And Registry Assets

Create real starter content in the new repos:

- `tempify-registry/templates.json`
- `tempify-templates/java-xyz/*`

These are demo/dev assets, not replacements for hermetic fixtures.

### ReqPack Plugin Tests

Add coverage for:

- installing `java-xyz` from registry-backed catalog
- merging built-in default repository source with explicit fixture repositories

Hermetic `.reqpack-test` fixtures should remain primary for core behavior.
If a new smoke-style case points at `tempify-registry/templates.json`, keep it separate from the smallest core cases so fixture stability remains high.

### Tempify C++ Tests

Add integration tests for:

- `tempify list --json` includes available-cache entries
- local/install precedence over available-cache entries with same id
- `tempify info --json` falls back to available-cache metadata
- missing available-cache does not break `tempify list`

## Risks And Mitigations

### Risk: Cached Registry Becomes Stale

Impact:

- `tempify list` may show old metadata until ReqPack refreshes cache again

Mitigation:

- make cache refresh part of normal ReqPack query/mutate flows
- document that available-cache is local snapshot, not live remote state

### Risk: Monorepo Template Source Clones More Than Needed

Impact:

- `source.subdir` still requires cloning full `tempify-templates` repo

Mitigation:

- acceptable for first small starter repo
- registry schema already supports moving to one-template-per-repo later without CLI changes

### Risk: CLI Output Becomes Ambiguous

Impact:

- users may think `available` means installed

Mitigation:

- explicit `status`
- explicit installed boolean in JSON
- explicit fallback wording in `info`

## Scope Boundary

This design is intentionally limited to:

- one small registry repo
- one small template repo with `java-xyz`
- better ReqPack install planning for Tempify plugin
- Tempify discovery of cached available templates

It does not include:

- automatic install during render
- registry write commands
- mutating user ReqPack config automatically
- registry publishing automation beyond what already exists in Tempify release pipeline
