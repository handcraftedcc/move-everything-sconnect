# Tag Release Recipe (For Other Move Modules)

This is the minimum setup needed so pushing a tag like `v0.1.3` creates:
- a successful build in GitHub Actions
- a GitHub Release object (not just a tag)
- an uploaded module artifact tarball

Use this as a copy pattern for other modules.

## 1. Required file contract

- `src/module.json`
  - Must contain the module `version` string.
- `release.json`
  - Must contain:
    - `version` (same as `src/module.json`)
    - `download_url` in this format:
      `https://github.com/<owner>/<repo>/releases/download/v<version>/<artifact>.tar.gz`
- `scripts/build.sh`
  - Must produce the final tarball in `dist/<artifact>.tar.gz`.
- `scripts/Dockerfile`
  - Must include all native/cross deps used by the build.
- `.github/workflows/release.yml`
  - Must run on tag push (`v*`) and publish a release.

## 2. Workflow requirements (critical)

Your release workflow must include all of these:

1. Trigger on tags:
   - `on.push.tags: ['v*']`
2. Release write permission:
   - `permissions.contents: write`
3. Version guard:
   - check tag version (`${GITHUB_REF_NAME#v}`) equals `src/module.json` version
4. Metadata guard:
   - check `release.json.version` equals tag version
   - check `release.json.download_url` equals:
     `https://github.com/${GITHUB_REPOSITORY}/releases/download/v${TAG_VERSION}/<artifact>.tar.gz`
5. Build step:
   - run module build (Dockerized if cross-compiling)
6. Publish step:
   - `softprops/action-gh-release` (or equivalent)
   - upload `dist/<artifact>.tar.gz`

If step 6 is missing, you only get a tag and workflow run, not a release asset.

## 3. Build container requirements (cross builds)

For Rust/C cross builds (like librespot + DSP), ensure the Docker image includes:

- host compiler toolchain:
  - `gcc` (needed for Rust build scripts/proc-macro build steps)
- target cross compiler:
  - e.g. `gcc-aarch64-linux-gnu`
- target dev libraries:
  - e.g. `libssl-dev:arm64`, `libasound2-dev:arm64`
- rustup target:
  - e.g. `aarch64-unknown-linux-gnu`

In cross Rust builds, set:
- `CARGO_TARGET_<TARGET>_LINKER`
- `PKG_CONFIG_ALLOW_CROSS=1`
- `PKG_CONFIG_LIBDIR` and `PKG_CONFIG_PATH` to target pkg-config directory

Without those, OpenSSL detection usually fails in CI.

## 4. Release metadata tests (recommended)

Add script tests that fail fast when release wiring drifts:

- `tests/test_release_metadata.sh`
  - validates `release.json`
  - validates workflow contains:
    - metadata verification step
    - build step
    - release publish step
- `tests/test_scaffold_identity.sh`
  - validates module id/name + release URL shape
- `tests/test_librespot_scaffold.sh` (or module-specific build test)
  - validates build script and Docker cross prerequisites

Run these locally and in CI before tagging.

## 5. Tagging flow that works

1. Merge/push `main` with all release fixes.
2. Confirm versions:
   - `src/module.json.version == release.json.version == <tag without v>`
3. Create and push tag:
   - `git tag -a v0.1.3 -m "Release v0.1.3"`
   - `git push origin v0.1.3`
4. Confirm workflow succeeds.
5. Confirm release exists and contains artifact:
   - release URL: `https://github.com/<owner>/<repo>/releases/tag/v0.1.3`
   - asset URL: `.../releases/download/v0.1.3/<artifact>.tar.gz`

## 6. Common failure modes

- Tag exists but no release asset:
  - release action missing from workflow.
- Workflow fails before build:
  - version mismatch between tag/module/release.json.
- Build fails early with `linker cc not found`:
  - `gcc` missing in Dockerfile.
- Build fails on OpenSSL/pkg-config in cross mode:
  - missing `PKG_CONFIG_ALLOW_CROSS` and target pkg-config vars.
- Release URL mismatch:
  - repo moved/renamed, but `release.json.download_url` still points to old repo.

## 7. Reusable checklist

Before every release:

- [ ] `src/module.json.version` matches intended tag.
- [ ] `release.json.version` matches intended tag.
- [ ] `release.json.download_url` points to current repo + artifact name.
- [ ] `scripts/build.sh` outputs `dist/<artifact>.tar.gz`.
- [ ] `release.yml` has verify + build + publish steps.
- [ ] Release tests pass.
- [ ] Tag push run succeeds and release asset is present.
