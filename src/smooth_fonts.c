#include "smooth_fonts.h"

#ifdef WORDISH_SMOOTH_FONTS

static GFont fonts[2][2][3];
static const uint32_t resources[2][2][3] = {
    {
        { RESOURCE_ID_FONT_SMOOTH_CONDENSED_48_1, RESOURCE_ID_FONT_SMOOTH_CONDENSED_48_2, RESOURCE_ID_FONT_SMOOTH_CONDENSED_48_3 },
        { RESOURCE_ID_FONT_SMOOTH_BOLD_48_1, RESOURCE_ID_FONT_SMOOTH_BOLD_48_2, RESOURCE_ID_FONT_SMOOTH_BOLD_48_3 },
    },
    {
        { RESOURCE_ID_FONT_SMOOTH_CONDENSED_54_1, RESOURCE_ID_FONT_SMOOTH_CONDENSED_54_2, RESOURCE_ID_FONT_SMOOTH_CONDENSED_54_3 },
        { RESOURCE_ID_FONT_SMOOTH_BOLD_54_1, RESOURCE_ID_FONT_SMOOTH_BOLD_54_2, RESOURCE_ID_FONT_SMOOTH_BOLD_54_3 },
    },
};

GFont smooth_font_for_choice(FontChoice choice, bool bold)
{
    if (choice != FONT_CHOICE_LARGE && choice != FONT_CHOICE_MEDIUM) {
        return NULL;
    }
    int size = choice == FONT_CHOICE_LARGE;
    for (int level = 0; level < 3; level++) {
        if (!fonts[size][bold][level]) {
            fonts[size][bold][level] = fonts_load_custom_font(resource_get_handle(resources[size][bold][level]));
        }
        if (!fonts[size][bold][level]) {
            return NULL;
        }
    }
    return fonts[size][bold][0];
}

bool smooth_font_draw(GContext *ctx, const char *text, FontChoice choice, bool bold,
    GRect bounds, GTextAlignment alignment, GColor foreground, GColor background)
{
    if (!smooth_font_for_choice(choice, bold)) {
        return false;
    }
    int size = choice == FONT_CHOICE_LARGE;
    // Pebble colour channels have four levels. Composite each nested mask against
    // the actual configured background; the innermost mask uses full text colour.
    for (int level = 0; level < 3; level++) {
        int coverage = level + 1;
        GColor colour = foreground;
        colour.r = (foreground.r * coverage + background.r * (3 - coverage) + 1) / 3;
        colour.g = (foreground.g * coverage + background.g * (3 - coverage) + 1) / 3;
        colour.b = (foreground.b * coverage + background.b * (3 - coverage) + 1) / 3;
        graphics_context_set_text_color(ctx, colour);
        graphics_draw_text(ctx, text, fonts[size][bold][level], bounds,
            GTextOverflowModeTrailingEllipsis, alignment, NULL);
    }
    return true;
}

void smooth_fonts_unload(void)
{
    for (int size = 0; size < 2; size++) {
        for (int bold = 0; bold < 2; bold++) {
            for (int level = 0; level < 3; level++) {
                if (fonts[size][bold][level]) {
                    fonts_unload_custom_font(fonts[size][bold][level]);
                    fonts[size][bold][level] = NULL;
                }
            }
        }
    }
}
#endif
