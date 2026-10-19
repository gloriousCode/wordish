#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/smooth_fonts.c"

static int loads, unloads, draws;
static uint32_t fail_resource;
static GColor colours[3];
static GFont drawn_fonts[3];
static const GRect expected_bounds = { 4, 7, 180, 66 };
uint32_t resource_get_handle(uint32_t id) { return id; }
GFont fonts_load_custom_font(uint32_t id) {
    loads++;
    return id == fail_resource ? NULL : (GFont)(uintptr_t)id;
}
void fonts_unload_custom_font(GFont font) { assert(font); unloads++; }
void graphics_context_set_text_color(GContext *ctx, GColor colour) {
    (void)ctx;
    colours[draws] = colour;
}
void graphics_draw_text(GContext *ctx, const char *text, GFont font, GRect bounds,
    int overflow, GTextAlignment alignment, void *attributes) {
    (void)ctx;
    assert(strcmp(text, "twelve") == 0);
    assert(memcmp(&bounds, &expected_bounds, sizeof(bounds)) == 0);
    assert(overflow == GTextOverflowModeTrailingEllipsis);
    assert(alignment == GTextAlignmentCenter && attributes == NULL);
    drawn_fonts[draws++] = font;
}

int main(void) {
    GContext ctx = 0;
    for (int size = 0; size < 2; size++) {
        FontChoice choice = size ? FONT_CHOICE_LARGE : FONT_CHOICE_MEDIUM;
        for (int bold = 0; bold < 2; bold++) {
            GFont measured = smooth_font_for_choice(choice, bold);
            assert(measured != NULL);
            int loaded = loads;
            assert(smooth_font_for_choice(choice, bold) == measured && loads == loaded);
            for (int fg = 0; fg < 64; fg++) {
                for (int bg = 0; bg < 64; bg++) {
                    GColor foreground = { .argb = 0xc0 | fg };
                    GColor background = { .argb = 0xc0 | bg };
                    draws = 0;
                    assert(smooth_font_draw(&ctx, "twelve", choice, bold, expected_bounds,
                        GTextAlignmentCenter, foreground, background));
                    assert(draws == 3 && drawn_fonts[0] == measured);
                    assert(colours[2].argb == foreground.argb);
                    for (int level = 0; level < 3; level++) {
                        for (int shift = 0; shift <= 4; shift += 2) {
                            int f = (fg >> shift) & 3, b = (bg >> shift) & 3;
                            int c = (colours[level].argb >> shift) & 3;
                            assert(c == (f * (level + 1) + b * (2 - level) + 1) / 3);
                        }
                    }
                }
            }
        }
    }
    assert(loads == 12);
    smooth_fonts_unload();
    assert(unloads == 12);
    smooth_fonts_unload();
    assert(unloads == 12);
    assert(smooth_font_for_choice(FONT_CHOICE_SMALL, false) == NULL);
    fail_resource = RESOURCE_ID_FONT_SMOOTH_CONDENSED_48_2;
    draws = 0;
    assert(!smooth_font_draw(&ctx, "twelve", FONT_CHOICE_MEDIUM, false, expected_bounds,
        GTextAlignmentCenter, (GColor){ .argb = 255 }, (GColor){ .argb = 192 }));
    assert(draws == 0);
    smooth_fonts_unload();
    assert(unloads == 13);
    puts("Smooth font renderer: colour blending, shared layout, caching and failure fallback pass");
}
