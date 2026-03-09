# Spotiplay Librespot Build + Supervisor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add librespot dependency build/staging and runtime supervisor skeleton for Spotiplay.

**Architecture:** Keep current module skeleton and render shell intact while swapping process lifecycle ownership from shairport-specific code to a first-pass librespot supervisor interface.

**Tech Stack:** Bash scripts, Docker build image, C DSP plugin runtime, shell tests.

---

### Task 1: Add failing librespot scaffold test

**Files:**
- Create: `tests/test_librespot_scaffold.sh`

**Step 1: Write the failing test**
- Assert `scripts/build_librespot.sh` exists and is executable.
- Assert default pinned version is `v0.5.0`.
- Assert `scripts/build.sh` invokes `scripts/build_librespot.sh`.
- Assert staging path references `dist/spotiplay/bin/librespot`.

**Step 2: Run test to verify it fails**
Run: `bash tests/test_librespot_scaffold.sh`
Expected: FAIL before implementation.

**Step 3: Write minimal implementation**
Add script and build references needed to satisfy expectations.

**Step 4: Run test to verify it passes**
Run: `bash tests/test_librespot_scaffold.sh`
Expected: PASS.

### Task 2: Implement librespot build script and build integration

**Files:**
- Create: `scripts/build_librespot.sh`
- Modify: `scripts/build.sh`
- Modify: `scripts/Dockerfile`
- Modify: `README.md`

**Step 1: Build script implementation**
- Add clone/fetch/checkout/build/copy flow.
- Add explicit failures for missing rust/cargo/output binary.
- Print version/commit/target.

**Step 2: Build pipeline integration**
- Call new script from `scripts/build.sh`.
- Ensure output lands in `dist/spotiplay/bin/librespot`.

**Step 3: Docker support**
- Ensure builder image has Rust + ARM64 target prerequisites.

**Step 4: Documentation**
- Update README with librespot build flow and current runtime status.

### Task 3: Replace runtime process lifecycle with supervisor skeleton

**Files:**
- Modify: `src/dsp/airplay_plugin.c`

**Step 1: Add supervisor interface functions**
- `supervisor_start`
- `supervisor_stop`
- `supervisor_restart`
- `supervisor_is_running`
- `supervisor_get_pid`
- `supervisor_get_state`
- `supervisor_clear_credentials`

**Step 2: Switch subprocess launch to librespot binary**
- Launch `module_dir/bin/librespot`.
- Configure pipe backend to FIFO path.
- Keep logs and state transitions explicit.

**Step 3: Wire params/state**
- Map `restart` and optional `reset_login` params to supervisor actions.
- Keep `status` getter aligned with runtime state.

### Task 4: Final verification

**Files:**
- Modify if needed: tests/docs based on verification outcomes.

**Step 1: Run tests**
- `bash tests/test_librespot_scaffold.sh`
- `bash tests/test_scaffold_identity.sh`
- `bash tests/test_release_metadata.sh`

**Step 2: Sanity search**
Run: `rg -n "shairport-sync|librespot|spotiplay" scripts src README.md tests`

**Step 3: Report output-backed status**
- Share pass/fail evidence and remaining gaps.
