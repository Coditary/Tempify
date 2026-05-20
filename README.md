# Tempify

Tempify erzeugt Projekt-Templates auf Basis von Lua-Manifests, Fragen, Hooks und eingebettetem Prebyte-Rendering.

Prebyte wird nicht im Repo vendort. Tempify zieht beim ersten Configure standardmaessig gepinntes Prebyte von GitHub und baut dagegen.

## Build

```bash
cmake -S . -B build
cmake --build build
```

Install lokal:

```bash
cmake --install build --prefix "$HOME/.local"
```

Voraussetzungen:

- C++23 Compiler
- CMake 3.31+
- `CLI11`
- `git` fuer ersten Configure ohne lokales Prebyte-Override

Lokales Prebyte-Override:

```bash
cmake -S . -B build -DTEMPIFY_PREBYTE_SOURCE_DIR=/abs/pfad/zu/Prebyte
```

## Release-Artefakte

Tag `v*`-Releases publizieren:

- Binary-Archive fuer Linux, macOS, Windows jeweils `x86_64` und `aarch64`
- `.rqp`-Pakete fuer Linux und macOS jeweils `x86_64` und `aarch64`
- `index.json` fuer ReqPack-Repository-Nutzung
- Docker-Image `ghcr.io/coditary/tempify` fuer `linux/amd64` und `linux/arm64`

Namensschema:

- `tempify-<version>-linux-x86_64.tar.gz`
- `tempify-<version>-linux-aarch64.tar.gz`
- `tempify-<version>-macos-x86_64.tar.gz`
- `tempify-<version>-macos-aarch64.tar.gz`
- `tempify-<version>-windows-x86_64.zip`
- `tempify-<version>-windows-aarch64.zip`
- `tempify-cli-<version>-linux-x86_64.rqp`
- `tempify-cli-<version>-linux-aarch64.rqp`
- `tempify-cli-<version>-macos-x86_64.rqp`
- `tempify-cli-<version>-macos-aarch64.rqp`

## ReqPack-Install

Tempify `.rqp` installiert:

- Runtime unter `$XDG_DATA_HOME/tempify/runtime/<version>-<release>+r<revision>/`
- stabilen Symlink `~/.local/bin/tempify`
- gebuendeltes ReqPack-Plugin `tempify` unter `$XDG_DATA_HOME/reqpack/plugins/tempify/`

Naming:

- ReqPack-CLI-Paketname ist `tempify-cli`
- ReqPack-Plugin-/Systemname fuer Templates bleibt `tempify`
- dadurch bleibt `rqp install tempify <template-id>` konfliktfrei

Beispiel mit lokalem Paket:

```bash
rqp install ./dist/tempify-cli-0.1.0-linux-x86_64.rqp --non-interactive
~/.local/bin/tempify --help
```

Beispiel mit Release-Repository-Index:

```bash
rqp install rqp:tempify-cli --non-interactive
```

Hinweis:

- dafuer muss Release-`index.json` als `rqp`-Repository in ReqPack konfiguriert sein
- Docker-Image enthaelt nur `tempify`, kein `rqp` und keine auto-installierte ReqPack-Plugin-Registrierung.

Weitere Doku:

- `docs/README.md`
- `docs/configuration.md`

## CLI

```bash
tempify <template-id> [target] [--set key=value ...] [--answers answers.json] [--write-answers answers.json] [-f|--overwrite-if-exists] [-s|--skip-if-file-exists] [--accept-hooks yes|ask|no] [--no-hooks] [--non-interactive] [--strict] [--hook-timeout-ms ms] [--diff|--reapply|--dry-run|--plan-json] [--tui]
tempify reapply <template-id> <target> [--set key=value ...] [--answers answers.json] [--non-interactive] [--strict] [--hook-timeout-ms ms] [--report] [--json] [--tui]
tempify <template-id> -q|--questions [--json] [--full]
tempify list
tempify refresh
tempify process [prebyte-args...]
tempify -p [prebyte-args...]
```

## Beispiele

```bash
# Lokale Templates anzeigen
./build/tempify list

# Template rendern
./build/tempify basic_cpp my-app \
  --set project_name="My App" \
  --set name_slug=my-app \
  --set namespace=app

# In existierenden Ordner rendern und Konflikte ueberschreiben
./build/tempify basic_cpp my-app -f \
  --set project_name="My App" \
  --set name_slug=my-app \
  --set namespace=app

# In existierenden Ordner rendern und bestehende Dateien behalten
./build/tempify basic_cpp my-app -s \
  --set project_name="My App" \
  --set name_slug=my-app \
  --set namespace=app

# Reapply-Plan als JSON ohne Writes
./build/tempify reapply basic_cpp my-app --report --json \
  --set project_name="My App" \
  --set name_slug=my-app \
  --set namespace=app

# Sichere Reapply-Aenderungen anwenden
./build/tempify reapply basic_cpp my-app --json \
  --set project_name="My App" \
  --set name_slug=my-app \
  --set namespace=app

# Fragen minimal anzeigen
./build/tempify advanced_hooks_layout --questions

# Fragen als JSON ausgeben
./build/tempify advanced_hooks_layout --questions --json

# Fragen mit allen Feldern ausgeben
./build/tempify advanced_hooks_layout --questions --full

# Shared-Template-Index neu aufbauen
./build/tempify refresh

# Prebyte-CLI durchreichen
./build/tempify process -h

# Prebyte-CLI Alias
./build/tempify -p -h
```

## Wichtige Befehle

- `list`: zeigt verfuegbare lokale Templates
- `<template-id> -q`, `<template-id> --questions`: zeigt standardmaessig knappe Fragen-Uebersicht
- `refresh`: baut Shared-Template-Index unter lokalem Tempify-Datenpfad neu auf
- `process`: reicht Restargumente an eingebettetes Prebyte weiter
- `--json`: gibt Fragen als JSON aus
- `--full`: zeigt auch leere Felder wie `choices: []` oder `help: ""`
- `--answers <path>`: laedt Fragenwerte aus JSON-Datei
- `--write-answers <path>`: schreibt aufgeloeste Fragenwerte als JSON-Datei
- `--non-interactive`: scheitert statt fehlende Werte interaktiv zu fragen
- `--strict`: lehnt unbekannte oder ungueltige importierte Werte ab
- `--set key=value`: setzt Template-Werte direkt ueber CLI
- `-f`, `--overwrite-if-exists`: erlaubt existierendes Ziel und ersetzt kollidierende Dateien
- `-s`, `--skip-if-file-exists`: erlaubt existierendes Ziel und ueberspringt kollidierende Dateien
- `--accept-hooks yes|ask|no`: steuert, ob Template-Hooks laufen
- `--no-hooks`: Alias fuer `--accept-hooks no`
- `--hook-timeout-ms <ms>`: bricht lange Hook-Phasen nach Timeout ab, `0` deaktiviert Timeout
- `--diff`: zeigt report-only Unterschiede fuer gemanagte Dateien, Origin-Metadaten und Update-Kind an und schreibt nichts
- `--reapply`: wendet nur sichere `create`/`update`/`delete`-Aenderungen auf existierende Ziele mit passender `.tempify-lock.json` an
- `--report`: gibt mit `--reapply` nur Report aus, ohne Writes
- `reapply`: top-level Alias fuer `tempify <template-id> <target> --reapply`
- `--dry-run`, `--plan-json`: zeigt Plan ohne Dateien zu schreiben
- `--tui`: nutzt Wizard-Frontend
- `-p`, `--prebyte`: Alias fuer `process`

Reapply-Hinweise:

- `--reapply` braucht explizites Ziel und vorhandene `.tempify-lock.json`
- `tempify reapply <template-id> <target>` nutzt exakt dieselbe Reapply-Logik wie Flag-Flow
- `--reapply` blockiert cross-template Reapply, wenn Lockfile-Template nicht zum angeforderten Template passt
- `--reapply` erlaubt Patch-/Prerelease-Upgrades, blockiert aber Major-Upgrades, Pre-1.0-Minor-Upgrades, Downgrades und nicht eindeutig vergleichbare Versionswechsel zur Review
- blockiert hart bei `conflict` oder `review`
- behaelt lokale Edits mit Aktion `keep`
- fuehrt in erstem Slice keine Hooks aus
- `--diff --json`, `--reapply --report --json` und `--reapply --json` zeigen Origin-Template, Origin-Version und Update-Kind
- JSON zeigt unter `update.policy` auch Action, Reason und Next-Step

`--diff` Textausgabe enthaelt ausserdem eine direkte Handlungsempfehlung je `update.kind`.

## Reapply JSON Beispiele

Erfolgreiches `--reapply --json`:

```json
{
  "status": "ok",
  "mode": "reapply",
  "template": {
    "id": "basic_cpp",
    "version": "0.1.0"
  },
  "origin": {
    "detected": true,
    "matches_requested_template": true,
    "matches_requested_version": false,
    "template_id": "basic_cpp",
    "template_version": "0.1.0-rc.1"
  },
  "update": {
    "kind": "upgrade",
    "from_version": "0.1.0-rc.1",
    "to_version": "0.1.0",
    "policy": {
      "action": "allow",
      "reason": "forward_version_change",
      "next_step": "Version upgrade detected. Review diff, then reapply ready actions to move managed files forward."
    }
  }
}
```

Blockiertes `--reapply --json` bei Template-Mismatch:

```json
{
  "status": "error",
  "code": "REAPPLY_BLOCKED",
  "blocked": {
    "conflict": [],
    "review": [
      ".tempify-lock.json"
    ],
    "origin_mismatch": {
      "lockfile": ".tempify-lock.json",
      "origin_template": {
        "id": "layered_cpp_product",
        "version": "0.1.0"
      },
      "requested_template": {
        "id": "basic_cpp",
        "version": "0.1.0"
      }
    },
    "version_transition": null
  }
}
```

Ausfuehrlichere Beispiele: `docs/reapply.md`

## Answers Und Config

Answers-Dateien:

- `--answers <path>` importiert Fragenwerte aus JSON
- `--write-answers <path>` exportiert aufgeloeste Fragenwerte fuer spaetere Replays
- `--strict` macht unbekannte oder ungueltige importierte Werte fatal

Config-Dateien:

- global: `$XDG_CONFIG_HOME/tempify/config.json`
- global fallback: `~/.config/tempify/config.json`
- workspace: naechste `.tempify/config.json`, aufwaerts ab aktuellem Arbeitsverzeichnis gesucht

Aktuelles Config-Schema:

```json
{
  "defaults": {
    "project_name": "My App",
    "include_ci": false
  },
  "render": {
    "accept_hooks": "ask",
    "hook_timeout_ms": 30000,
    "existing_path_behavior": "skip"
  }
}
```

Fragenwert-Prioritaet, niedrig nach hoch:

- Fragen-Defaults und abgeleitete Defaults
- Template-`.env`
- globale Config `defaults`
- Workspace-Config `defaults`
- `--answers`
- CLI `--set`

Render-Prioritaet, niedrig nach hoch:

- globale Config `render`
- Workspace-Config `render`
- explizite CLI-Flags

Hinweise:

- Config ist aktuell Soft-Default-Layer, kein Policy-Lockdown
- Answer-File-Werte schlagen Config-Defaults
- explizite CLI-Werte schlagen Config und Answer-File

## Shared Template Store

Tempify unterstuetzt zusaetzlich zu `./templates` auch einen lokalen Shared-Template-Store.

Pfad:

- wenn `XDG_DATA_HOME` gesetzt ist: `$XDG_DATA_HOME/tempify`
- sonst: `~/.local/share/tempify`

Struktur:

```txt
<shared-root>/
  templates/
  index/
    templates.json
```

Hinweise:

- Package Manager kann `templates/` und `index/templates.json` direkt verwalten
- `tempify refresh` scannt `templates/` und erzeugt `templates.json` selbst neu
- `list` zeigt Workspace-Templates und Shared-Templates zusammen an

## Template-Struktur

Ein Template besteht typischerweise aus:

```txt
templates/<template-id>/
  template.lua
  questions.lua
  files/
```

Optional koennen je nach Template auch `layout.lua`, Hook-Skripte und weitere Lua-Dateien vorkommen.

## Questions-Struktur

`questions.lua` ist jetzt gruppiert aufgebaut:

```lua
return {
  order = { "Project", "CI" },
  groups = {
    Project = {
      { key = "project_name", type = "string", prompt = "Project name" },
      { key = "project_slug", type = "string" },
    },
    CI = {
      { key = "include_ci", type = "bool", prompt = "Include CI?" },
    },
  },
}
```

Regeln:

- `order` bestimmt Reihenfolge der Gruppen
- `groups` enthaelt Fragen je Gruppe
- Fragen setzen `group` nicht mehr selbst; das kommt aus `groups.<name>`
- jede Gruppe muss in `order` stehen
- leere Gruppen sind ungueltig

## Tests

```bash
ctest --test-dir build --output-on-failure
```

Plugin-Checks:

```bash
rqp test-plugin --plugin . --preset core
```
