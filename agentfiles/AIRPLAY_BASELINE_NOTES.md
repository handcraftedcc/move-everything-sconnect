# AirPlay Baseline Notes (Reference for Spotify Module)

## Purpose
This repository currently contains an AirPlay module used as a structural reference for building the Spotify receiver module.

## Top-Level Layout
- `README.md`: module overview, requirements, build/install, architecture
- `release.json`: release metadata (`version`, `download_url`)
- `scripts/`: build and install pipeline
- `src/`: module manifest, UI, DSP implementation, help docs
- `tests/`: release metadata validation script

## Module Conventions to Mirror
- `src/module.json` defines:
  - stable module identity fields (`id`, `name`, `version`, `api_version`)
  - UI entrypoints (`ui`, `ui_chain`)
  - DSP binary (`dsp`)
  - capabilities and `chain_params`
- Sound generators are chainable and expose params through `capabilities.chain_params`.
- UI code uses Move shared menu/input utilities from `/data/UserData/move-anything/shared/*`.
- Runtime status is surfaced through host params (`host_module_get_param` / `host_module_set_param`).

## Build/Packaging Pattern
- `scripts/build.sh`
  - runs in Docker on host, then self-invokes inside container
  - cross-compiles for ARM64
  - builds backend dependency first, then DSP plugin
  - packages module assets into `dist/<module-id>/`
  - creates release tarball `dist/<output>.tar.gz`
- `scripts/install.sh`
  - copies built module to Move path:
    `/data/UserData/move-anything/modules/sound_generators/<module-id>/`

## Runtime/Architecture Pattern
- DSP plugin owns backend process lifecycle (spawn, monitor, terminate).
- Backend binary lives in `dist/<module-id>/bin` and is launched from module dir.
- Module handles audio FIFO/ring-buffer ingestion and renders in `render_block`.
- Lifecycle must avoid orphan processes and stale runtime artifacts.

## Release/Test Pattern
- `tests/test_release_metadata.sh` validates release metadata consistency:
  - `release.json.version` == `src/module.json.version`
  - `release.json.download_url` format is correct
  - release workflow contains metadata verification step

## Move Module Doc Notes (from docs/MODULES.md)
- Module package generally includes `module.json`, optional UI/help files, and binaries/scripts.
- Modules are organized by component type (sound generators, audio FX, MIDI FX).
- Lifecycle functions for DSP modules include creation, param changes, reset, and render callbacks.
- API v2 supports `chain_params` for Shadow hierarchy integration.

## Immediate Implications for Spotify Work
- Keep identical packaging/install shape so host integration is predictable.
- Keep a single backend supervisor owner in module runtime logic.
- Mirror status-driven UI pattern (explicit states mapped to host params).
- Preserve release metadata + validation flow from day one.
