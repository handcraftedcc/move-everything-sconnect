# Spotiplay Scaffold Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Convert AirPlay-facing module scaffold to Spotiplay naming and packaging while preserving existing structure.

**Architecture:** Keep the current Move module template shape and lifecycle wiring intact, but rename all outward-facing metadata, packaging paths, and documentation to `spotiplay`. Treat the DSP/backend code as a temporary placeholder with no functional Spotify behavior yet.

**Tech Stack:** Bash scripts, JSON module metadata, JavaScript UI/help assets, shell-based repository tests.

---

### Task 1: Add failing scaffold identity test

**Files:**
- Create: `tests/test_scaffold_identity.sh`

**Step 1: Write the failing test**

```bash
#!/usr/bin/env bash
set -euo pipefail

module_json="src/module.json"
release_json="release.json"

[ "$(jq -r '.id' "$module_json")" = "spotiplay" ]
[ "$(jq -r '.name' "$module_json")" = "Spotiplay" ]
[ "$(jq -r '.download_url' "$release_json")" = "https://github.com/charlesvestal/move-anything-spotiplay/releases/download/v$(jq -r '.version' "$release_json")/spotiplay-module.tar.gz" ]
```

**Step 2: Run test to verify it fails**

Run: `bash tests/test_scaffold_identity.sh`
Expected: FAIL because module id/name and release URL still reference AirPlay.

**Step 3: Write minimal implementation**

Update module/release metadata to spotiplay values.

**Step 4: Run test to verify it passes**

Run: `bash tests/test_scaffold_identity.sh`
Expected: PASS.

**Step 5: Commit**

```bash
git add tests/test_scaffold_identity.sh src/module.json release.json
git commit -m "chore: add spotiplay scaffold identity checks"
```

### Task 2: Normalize scripts and docs to spotiplay scaffold

**Files:**
- Modify: `README.md`
- Modify: `scripts/build.sh`
- Modify: `scripts/install.sh`
- Modify: `src/help.json`
- Modify: `src/ui.js`
- Modify: `tests/test_release_metadata.sh`

**Step 1: Write/update failing expectation checks**

Add/adjust checks in test scripts so tarball/module IDs and release URL naming are spotiplay-specific.

**Step 2: Run tests to verify failures before implementation**

Run: `bash tests/test_scaffold_identity.sh && bash tests/test_release_metadata.sh`
Expected: at least one FAIL before all updates are complete.

**Step 3: Implement minimal updates**

- Rename build/install output/module id references from `airplay` to `spotiplay`
- Update README/help/UI labels to Spotiplay wording
- Keep runtime backend implementation unchanged

**Step 4: Run tests to verify passing**

Run: `bash tests/test_scaffold_identity.sh && bash tests/test_release_metadata.sh`
Expected: PASS.

**Step 5: Commit**

```bash
git add README.md scripts/build.sh scripts/install.sh src/help.json src/ui.js tests/test_release_metadata.sh
git commit -m "chore: normalize scaffold naming for spotiplay"
```

### Task 3: Final verification of scaffold baseline

**Files:**
- Modify: `agentfiles/AIRPLAY_BASELINE_NOTES.md` (if needed)

**Step 1: Verify build/install script references**

Run: `rg -n "airplay|spotiplay" README.md scripts src tests release.json`
Expected: AirPlay references only where intentionally preserved as placeholder runtime internals.

**Step 2: Verify tests pass**

Run: `bash tests/test_scaffold_identity.sh`
Run: `bash tests/test_release_metadata.sh`
Expected: both PASS.

**Step 3: Verify git status is clean and scoped**

Run: `git status --short`
Expected: only intended scaffold/documentation changes.

**Step 4: Commit**

```bash
git add -A
git commit -m "chore: prepare spotiplay scaffold baseline"
```
