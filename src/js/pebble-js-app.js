var VERSION = "1.6.3";
var OPTIONS_STORAGE_KEY = "fuzzy-text-two-options-v2";
var CONFIG_URL = "https://www.gloriousedge.com/wordish/resources/configure-fuzzy-text-two.html";

var isReady = false;
var callbacks = [];
var pendingConfiguration = null;
var inFlightConfiguration = null;
var configurationRetryTimer = null;
var MAX_CONFIGURATION_ATTEMPTS = 3;

var alignments = {
  center: 0,
  left:   1,
  right:  2
};

var langs = {
  ca:    0,
  de:    1,
  en_GB: 2,
  en_US: 3,
  es:    4,
  fr:    5,
  no:    6,
  sv:    7
};

var fonts = {
  classic: 0,
  sharp:   1,
  compact: 2,
  tall:    3
};

var datePositions = {
  off:    0,
  top:    1,
  bottom: 2
};

var dateFormats = {
  dd_mm_yy:    0,
  mm_dd_yyyy: 1,
  mon_d_aug:  2,
  dd_slash_mm: 3,
  mm_slash_dd: 4
};

var colourValues = {
  black:     0x000000,
  white:     0xFFFFFF,
  red:       0xFF0000,
  orange:    0xFF5500,
  yellow:    0xFFFF00,
  green:     0x00AA00,
  mint:      0x55FFAA,
  cyan:      0x00AAFF,
  blue:      0x0055FF,
  deep_teal: 0x005555,
  maroon: 0x550000,
  forest: 0x005500,
  cream: 0xFFFFAA,
  peach: 0xFFAA55,
  lavender: 0xAAAAFF,
  rose: 0xFFAAFF,
  ice: 0xAAFFFF,
  slate: 0x555555,
  navy:      0x000055,
  plum:      0x550055,
  purple:    0xAA00FF,
  magenta:   0xFF00AA
};

var optionalColourValues = {
  match:     -1,
  black:     0x000000,
  white:     0xFFFFFF,
  red:       0xFF0000,
  orange:    0xFF5500,
  yellow:    0xFFFF00,
  green:     0x00AA00,
  mint:      0x55FFAA,
  cyan:      0x00AAFF,
  blue:      0x0055FF,
  deep_teal: 0x005555,
  maroon: 0x550000,
  forest: 0x005500,
  cream: 0xFFFFAA,
  peach: 0xFFAA55,
  lavender: 0xAAAAFF,
  rose: 0xFFAAFF,
  ice: 0xAAFFFF,
  slate: 0x555555,
  navy:      0x000055,
  plum:      0x550055,
  purple:    0xAA00FF,
  magenta:   0xFF00AA
};

function lookupValue(values, key, fallbackKey) {
  return Object.prototype.hasOwnProperty.call(values, key) ? values[key] : values[fallbackKey];
}

function readyCallback(event) {
  isReady = true;
  var callback;
  while (callbacks.length > 0) {
    callback = callbacks.shift();
    callback(event);
  }
}

function showConfiguration(event) {
  onReady(function() {
    var opts = getOptions();
    Pebble.openURL(CONFIG_URL + "?v=" + encodeURIComponent(VERSION) + "#options=" + encodeURIComponent(opts));
  });
}

function webviewclosed(event) {
  var resp = event && event.response;
  console.log('configuration response: '+ resp + ' ('+ typeof resp +')');

  var options = parseConfigurationResponse(resp);
  if (Object.keys(options).length === 0) {
    return;
  }

  // Migrate incoming legacy choices before merging, so an old page can still
  // change settings that a newer page previously saved under canonical names.
  // Explicit canonical fields in this response always take precedence.
  if (typeof options.invert !== 'undefined') {
    if (typeof options.foreground_colour === 'undefined') {
      options.foreground_colour = options.invert ? 'black' : 'white';
    }
    if (typeof options.background_colour === 'undefined') {
      options.background_colour = options.invert ? 'white' : 'black';
    }
  }
  var aliases = {
    minute_colour: ['row_one_colour', 'row_two_colour'],
    hour_colour: ['row_three_colour'],
    date_colour: ['date_part_one_colour', 'date_part_two_colour', 'date_part_three_colour']
  };
  Object.keys(aliases).forEach(function(alias) {
    if (typeof options[alias] !== 'undefined') {
      aliases[alias].forEach(function(key) {
        if (typeof options[key] === 'undefined') {
          options[key] = options[alias];
        }
      });
    }
  });

  onReady(function() {
    var stored = parseConfigurationResponse(getOptions());
    Object.keys(options).forEach(function(key) {
      stored[key] = options[key];
    });
    var serializedOptions = JSON.stringify(stored);
    setOptions(serializedOptions);

    var message = prepareConfiguration(serializedOptions);
    transmitConfiguration(message);
  });
}

// Only known values may reach storage or the watch. Webviews can return valid
// JSON primitives (including null) as well as malformed or cancelled responses.
function parseConfigurationResponse(resp) {
  var parsed;
  if (!resp) {
    return {};
  }

  try {
    parsed = JSON.parse(resp);
  }
  catch (err) {
    try {
      parsed = JSON.parse(decodeURIComponent(resp));
    }
    catch (decodeErr) {
      console.log('Unable to parse configuration response: ' + decodeErr.message);
      return {};
    }
  }
  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
    return {};
  }

  var options = {};
  var choices = {
    text_align: alignments,
    lang: langs,
    font: fonts,
    foreground_colour: colourValues,
    background_colour: colourValues,
    date_position: datePositions,
    date_format: dateFormats,
    row_one_colour: optionalColourValues,
    row_two_colour: optionalColourValues,
    row_three_colour: optionalColourValues,
    date_part_one_colour: optionalColourValues,
    date_part_two_colour: optionalColourValues,
    date_part_three_colour: optionalColourValues,
    minute_colour: optionalColourValues,
    hour_colour: optionalColourValues,
    date_colour: optionalColourValues
  };
  Object.keys(choices).forEach(function(key) {
    if (typeof parsed[key] === 'string' &&
        Object.prototype.hasOwnProperty.call(choices[key], parsed[key])) {
      options[key] = parsed[key];
    }
  });
  ['invert', 'animations_enabled', 'gesture_enabled',
    'low_battery_enabled', 'disconnect_enabled'].forEach(function(key) {
    if (parsed[key] === true || parsed[key] === false ||
        parsed[key] === 0 || parsed[key] === 1) {
      options[key] = !!parsed[key];
    }
  });
  if (parsed.exact_time_seconds === 3 || parsed.exact_time_seconds === 5 ||
      parsed.exact_time_seconds === 10) {
    options.exact_time_seconds = parsed.exact_time_seconds;
  }
  return options;
}

// Retrieves stored configuration from localStorage.
function getOptions() {
  try {
    return localStorage.getItem(OPTIONS_STORAGE_KEY) || "{}";
  }
  catch (err) {
    console.log('Unable to read options: ' + err.message);
    return "{}";
  }
}

// Stores options in localStorage.
function setOptions(options) {
  try {
    localStorage.setItem(OPTIONS_STORAGE_KEY, options);
  }
  catch (err) {
    console.log('Unable to store options: ' + err.message);
  }
}

// Takes a string containing serialized JSON as input.  This is the
// format that is sent back from the configuration web UI.  Produces
// a JSON message to send to the watch face.
function prepareConfiguration(serialized_settings) {
  var settings = parseConfigurationResponse(serialized_settings);

  settings.lang = settings.lang || "en_GB";
  settings.text_align = settings.text_align || "center";
  settings.font = settings.font || "classic";
  settings.date_position = settings.date_position || "off";
  settings.date_format = settings.date_format || "dd_mm_yy";

  var foreground = settings.foreground_colour || (settings.invert ? "black" : "white");
  var background = settings.background_colour || (settings.invert ? "white" : "black");
  var rowOne = settings.row_one_colour || settings.minute_colour || "match";
  var rowTwo = settings.row_two_colour || settings.minute_colour || "match";
  var rowThree = settings.row_three_colour || settings.hour_colour || "match";
  var datePartOne = settings.date_part_one_colour || settings.date_colour || "match";
  var datePartTwo = settings.date_part_two_colour || settings.date_colour || "match";
  var datePartThree = settings.date_part_three_colour || settings.date_colour || "match";
  return {
    "0": settings.invert ? 1 : 0,
    "1": lookupValue(alignments, settings.text_align, "center"),
    "2": lookupValue(langs, settings.lang, "en_GB"),
    "3": lookupValue(fonts, settings.font, "classic"),
    "4": lookupValue(colourValues, foreground, "white"),
    "5": lookupValue(colourValues, background, "black"),
    "6": lookupValue(datePositions, settings.date_position, "off"),
    "7": lookupValue(dateFormats, settings.date_format, "dd_mm_yy"),
    "8": lookupValue(optionalColourValues, rowOne, "match"),
    "9": lookupValue(optionalColourValues, rowTwo, "match"),
    "10": lookupValue(optionalColourValues, rowThree, "match"),
    "11": lookupValue(optionalColourValues, datePartOne, "match"),
    "12": lookupValue(optionalColourValues, datePartTwo, "match"),
    "13": lookupValue(optionalColourValues, datePartThree, "match"),
    "14": settings.animations_enabled === false ? 0 : 1,
    "15": settings.gesture_enabled === false ? 0 : 1,
    "16": settings.exact_time_seconds || 3,
    "17": settings.low_battery_enabled ? 1 : 0,
    "18": settings.disconnect_enabled ? 1 : 0
  };
}

// Keep only the latest unsent settings, and never overlap AppMessage requests.
function transmitConfiguration(settings) {
  pendingConfiguration = { settings: settings, attempts: 0 };
  if (configurationRetryTimer !== null) {
    clearTimeout(configurationRetryTimer);
    configurationRetryTimer = null;
  }
  sendPendingConfiguration();
}

// Retry transient failures twice, after one and two seconds. A newer save
// replaces a waiting retry and follows any in-flight delivery in order.
function sendPendingConfiguration() {
  if (inFlightConfiguration || !pendingConfiguration || configurationRetryTimer !== null) {
    return;
  }
  var delivery = pendingConfiguration;
  pendingConfiguration = null;
  var attempt = {};
  inFlightConfiguration = attempt;
  delivery.attempts++;

  function delivered() {
    if (inFlightConfiguration !== attempt) {
      return;
    }
    inFlightConfiguration = null;
    sendPendingConfiguration();
  }

  function failed(event) {
    if (inFlightConfiguration !== attempt) {
      return;
    }
    inFlightConfiguration = null;
    logError(event);
    if (pendingConfiguration) {
      sendPendingConfiguration();
    }
    else if (delivery.attempts < MAX_CONFIGURATION_ATTEMPTS) {
      pendingConfiguration = delivery;
      configurationRetryTimer = setTimeout(function() {
        configurationRetryTimer = null;
        sendPendingConfiguration();
      }, delivery.attempts * 1000);
    }
    else {
      console.log('Configuration delivery stopped after ' + delivery.attempts + ' attempts; saved settings will retry on the next launch.');
    }
  }

  console.log('sending message: ' + JSON.stringify(delivery.settings));
  try {
    Pebble.sendAppMessage(delivery.settings, delivered, failed);
  }
  catch (err) {
    failed({ error: err });
  }
}

function logError(event) {
  var transactionId = event && event.data ? event.data.transactionId : 'unknown';
  var message = event && event.error ? event.error.message : 'unknown';
  console.log('Unable to deliver message with transactionId='+
              transactionId +' ; Error is '+ message);
}

function onReady(callback) {
  if (isReady) {
    callback();
  }
  else {
    callbacks.push(callback);
  }
}

Pebble.addEventListener("ready", readyCallback);
Pebble.addEventListener("showConfiguration", showConfiguration);
Pebble.addEventListener("webviewclosed", webviewclosed);

onReady(function(event) {
  var message = prepareConfiguration(getOptions());
  transmitConfiguration(message);
});
