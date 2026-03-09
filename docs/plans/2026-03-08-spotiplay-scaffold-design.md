# Spotiplay Scaffold Design

## Goal
Convert the imported AirPlay reference module into a Spotify receiver scaffold named `spotiplay` while preserving the proven Move module structure and build/install flow.

## Scope
- Rename outward-facing module identity and packaging metadata from AirPlay to Spotiplay.
- Keep existing DSP/backend implementation as a temporary placeholder for now.
- Keep repository layout and scripts aligned with the AirPlay template so future librespot integration can follow the same lifecycle/build conventions.

## Non-Goals
- No real librespot integration in this step.
- No Spotify auth, metadata, or audio path implementation in this step.
- No runtime behavior hardening beyond scaffold consistency updates.

## Architecture and Structure
- Keep top-level layout: `scripts/`, `src/`, `tests/`, `release.json`.
- Keep build/install packaging behavior, but point module id/output paths to `spotiplay`.
- Keep UI/help files as placeholder screens with Spotify-oriented text.
- Keep DSP source as placeholder implementation until Phase 2 backend swap.

## Data and Config Surfaces Updated
- `src/module.json`: `id`, `name`, `abbrev`, description, and defaults remain chainable sound generator-compatible.
- `release.json`: version-aligned download URL updated for the Spotiplay repo artifact name.
- `scripts/build.sh` + `scripts/install.sh`: dist folder names, module IDs, and banner strings updated.
- `README.md` + `src/help.json` + `src/ui.js`: user-facing identity text updated.

## Testing Strategy
- Add a focused identity test (`tests/test_scaffold_identity.sh`) that enforces scaffold naming expectations.
- Keep release metadata test and align expected URL naming.
- Run both tests to confirm scaffold consistency.

## Risks and Mitigation
- Risk: Placeholder DSP still references AirPlay internals.
- Mitigation: explicitly document placeholder status and defer backend replacement to next phase.
