#pragma once

#include <pebble.h>

// Optional, event-driven indicators. The callback fires when their visibility changes.
void status_indicators_init(Layer *parent, GRect bounds, void (*changed)(void));
void status_indicators_configure(bool battery_enabled, bool disconnect_enabled, GColor colour);
bool status_indicators_visible(void);
void status_indicators_deinit(void);
