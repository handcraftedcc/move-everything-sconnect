# Spotiplay Librespot Build + Supervisor Design

## Goal
Add a real librespot dependency build path and replace the AirPlay process lifecycle with a first-pass librespot supervisor skeleton, while keeping the existing module scaffold stable.

## Scope
- Add `scripts/build_librespot.sh` with pinned default tag `v0.5.0` and ARM64 build output staging.
- Update build pipeline to include librespot binary in `dist/spotiplay/bin/librespot`.
- Replace shairport-specific runtime process management with librespot supervisor functions in DSP runtime.
- Preserve current ring-buffer/audio render shell as interim plumbing.

## Non-Goals
- No full Spotify auth/session UX.
- No metadata parsing/event bridge in this phase.
- No production-hard crash backoff policy yet.

## Architecture
- Build layer:
  - `scripts/build_librespot.sh` owns source checkout/fetch, tag checkout, cargo build, and binary copy.
  - `scripts/build.sh` orchestrates module packaging and calls the librespot build script.
- Runtime layer:
  - DSP plugin contains supervisor API wrappers (`start/stop/restart/is_running/get_pid/get_state/clear_credentials`).
  - librespot subprocess launched from `module_dir/bin/librespot` with pipe backend writing PCM to module FIFO.
  - Render callback continues to consume FIFO data and output audio/silence behavior.

## Error Handling
- Missing cargo/rustup/toolchain: explicit build script failure messages.
- Missing built binary after cargo build: explicit failure.
- Process spawn failure or unexpected exit: state transitions to `error` with host log details.

## Testing
- Add `tests/test_librespot_scaffold.sh` to validate:
  - `build_librespot.sh` exists
  - default pinned version `v0.5.0`
  - staging path `dist/spotiplay/bin/librespot`
  - `scripts/build.sh` calls `build_librespot.sh`
- Keep existing scaffold and release metadata tests passing.

## Risks
- librespot CLI flags may require adjustment per target build/runtime environment.
- Cross-compile requirements can evolve with upstream Rust crate dependencies.

## Mitigation
- Keep build version override via `LIBRESPOT_VERSION`.
- Log the exact command/tag/commit used for easier troubleshooting.
- Keep runtime supervisor interfaces explicit so behavior can be hardened incrementally.
