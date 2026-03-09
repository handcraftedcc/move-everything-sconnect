import { MoveShift } from '/data/UserData/move-anything/shared/constants.mjs';

import { isCapacitiveTouchMessage } from '/data/UserData/move-anything/shared/input_filter.mjs';

import { createAction } from '/data/UserData/move-anything/shared/menu_items.mjs';
import { createMenuState, handleMenuInput } from '/data/UserData/move-anything/shared/menu_nav.mjs';
import { createMenuStack } from '/data/UserData/move-anything/shared/menu_stack.mjs';
import { drawStackMenu } from '/data/UserData/move-anything/shared/menu_render.mjs';

const SPINNER = ['-', '/', '|', '\\'];
const TRANSPORT_CONTROLS_VISIBLE = false;

let status = 'stopped';
let deviceName = 'Move';
let controlsEnabled = false;
let trackName = '';
let trackArtist = '';
let playbackEvent = '';
let quality = 320;
let shiftHeld = false;

let menuState = createMenuState();
let menuStack = createMenuStack();

let tickCounter = 0;
let spinnerTick = 0;
let spinnerFrame = 0;
let needsRedraw = true;

function refreshState() {
  const prevStatus = status;
  const prevControls = controlsEnabled;
  const prevTrackName = trackName;
  const prevTrackArtist = trackArtist;
  const prevQuality = quality;
  const prevPlaybackEvent = playbackEvent;

  status = host_module_get_param('status') || 'stopped';
  deviceName = host_module_get_param('device_name') || 'Move';
  controlsEnabled = host_module_get_param('controls_enabled') === '1';
  trackName = host_module_get_param('track_name') || '';
  trackArtist = host_module_get_param('track_artist') || '';
  playbackEvent = host_module_get_param('playback_event') || '';
  quality = parseInt(host_module_get_param('quality') || '320', 10);
  if (![96, 160, 320].includes(quality)) quality = 320;

  if (
    prevStatus !== status ||
    prevControls !== controlsEnabled ||
    prevTrackName !== trackName ||
    prevTrackArtist !== trackArtist ||
    prevQuality !== quality ||
    prevPlaybackEvent !== playbackEvent
  ) {
    rebuildMenu();
    needsRedraw = true;
  }
}

function statusLabel() {
  if (status === 'starting') return 'Starting receiver';
  if (status === 'waiting_for_spotify') return 'Waiting for Spotify app';
  if (status === 'authenticating') return 'Authenticating';
  if (status === 'ready') return 'Ready for playback';
  if (status === 'playing') return 'Receiving audio';
  if (status === 'stopped') return 'Stopped';
  if (status === 'error') return 'Error';
  return status;
}

function qualityLabel(value) {
  if (value === 96) return 'Low (96)';
  if (value === 160) return 'Medium (160)';
  return 'High (320)';
}

function nextQuality(value) {
  if (value === 96) return 160;
  if (value === 160) return 320;
  return 96;
}

function trackPrefix(value, maxLen) {
  if (!value) return '';
  if (value.length <= maxLen) return value;
  return `${value.slice(0, maxLen - 1)}…`;
}

function buildRootItems() {
  const items = [];

  items.push(createAction(`Name: ${deviceName}`, () => {}));

  items.push(createAction(`Track: ${trackName ? trackPrefix(trackName, 20) : '(none)'}`, () => {}));
  items.push(createAction(`Artist: ${trackArtist ? trackPrefix(trackArtist, 19) : '(none)'}`, () => {}));

  items.push(createAction(`[Quality: ${qualityLabel(quality)}]`, () => {
    const updated = nextQuality(quality);
    host_module_set_param('quality', String(updated));
    needsRedraw = true;
  }));

  if (TRANSPORT_CONTROLS_VISIBLE && controlsEnabled) {
    const playPauseLabel = playbackEvent === 'playing' ? '[Pause]' : '[Play/Pause]';
    items.push(createAction(playPauseLabel, () => {
      host_module_set_param('play_pause', '1');
      needsRedraw = true;
    }));

    items.push(createAction('[Next]', () => {
      host_module_set_param('next', '1');
      needsRedraw = true;
    }));

    items.push(createAction('[Previous]', () => {
      host_module_set_param('previous', '1');
      needsRedraw = true;
    }));
  } else if (TRANSPORT_CONTROLS_VISIBLE && !controlsEnabled) {
    items.push(createAction('[Enable Controls]', () => {
      host_module_set_param('enable_controls', '1');
      needsRedraw = true;
    }));
  }

  items.push(createAction('[Reset Auth]', () => {
    host_module_set_param('reset_auth', '1');
    needsRedraw = true;
  }));

  return items;
}

function rebuildMenu() {
  const items = buildRootItems();
  const current = menuStack.current();
  if (!current) {
    menuStack.push({
      title: 'SConnect',
      items,
      selectedIndex: 0
    });
    menuState.selectedIndex = 0;
  } else {
    current.title = 'SConnect';
    current.items = items;
    if (menuState.selectedIndex >= items.length) {
      menuState.selectedIndex = Math.max(0, items.length - 1);
    }
  }
  needsRedraw = true;
}

function currentFooter() {
  const waitingStates = status === 'starting' ||
    status === 'waiting_for_spotify' ||
    status === 'authenticating';
  const activity = waitingStates ? 'Working' : '';
  if (activity) return `${activity} ${SPINNER[spinnerFrame]}`;
  if (controlsEnabled) return 'Enhanced mode';
  if (status === 'ready' || status === 'playing') return 'Simple mode';
  return statusLabel();
}

globalThis.init = function () {
  status = 'stopped';
  deviceName = 'Move';
  controlsEnabled = false;
  trackName = '';
  trackArtist = '';
  playbackEvent = '';
  quality = 320;
  shiftHeld = false;

  menuState = createMenuState();
  menuStack = createMenuStack();
  tickCounter = 0;
  spinnerTick = 0;
  spinnerFrame = 0;
  needsRedraw = true;

  rebuildMenu();
};

globalThis.tick = function () {
  tickCounter = (tickCounter + 1) % 6;
  if (tickCounter === 0) {
    refreshState();
  }

  if (status === 'starting' || status === 'waiting_for_spotify' || status === 'authenticating') {
    spinnerTick = (spinnerTick + 1) % 3;
    if (spinnerTick === 0) {
      spinnerFrame = (spinnerFrame + 1) % SPINNER.length;
      needsRedraw = true;
    }
  } else {
    spinnerTick = 0;
  }

  if (needsRedraw) {
    const current = menuStack.current();
    if (!current) {
      rebuildMenu();
    }

    clear_screen();
    drawStackMenu({
      stack: menuStack,
      state: menuState,
      footer: currentFooter()
    });

    needsRedraw = false;
  }
};

globalThis.onMidiMessageInternal = function (data) {
  const statusByte = data[0] & 0xF0;
  const cc = data[1];
  const val = data[2];

  if (isCapacitiveTouchMessage(data)) return;

  if (statusByte === 0xB0 && cc === MoveShift) {
    shiftHeld = val > 0;
    return;
  }

  if (statusByte !== 0xB0) return;

  const current = menuStack.current();
  if (!current) {
    rebuildMenu();
    return;
  }

  const result = handleMenuInput({
    cc,
    value: val,
    items: current.items,
    state: menuState,
    stack: menuStack,
    onBack: () => {
      host_return_to_menu();
    },
    shiftHeld
  });

  if (result.needsRedraw) {
    needsRedraw = true;
  }
};

globalThis.onMidiMessageExternal = function (data) {
  /* No external MIDI handling needed */
};

globalThis.chain_ui = {
  init: globalThis.init,
  tick: globalThis.tick,
  onMidiMessageInternal: globalThis.onMidiMessageInternal,
  onMidiMessageExternal: globalThis.onMidiMessageExternal
};
