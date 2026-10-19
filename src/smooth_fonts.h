#pragma once

#include <pebble.h>
#include "text_layout.h"

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
#define WORDISH_SMOOTH_FONTS

// The outer coverage mask is also the font used for layout measurement.
GFont smooth_font_for_choice(FontChoice choice, bool bold);
// Returns false for a system-font fallback or unavailable font resources.
bool smooth_font_draw(GContext *ctx, const char *text, FontChoice choice, bool bold,
    GRect bounds, GTextAlignment alignment, GColor foreground, GColor background);
void smooth_fonts_unload(void);
#endif
