# Move Everything - SConnect

SConnect is a Spotify Connect receiver module for [Move Everything](https://github.com/charlesvestal/move-anything) on Ableton Move.
It is built on `librespot`.

Load it in a sound generator slot, then select the Move from the Spotify app as a playback device.

## Important Use Policy

SConnect is intended for personal listening only.

Supported use:
- Stream playback to your own Move device for listening.

Not supported:
- Recording, sampling, ripping, downloading, or exporting streamed audio
- Creating permanent copies of streamed content
- Public/commercial playback (stores, venues, client work, or broadcast use)
- Any use that violates Spotify terms or copyright law

This project is an unofficial community module and is not affiliated with, endorsed by, or sponsored by Spotify.

## What Works Right Now

- Spotify Connect playback through the Move signal chain
- Built on `librespot` as the Spotify Connect receiver backend
- Device name shown in Spotify defaults to `Move Everything` (or your custom name)
- Track metadata display in module UI:
  - `Track`
  - `Artist`
- Quality selection from module UI: `Low (96)`, `Medium (160)`, `High (320)`
- Quality setting is persisted and restored when reopening the module
- `[Reset Auth]` button clears Spotify credentials/token cache and restarts

## Requirements

- Move Everything host (v0.3.0+)
- Docker (for cross-build via `scripts/build.sh`)
- `jq` and `ripgrep` (for local test scripts)
- Move and your Spotify controller (phone/computer) on the same network

## Build

```bash
./scripts/build.sh
```

This builds `librespot` for ARM64 inside Docker, builds the DSP module, and packages:
- `dist/sconnect/`
- `dist/sconnect-module.tar.gz`

## Install

```bash
./scripts/install.sh
```

Default install target:
- Device: `root@move.local`
- Path: `/data/UserData/move-anything/modules/sound_generators/sconnect/`

If hostname-based install is flaky in your environment, manual copy to the same path also works.

## Usage

1. Build and install SConnect.
2. In Move Everything, load SConnect in a sound generator slot.
3. Open Spotify on your phone/computer.
4. Open the Spotify device picker.
5. Select the Move receiver shown by SConnect.
6. Start playback.

The SConnect module screen shows runtime state plus current track/artist when available.

## Buttons at the Bottom

- `[Reset Auth]`: clear Spotify auth/cache files and restart backend.

Use `Reset Auth` when device visibility/auth state gets stuck or after account/device changes.

## Quality and Stutter Tips

- `High (320)` gives best quality but needs the most network/CPU margin.
- If you hear occasional stutter, try `Medium (160)` first, then `Low (96)`.
- Changing quality restarts the backend; a brief reconnect is expected.

## Troubleshooting

- Device does not appear in Spotify:
  - Confirm Move and phone/computer are on the same network.
  - Reopen the module or use `[Reset Auth]`.
- Metadata shows `(none)`:
  - Nothing is currently playing, or playback event data has not arrived yet.
- Auth/control state seems broken:
  - Use `[Reset Auth]`, then reconnect in Spotify.
- Need runtime logs:
  - `ssh ableton@move.local 'tail -n 200 /data/UserData/move-anything/cache/sconnect-runtime.log'`

## Compliance Notes

- SConnect is a connector for playback only and intentionally does not provide recording/export tooling.
- You are responsible for using this module in compliance with Spotify terms and applicable copyright law.
- If you distribute modified versions, keep these restrictions visible in your docs/UI.

## Librespot Disclaimer

Using this code to connect to Spotify's API is probably forbidden by them.
Use at your own risk.

## Project Layout

- `scripts/`: build/install pipeline
- `scripts/build_librespot.sh`: pinned librespot checkout/build/stage flow
- `src/module.json`: module manifest
- `src/ui.js`: module UI behavior
- `src/dsp/sconnect_plugin.c`: DSP/runtime supervisor and audio path
- `src/runtime/sconnect_event.sh`: now-playing metadata hook
- `tests/`: scaffold/runtime/compat/metadata test scripts

## License

MIT

## Trademark Notice

Spotify is a trademark of Spotify AB. This project is independent and not affiliated with Spotify.
