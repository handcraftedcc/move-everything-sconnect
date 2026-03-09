Here is a clean IMPLEMENTATION_PLAN.md companion document for the AI code agent.

IMPLEMENTATION_PLAN.md
Project
Move Everything – Spotify Receiver (librespot-based)
Goal
Implement a Spotify Connect receiver module for Move Everything that uses librespot as a supervised backend process. The module should appear as a Spotify playback target, support first-time authentication, persist login state, play audio through the Move audio path, and stop cleanly when the module unloads.

1. Implementation Philosophy
This project should optimize for:
working end-to-end playback first
low-risk integration
strong lifecycle handling
simple debugging
minimal abstraction in v1
Do not over-engineer a generic multi-backend network audio framework in v1.
The first successful version should be explicitly librespot-specific.

2. High-Level Build Order
Implement in this order:
Confirm Move Everything module structure
Create minimal module shell
Add librespot dependency build script
Launch librespot manually from module
Track process lifecycle
Make receiver discoverable in Spotify
Handle first-time auth/session persistence
Expose module UI states
Bridge audio into Move path
Add metadata/state monitoring
Add reset/restart actions
Harden crash/unload behavior
Test repeated load/unload/restart scenarios
Document install, build, and troubleshooting
Do not move to polish until step 9 works.

3. Phase 0 — Discovery / Validation
Task 0.1
Inspect the current Move Everything module conventions.
Determine:
expected repo layout
module.json structure
how modules are loaded/unloaded
how module settings are defined
how module UI state is updated
how audio-generating/receiving modules are expected to behave
whether module lifecycle callbacks exist for init / activate / deactivate / unload
Deliverable
A short internal notes file documenting:
lifecycle hooks
process-spawning options
audio path expectations
logging options

Task 0.2
Verify the easiest current librespot integration path for ARM Linux.
Determine:
recommended repo/tag for v1
required Rust toolchain version
whether librespot binary alone is enough
whether any runtime assets are needed
which auth mode is practical
how credentials/session are stored
whether metadata can be extracted from stdout/logs or requires another interface
Deliverable
A short integration note:
chosen librespot version
expected launch command pattern
auth strategy
runtime dependencies

4. Phase 1 — Minimal Module Skeleton
Task 1.1
Create the base module repo structure.
Suggested structure:
move-anything-spotify-receiver/
  README.md
  IMPLEMENTATION_PLAN.md
  module.json
  scripts/
    build_librespot.sh
  deps/
  bin/
  src/
    module_controller.*
    librespot_supervisor.*
    audio_bridge.*
    state_model.*
    logger.*
Use naming that matches the Move Everything ecosystem.

Task 1.2
Create a minimal loadable module that does nothing except:
load successfully
display a title
show a static status like Receiver Inactive
Acceptance
module installs
module appears in Move Everything
module can be loaded/unloaded without error

5. Phase 2 — librespot Dependency Build
Task 2.1
Implement scripts/build_librespot.sh.
Requirements:
clone librespot if absent
fetch updates if present
checkout configured version
build with Cargo
copy binary to bin/librespot
verify executable bit
print version/commit/target info
Inputs
Support:
pinned version by default
optional environment variable or config override for main or another tag
Example config concept:
LIBRESPOT_VERSION=vX.X.X

Task 2.2
Make the build fail clearly if:
Rust is missing
cargo fails
target architecture mismatch occurs
resulting binary is missing
Acceptance
clean checkout can build librespot
binary exists at expected path
module can detect its presence

6. Phase 3 — librespot Supervisor
Task 3.1
Create a dedicated supervisor component.
Suggested API:
start()
stop()
restart()
is_running()
get_pid()
get_state()
clear_credentials()
This component should own:
process handle
PID
launch command
shutdown behavior
restart logic
lock/singleton handling

Task 3.2
Implement process start.
Start librespot only when:
module is active
receiver mode is enabled
singleton lock is acquired
Do not start on passive existence alone.

Task 3.3
Implement process stop.
Shutdown order:
send SIGTERM
wait up to ~2 seconds
send SIGKILL if needed
release lock
clear internal process state
Acceptance
no orphaned librespot process remains after unload
repeated load/unload does not duplicate processes

7. Phase 4 — Single Instance Enforcement
Task 4.1
Add singleton protection.
Use one of:
lock file
PID file
local socket ownership
The module must guarantee:
only one active receiver per device
no duplicate Spotify device advertisements caused by duplicate librespot instances
Acceptance
starting a second instance does not silently spawn another backend
stale lock is recoverable after crash

8. Phase 5 — Auth / Session Flow
Task 5.1
Implement first-run auth detection.
On startup:
check whether librespot has usable stored credentials/session
if not, enter Not Logged In / Waiting for Spotify state
Task 5.2
Use librespot’s device-side login flow, preferably Spotify Connect/Zeroconf style.
The user flow should be:
module active
receiver advertising
user opens Spotify
user selects device
librespot authenticates
session persists locally
Task 5.3
Document and standardize the credentials/session storage path.
Module must support:
detect valid stored session
detect invalid/expired session
reset/delete stored session
Acceptance
first-time login works
session survives restart
reset login forces clean re-auth flow

9. Phase 6 — Runtime State Model
Task 6.1
Implement an explicit internal state model.
Suggested states:
UNINITIALIZED
INACTIVE
STARTING
WAITING_FOR_SPOTIFY
AUTHENTICATING
READY
BUFFERING
PLAYING
PAUSED
ERROR
STOPPING
Every transition should be logged.

Task 6.2
Map librespot process/runtime conditions into user-facing states.
Prioritize exposing:
backend running or not
auth required or not
waiting for selection
currently playing
paused
crashed
network/auth error
Acceptance
UI state always reflects a valid known module/backend state
no silent ambiguous state

10. Phase 7 — Module UI
Task 7.1
Build a minimal but useful UI.
Main page should show:
receiver status
device name
track title
artist
optional album
optional time/progress
Task 7.2
Add settings/actions:
device name
receiver enabled
restart receiver
reset Spotify login
debug logging level
Optional:
auto-start on activation
startup volume
fade duration
Acceptance
user can understand what the receiver is doing without looking at logs
key recovery actions are available from UI/settings

11. Phase 8 — Metadata / Event Monitoring
Task 8.1
Determine the simplest reliable way to get metadata and playback state from librespot.
Possible sources:
stdout/stderr parsing
a status IPC interface if available
wrapper-side event parsing
Prefer the least fragile method available in practice.
Task 8.2
Extract and expose:
title
artist
album if easy
playing/paused state
connection/auth state
Acceptance
metadata updates when tracks change
play/pause state is reflected in UI

12. Phase 9 — Audio Bridge
Task 9.1
Determine the cleanest audio handoff path from librespot into the Move Everything audio path.
Requirements:
stereo
non-blocking
silence when inactive
no audio-thread process/network work
safe shutdown
stable buffering
Task 9.2
Implement buffering and silence/fade behavior.
Requirements:
output silence when disconnected
brief fade in/out to avoid clicks
safe underrun behavior
audio path must not stall if backend exits
Acceptance
playback works
audio engine remains stable during start/stop/crash/unload
no hanging callback

13. Phase 10 — Lifecycle Hardening
Task 10.1
Ensure librespot only starts when module is truly active.
Not enough:
module exists in project
Required:
active/enabled/running module state
Task 10.2
Add startup debounce and shutdown stabilization.
Requirements:
avoid rapid spawn/kill loops
wait for cleanup before restart
tolerate fast module toggles
Task 10.3
Add crash restart backoff.
Example behavior:
first failure: short retry
repeated failures: increasing delay
after N failures: enter stable error state
Acceptance
rapid toggling does not create orphan processes or crash loops
repeated failure settles into readable error state

14. Phase 11 — Logging / Diagnostics
Task 11.1
Implement structured logging.
Log:
module load/unload
librespot start/stop
PID
state transitions
auth/session events
metadata changes
restart attempts
crash reasons if known
Task 11.2
Support log levels:
Off
Basic
Verbose
Verbose should include timestamps.
Acceptance
major failures are diagnosable from logs
state transitions are reconstructable from logs

15. Phase 12 — Error Handling
Task 12.1
Handle expected failures cleanly:
librespot binary missing
build output missing
backend launch failure
no network
auth failed
session expired
audio handoff failure
backend crash
singleton conflict
Task 12.2
Map these to simple messages:
Receiver not started
Spotify login required
Waiting for Spotify app
Network unavailable
Spotify session expired
Receiver crashed
Audio output error
Acceptance
failure conditions are visible and recoverable where possible

16. Phase 13 — Validation Checklist
Run the following test cases.
Build
clean checkout builds librespot successfully
pinned version checkout works
rebuild after version change works
Startup
module loads without starting unintended receivers
active module starts backend
inactive module does not
Auth
first login works
session persists across restart
reset login works
Playback
device appears in Spotify
playback transfer works
stereo audio plays
metadata updates
pause/play updates state
Lifecycle
unload stops librespot
no orphan after unload
reload does not duplicate process
rapid reload/unload remains stable
Failure cases
missing binary shows clear error
network loss handled cleanly
backend crash detected
repeated crashes do not cause infinite thrash

17. Suggested Milestones
Milestone 1
Minimal module shell loads successfully.
Milestone 2
librespot builds from source and can be launched manually.
Milestone 3
Module supervises librespot start/stop cleanly.
Milestone 4
Move appears as a Spotify device and auth flow works.
Milestone 5
Audio playback reaches Move audio path.
Milestone 6
Metadata and UI states work.
Milestone 7
Lifecycle hardening and crash handling complete.
Milestone 8
Docs and troubleshooting finished.

18. Deliverables
The AI code agent should produce:
module source code
librespot build script
installation instructions
configuration notes
troubleshooting notes
documented auth/session storage path
documented lifecycle behavior
clear known limitations for v1

19. Known Non-Goals
Do not spend v1 time on:
Spotify browsing UI
playlist editing
album art
AirPlay
Chromecast
generic multi-backend receiver abstraction
official Spotify hardware SDK integration
browser-based Spotify playback

20. Final Brief for the AI Code Agent
Build a librespot-specific Spotify Receiver module for Move Everything. Start with a minimal loadable module, then add a build script that fetches and compiles librespot from GitHub, supervise librespot as a subprocess, implement Spotify Connect device authentication/session persistence, expose receiver state and metadata in the UI, bridge audio safely into the Move audio path, enforce single-instance behavior, and ensure librespot stops when the module unloads. Prioritize end-to-end working playback and strong lifecycle behavior over abstraction.
EXTRA NOTES:
Github builds should happen via tag workflow.
Local builds are fine normal.,
When running any install.sh it needs to be run outside of the sandbox

