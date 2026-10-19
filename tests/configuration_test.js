'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { test } = require('node:test');

const root = path.join(__dirname, '..');
const bridgeSource = fs.readFileSync(path.join(root, 'src/js/pebble-js-app.js'), 'utf8');
const html = fs.readFileSync(path.join(root, 'resources/configure-fuzzy-text-two.html'), 'utf8');
const pageSource = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const plain = value => JSON.parse(JSON.stringify(value));

function bridge(stored = '{}', unavailableStorage = false, manualDelivery = false) {
  const handlers = {};
  const messages = [];
  const writes = [];
  const urls = [];
  const deliveries = [];
  const timers = new Map();
  let nextTimerId = 0;
  const context = vm.createContext({
    console: { log() {} },
    setTimeout(callback, delay) {
      const id = ++nextTimerId;
      timers.set(id, { callback, delay });
      return id;
    },
    clearTimeout(id) { timers.delete(id); },
    localStorage: {
      getItem() {
        if (unavailableStorage) throw new Error('Storage unavailable');
        return stored;
      },
      setItem(key, value) {
        if (unavailableStorage) throw new Error('Storage unavailable');
        writes.push({ key, value });
        stored = value;
      }
    },
    Pebble: {
      addEventListener(name, callback) { handlers[name] = callback; },
      sendAppMessage(message, success, failure) {
        messages.push(plain(message));
        deliveries.push({ success, failure });
        if (!manualDelivery) success();
      },
      openURL(url) { urls.push(url); }
    }
  });
  vm.runInContext(bridgeSource, context);
  return {
    context, handlers, messages, writes, urls, deliveries, timers,
    runNextTimer() {
      const next = timers.entries().next().value;
      assert.ok(next, 'A retry timer is scheduled');
      timers.delete(next[0]);
      next[1].callback();
      return next[1].delay;
    }
  };
}

// Model just the form APIs used by this page; derive choices from the real HTML
// so missing controls and invalid option values fail these behaviour checks.
function page(options, rawHash) {
  const elements = [];
  function element(attributes = {}) {
    return Object.assign({
      value: '', checked: false, disabled: false, style: {}, className: '',
      textContent: '', children: [], listeners: {},
      addEventListener(name, callback) { this.listeners[name] = callback; },
      appendChild(child) { this.children.push(child); }
    }, attributes);
  }
  for (const match of html.matchAll(/<select\b([^>]*)>([\s\S]*?)<\/select>/g)) {
    const choices = Array.from(match[2].matchAll(/<option value="([^"]*)"([^>]*)>/g));
    elements.push(element({
      name: match[1].match(/name="([^"]*)"/)[1],
      id: match[1].match(/id="([^"]*)"/)[1],
      value: (choices.find(choice => /selected/.test(choice[2])) || choices[0])[1],
      options: choices.map(choice => ({ value: choice[1] }))
    }));
  }
  for (const match of html.matchAll(/<input\b([^>]*)>/g)) {
    elements.push(element({
      name: match[1].match(/name="([^"]*)"/)[1],
      value: match[1].match(/value="([^"]*)"/)[1],
      checked: /\bchecked\b/.test(match[1])
    }));
  }
  for (const match of html.matchAll(/\bid="([^"]*)"/g)) {
    if (!elements.some(node => node.id === match[1])) elements.push(element({ id: match[1] }));
  }
  elements.push(element({ href: 'pebblejs://close' }));
  function querySelectorAll(selector) {
    const match = selector.match(/^\[(name|href)="([^"]+)"\](:checked)?$/);
    assert.ok(match, 'Supported selector: ' + selector);
    return elements.filter(node => node[match[1]] === match[2] && (!match[3] || node.checked));
  }
  const document = {
    querySelectorAll,
    querySelector(selector) { return querySelectorAll(selector)[0]; },
    getElementById(id) { return elements.find(node => node.id === id); },
    createElement() { return element(); }
  };
  const context = vm.createContext({
    document,
    window: { location: { hash: rawHash === undefined ? '#options=' + encodeURIComponent(JSON.stringify(options)) : rawHash, search: '' } }
  });
  vm.runInContext(pageSource, context);
  return { context, document, field: name => document.querySelector('[name="' + name + '"]') };
}

test('bridge ignores cancelled, malformed and non-object responses without writing or sending', () => {
  const { handlers, messages, writes } = bridge();
  handlers.ready();
  const originalCount = messages.length;
  for (const response of [undefined, '', 'CANCELLED', '%', 'null', 'false', '12', '"hello"', '[]', '{}', '{"font":"invalid"}', '{"preset":"paper"}']) {
    handlers.webviewclosed({ response });
  }
  assert.equal(messages.length, originalCount);
  assert.equal(writes.length, 0);
});

test('bridge accepts encoded settings and preserves existing settings on a partial update', () => {
  const { handlers, messages, writes } = bridge('{"lang":"fr","background_colour":"blue"}');
  handlers.ready();
  handlers.webviewclosed({ response: encodeURIComponent('{"font":"compact","lang":"invalid","unknown":123}') });
  assert.deepEqual(JSON.parse(writes[0].value), { lang: 'fr', background_colour: 'blue', font: 'compact' });
  assert.equal(messages.at(-1)['2'], 5);
  assert.equal(messages.at(-1)['3'], 2);
  assert.equal(messages.at(-1)['5'], 0x0055ff);
});

test('bridge falls back safely for corrupt storage and supports unavailable storage', () => {
  for (const stored of ['null', '[]', '42', '{', '{"font":"__proto__","invert":"false"}']) {
    const { handlers, messages } = bridge(stored);
    assert.doesNotThrow(() => handlers.ready());
    assert.equal(messages[0]['0'], 0);
    assert.equal(messages[0]['3'], 0);
  }
  const { handlers, messages } = bridge('{}', true);
  handlers.ready();
  handlers.webviewclosed({ response: '{"font":"tall"}' });
  assert.equal(messages.at(-1)['3'], 3);
});

test('bridge migrates legacy inversion and shared row/date colours', () => {
  const { context } = bridge();
  const settings = context.prepareConfiguration(JSON.stringify({
    invert: true, minute_colour: 'cyan', hour_colour: 'yellow', date_colour: 'red'
  }));
  assert.equal(settings['4'], 0);
  assert.equal(settings['5'], 0xffffff);
  assert.equal(settings['8'], 0x00aaff);
  assert.equal(settings['9'], 0x00aaff);
  assert.equal(settings['10'], 0xffff00);
  assert.equal(settings['11'], 0xff0000);
  assert.equal(settings['12'], 0xff0000);
  assert.equal(settings['13'], 0xff0000);
});

test('configuration page safely loads invalid JSON shapes, choices and URL escapes', () => {
  for (const options of [null, [], 42, 'hello', { lang: 'invalid', font: 'invalid', foreground_colour: 'invalid' }]) {
    const { context } = page(options);
    const saved = context.saveOptions();
    assert.equal(saved.lang, 'en_GB');
    assert.equal(saved.font, 'classic');
    assert.equal(saved.foreground_colour, 'white');
    assert.equal(saved.preset, 'classic');
  }
  assert.doesNotThrow(() => page({}, '#options=%E0%A4%A'));
});

test('configuration page preserves legacy inversion and colours on save', () => {
  const { context } = page({ invert: true, minute_colour: 'red', hour_colour: 'blue', date_colour: 'green' });
  const saved = context.saveOptions();
  assert.equal(saved.foreground_colour, 'black');
  assert.equal(saved.background_colour, 'white');
  assert.equal(saved.row_one_colour, 'red');
  assert.equal(saved.row_two_colour, 'red');
  assert.equal(saved.row_three_colour, 'blue');
  assert.equal(saved.date_part_three_colour, 'green');
  assert.equal(saved.preset, 'custom');
});

test('presets preserve language and update to custom after manual style changes', () => {
  const { context, field, document } = page({ lang: 'sv' });
  for (const name of Object.keys(context.presets)) {
    field('preset').value = name;
    field('preset').listeners.change.call(field('preset'));
    document.getElementById('config').listeners.change();
    assert.equal(context.saveOptions().preset, name);
    assert.equal(context.saveOptions().lang, 'sv');
  }
  field('background_colour').value = 'red';
  document.getElementById('config').listeners.change();
  assert.equal(context.saveOptions().preset, 'custom');
});

test('preview warns about low contrast and disables unused date controls without losing values', () => {
  const { context, field, document } = page({ date_part_three_colour: 'red' });
  assert.equal(field('date_format').disabled, true);
  assert.equal(field('date_part_one_colour').disabled, true);
  assert.equal(document.getElementById('contrast-warning').className, 'hidden');
  field('date_position').value = 'top';
  field('date_format').value = 'dd_slash_mm';
  context.updatePreview();
  assert.equal(field('date_part_one_colour').disabled, false);
  assert.equal(field('date_part_three_colour').disabled, true);
  assert.equal(context.saveOptions().date_part_three_colour, 'red');
  field('foreground_colour').value = 'black';
  context.updatePreview();
  assert.equal(document.getElementById('contrast-warning').className, '');
});

test('save and cancel retain supported Pebble and emulator return URLs', () => {
  for (const returnTo of [undefined, 'http://localhost:1234/return?settings=']) {
    const { context, document } = page({}, returnTo ? '#return_to=' + encodeURIComponent(returnTo) : '');
    document.getElementById('b-submit').listeners.click();
    const prefix = returnTo || 'pebblejs://close#';
    assert.equal(document.location, prefix + encodeURIComponent(JSON.stringify(context.saveOptions())));
    let prevented = false;
    document.querySelector('[href="pebblejs://close"]').listeners.click({ preventDefault() { prevented = true; } });
    assert.equal(prevented, !!returnTo);
    if (returnTo) assert.equal(document.location, returnTo);
  }
});

test('custom font glyph filters retain digits and colon for the exact time display', () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(root, 'appinfo.json'), 'utf8'));
  for (const resource of manifest.resources.media.filter(item => item.type === 'font')) {
    const pattern = new RegExp(resource.characterRegex);
    for (const glyph of '0123456789:') assert.ok(pattern.test(glyph), resource.name + ' missing ' + glyph);
  }
});


test('bridge maps new preferences with stable defaults and rejects invalid types', () => {
  const { context } = bridge();
  const defaults = context.prepareConfiguration('{}');
  assert.deepEqual([14, 15, 16, 17, 18].map(key => defaults[key]), [1, 1, 3, 0, 0]);
  for (const exact_time_seconds of [3, 5, 10]) {
    const settings = context.prepareConfiguration(JSON.stringify({
      animations_enabled: false, gesture_enabled: 0, exact_time_seconds,
      low_battery_enabled: true, disconnect_enabled: 1
    }));
    assert.deepEqual([14, 15, 16, 17, 18].map(key => settings[key]), [0, 0, exact_time_seconds, 1, 1]);
  }
  for (const value of ['false', 'true', 2, null, {}, []]) {
    const settings = context.prepareConfiguration(JSON.stringify({
      animations_enabled: value, gesture_enabled: value,
      low_battery_enabled: value, disconnect_enabled: value, exact_time_seconds: value
    }));
    assert.deepEqual([14, 15, 16, 17, 18].map(key => settings[key]), [1, 1, 3, 0, 0]);
  }
  assert.equal(context.prepareConfiguration('{"exact_time_seconds":"5"}')['16'], 3);
});

test('new configuration preferences retain defaults, round-trip and survive every style preset', () => {
  const preferences = {
    animations_enabled: false, gesture_enabled: false, exact_time_seconds: 10,
    low_battery_enabled: true, disconnect_enabled: true
  };
  const defaults = page({}).context.saveOptions();
  assert.deepEqual(Object.keys(preferences).map(key => defaults[key]), [true, true, 3, false, false]);
  const { context, field, document } = page(preferences);
  for (const name of Object.keys(context.presets)) {
    field('preset').value = name;
    field('preset').listeners.change.call(field('preset'));
    document.getElementById('config').listeners.change();
    const saved = context.saveOptions();
    for (const key of Object.keys(preferences)) assert.equal(saved[key], preferences[key]);
    assert.equal(field('exact_time_seconds').disabled, true);
  }
  field('gesture_enabled').checked = true;
  document.getElementById('config').listeners.change();
  assert.equal(field('exact_time_seconds').disabled, false);
  assert.equal(context.saveOptions().exact_time_seconds, 10);
  const invalid = page({ animations_enabled: 'false', gesture_enabled: [], exact_time_seconds: 9,
    low_battery_enabled: 'true', disconnect_enabled: 7 }).context.saveOptions();
  assert.deepEqual(Object.keys(preferences).map(key => invalid[key]), [true, true, 3, false, false]);
});

test('delivery recovers after a bounded retry and stops scheduling on success', () => {
  const state = bridge('{}', false, true);
  state.context.transmitConfiguration({ '3': 2 });
  state.deliveries[0].failure({ error: { message: 'temporarily disconnected' } });
  assert.equal(state.messages.length, 1);
  assert.equal(state.runNextTimer(), 1000);
  assert.deepEqual(state.messages, [{ '3': 2 }, { '3': 2 }]);
  state.deliveries[1].success();
  assert.equal(state.timers.size, 0);
  assert.equal(state.context.inFlightConfiguration, null);
  assert.equal(state.context.pendingConfiguration, null);
});

test('delivery exhausts three attempts, retains saved settings and permits a fresh save', () => {
  const state = bridge('{}', false, true);
  state.handlers.webviewclosed({ response: '{"font":"tall"}' });
  state.handlers.ready();
  state.deliveries[0].success();
  state.deliveries[1].failure();
  assert.equal(state.runNextTimer(), 1000);
  state.deliveries[2].failure();
  assert.equal(state.runNextTimer(), 2000);
  state.deliveries[3].failure();
  assert.equal(state.timers.size, 0);
  assert.equal(state.context.pendingConfiguration, null);
  assert.equal(JSON.parse(state.writes.at(-1).value).font, 'tall');
  state.handlers.webviewclosed({ response: '{"font":"compact"}' });
  assert.equal(state.messages.at(-1)['3'], 2);
  state.deliveries[4].success();
  assert.equal(state.context.inFlightConfiguration, null);
});

test('newer saves replace delayed retries and coalesce behind an in-flight message', () => {
  const state = bridge('{}', false, true);
  state.context.transmitConfiguration({ '3': 0 });
  state.deliveries[0].failure();
  state.context.transmitConfiguration({ '3': 1 });
  assert.equal(state.timers.size, 0);
  assert.deepEqual(state.messages, [{ '3': 0 }, { '3': 1 }]);
  state.context.transmitConfiguration({ '3': 2 });
  state.context.transmitConfiguration({ '3': 3 });
  assert.equal(state.messages.length, 2);
  state.deliveries[1].failure();
  assert.deepEqual(state.messages, [{ '3': 0 }, { '3': 1 }, { '3': 3 }]);
  assert.equal(state.timers.size, 0);
  state.deliveries[2].success();
  // Delayed duplicate callbacks must not resurrect the stale configuration.
  state.deliveries[0].failure();
  state.deliveries[1].success();
  assert.equal(state.messages.length, 3);
  assert.equal(state.timers.size, 0);
});

test('newer settings follow successful delivery in order and old callbacks cannot finish a retry', () => {
  const state = bridge('{}', false, true);
  state.context.transmitConfiguration({ '3': 0 });
  state.deliveries[0].failure();
  state.runNextTimer();
  state.deliveries[0].success();
  state.context.transmitConfiguration({ '3': 3 });
  assert.equal(state.messages.length, 2);
  state.deliveries[1].success();
  assert.deepEqual(state.messages, [{ '3': 0 }, { '3': 0 }, { '3': 3 }]);
  state.deliveries[2].success();
  assert.equal(state.context.inFlightConfiguration, null);
});

test('synchronous AppMessage exceptions use the same bounded retry path', () => {
  const state = bridge();
  let attempts = 0;
  state.context.Pebble.sendAppMessage = () => { attempts++; throw new Error('SDK unavailable'); };
  assert.doesNotThrow(() => state.context.transmitConfiguration({ '3': 1 }));
  assert.equal(state.runNextTimer(), 1000);
  assert.equal(state.runNextTimer(), 2000);
  assert.equal(attempts, 3);
  assert.equal(state.timers.size, 0);
  assert.equal(state.context.inFlightConfiguration, null);
});


test('manifest, bridge and configuration page agree on release identity and every protocol key', () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(root, 'appinfo.json'), 'utf8'));
  const { context } = bridge();
  assert.equal(context.VERSION, manifest.versionLabel);
  assert.equal(manifest.shortName, manifest.longName);
  assert.ok(html.includes('<title>Configure ' + manifest.longName + '</title>'));
  assert.ok(html.includes('<h1>Make ' + manifest.longName + ' yours</h1>'));
  const expected = {
    INVERT: [0, 0], TEXT_ALIGN: [1, 0], LANGUAGE: [2, 2], FONT_CHOICE: [3, 0],
    FOREGROUND_COLOR: [4, 0xffffff], BACKGROUND_COLOR: [5, 0],
    DATE_POSITION: [6, 0], DATE_FORMAT: [7, 0], ROW_ONE_COLOR: [8, -1],
    ROW_TWO_COLOR: [9, -1], ROW_THREE_COLOR: [10, -1], DATE_PART_ONE_COLOR: [11, -1],
    DATE_PART_TWO_COLOR: [12, -1], DATE_PART_THREE_COLOR: [13, -1],
    ANIMATIONS_ENABLED: [14, 1], GESTURE_ENABLED: [15, 1], EXACT_TIME_SECONDS: [16, 3],
    LOW_BATTERY_ENABLED: [17, 0], DISCONNECT_ENABLED: [18, 0]
  };
  const defaults = plain(context.prepareConfiguration('{}'));
  assert.deepEqual(Object.keys(manifest.appKeys).sort(), Object.keys(expected).sort());
  assert.deepEqual(Object.keys(defaults).sort(), Object.values(manifest.appKeys).map(String).sort());
  for (const [name, [id, value]] of Object.entries(expected)) {
    assert.equal(manifest.appKeys[name], id, name + ' ID');
    assert.equal(defaults[id], value, name + ' default');
  }
});


test('configuration URL versions the hosted page query while preserving saved options in the fragment', () => {
  const stored = JSON.stringify({ lang: 'fr', font: 'sharp', custom: 'spaces & accents: é' });
  const { handlers, context, urls } = bridge(stored);
  handlers.showConfiguration();
  assert.equal(urls.length, 0);
  handlers.ready();
  assert.equal(urls.length, 1);
  const url = new URL(urls[0]);
  assert.equal(url.origin + url.pathname,
    "https://www.gloriousedge.com/wordish/resources/configure-fuzzy-text-two.html");
  assert.equal(url.search, '?v=' + encodeURIComponent(context.VERSION));
  assert.equal(url.hash, '#options=' + encodeURIComponent(stored));
  assert.equal(decodeURIComponent(url.hash.slice('#options='.length)), stored);
  assert.equal(url.searchParams.has('options'), false);
});


test('incoming legacy aliases override saved canonical values while preserving newer preferences', () => {
  const preferences = {
    animations_enabled: false, gesture_enabled: false, exact_time_seconds: 10,
    low_battery_enabled: true, disconnect_enabled: true
  };
  const stored = Object.assign({
    foreground_colour: 'white', background_colour: 'black',
    row_one_colour: 'match', row_two_colour: 'match', row_three_colour: 'match',
    date_part_one_colour: 'match', date_part_two_colour: 'match', date_part_three_colour: 'match'
  }, preferences);
  for (const canonicalOverrides of [false, true]) {
    const state = bridge(JSON.stringify(stored));
    state.handlers.ready();
    const incoming = { invert: true, minute_colour: 'red', hour_colour: 'blue', date_colour: 'green' };
    if (canonicalOverrides) {
      Object.assign(incoming, { foreground_colour: 'purple', row_one_colour: 'yellow', date_part_two_colour: 'cyan' });
    }
    state.handlers.webviewclosed({ response: JSON.stringify(incoming) });
    const result = JSON.parse(state.writes.at(-1).value);
    assert.equal(result.foreground_colour, canonicalOverrides ? 'purple' : 'black');
    assert.equal(result.background_colour, 'white');
    assert.equal(result.row_one_colour, canonicalOverrides ? 'yellow' : 'red');
    assert.equal(result.row_two_colour, 'red');
    assert.equal(result.row_three_colour, 'blue');
    assert.equal(result.date_part_one_colour, 'green');
    assert.equal(result.date_part_two_colour, canonicalOverrides ? 'cyan' : 'green');
    assert.equal(result.date_part_three_colour, 'green');
    for (const key of Object.keys(preferences)) assert.equal(result[key], preferences[key]);
    assert.equal(state.messages.at(-1)['4'], canonicalOverrides ? 0xaa00ff : 0);
    assert.equal(state.messages.at(-1)['8'], canonicalOverrides ? 0xffff00 : 0xff0000);
    assert.equal(state.messages.at(-1)['12'], canonicalOverrides ? 0x00aaff : 0x00aa00);
    state.handlers.webviewclosed({ response: '{"invert":false}' });
    assert.equal(state.messages.at(-1)['4'], 0xffffff);
    assert.equal(state.messages.at(-1)['5'], 0);
  }
});

test('every preset has readable word and date contrast and valid bridge colours', () => {
  const { context, field, document } = page({});
  const bridgeState = bridge('{}');
  function contrast(a, b) {
    const first = context.luminance(context.previewColours[a]);
    const second = context.luminance(context.previewColours[b]);
    return (Math.max(first, second) + 0.05) / (Math.min(first, second) + 0.05);
  }
  for (const [name, preset] of Object.entries(context.presets)) {
    field('preset').value = name;
    field('preset').listeners.change.call(field('preset'));
    document.getElementById('config').listeners.change();
    assert.equal(document.getElementById('contrast-warning').className, 'hidden', name);
    for (const row of ['one', 'two', 'three']) {
      const chosen = preset['row_' + row + '_colour'];
      const colour = chosen === 'match' ? preset.foreground_colour : chosen;
      assert.ok(contrast(colour, preset.background_colour) >= 3, name + ' row ' + row);
      assert.equal(bridgeState.context.colourValues[colour], parseInt(context.previewColours[colour].slice(1), 16));
      if (preset.date_position !== 'off') {
        const dateChoice = preset['date_part_' + row + '_colour'];
        const dateColour = dateChoice === 'match' ? preset.foreground_colour : dateChoice;
        assert.ok(contrast(dateColour, preset.background_colour) >= 4.5, name + ' date ' + row);
      }
    }
  }
  for (const colour of ['navy', 'plum']) {
    const saved = page({ foreground_colour: colour, background_colour: colour,
      row_one_colour: colour, date_part_one_colour: colour }).context.saveOptions();
    assert.equal(saved.foreground_colour, colour);
    assert.equal(saved.background_colour, colour);
    assert.equal(saved.row_one_colour, colour);
    assert.equal(saved.date_part_one_colour, colour);
    assert.equal(bridgeState.context.colourValues[colour], bridgeState.context.optionalColourValues[colour]);
  }
});

test('undo restores the complete opened settings after preset and behaviour edits', () => {
  const { context, field, document } = page({ lang: 'fr', font: 'compact', text_align: 'right',
    foreground_colour: 'cream', background_colour: 'forest', row_one_colour: 'mint',
    date_position: 'off', date_format: 'mm_dd_yyyy', date_part_one_colour: 'rose',
    gesture_enabled: false, exact_time_seconds: 10, animations_enabled: false,
    low_battery_enabled: true, disconnect_enabled: true });
  const opened = JSON.stringify(context.saveOptions());
  assert.equal(document.getElementById('b-undo').disabled, true);
  field('preset').value = 'ember';
  field('preset').listeners.change.call(field('preset'));
  field('gesture_enabled').checked = true;
  field('low_battery_enabled').checked = false;
  document.getElementById('config').listeners.change();
  assert.equal(document.getElementById('b-undo').disabled, false);
  assert.match(document.getElementById('edit-status').textContent, /Unsaved changes/);
  document.getElementById('b-undo').listeners.click();
  assert.equal(JSON.stringify(context.saveOptions()), opened);
  assert.equal(field('exact_time_seconds').disabled, true);
  assert.equal(field('date_format').disabled, true);
  assert.equal(document.getElementById('b-undo').disabled, true);
  assert.equal(document.getElementById('edit-status').textContent, 'No unsaved changes');
});

test('matching all text to the default colour is previewed, saved and reversible', () => {
  const { context, field, document } = page({ foreground_colour: 'cream', background_colour: 'maroon',
    row_one_colour: 'peach', row_two_colour: 'rose', row_three_colour: 'white',
    date_part_one_colour: 'peach', date_position: 'top' });
  const opened = JSON.stringify(context.saveOptions());
  document.getElementById('b-match-colours').listeners.click();
  for (const key of ['row_one_colour', 'row_two_colour', 'row_three_colour',
    'date_part_one_colour', 'date_part_two_colour', 'date_part_three_colour']) {
    assert.equal(field(key).value, 'match');
  }
  assert.equal(document.getElementById('detail-row-one').style.color, '#ffffaa');
  assert.equal(document.getElementById('detail-colour-preview').style.backgroundColor, '#550000');
  assert.equal(document.getElementById('preview-row-one').style.color, '#ffffaa');
  document.getElementById('b-submit').listeners.click();
  const saved = JSON.parse(decodeURIComponent(document.location.split('#')[1]));
  assert.equal(saved.row_one_colour, 'match');
  document.getElementById('b-undo').listeners.click();
  assert.equal(JSON.stringify(context.saveOptions()), opened);
});
