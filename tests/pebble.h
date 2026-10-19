// Minimal host model for watchface state and animation lifecycle regressions.
#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
#define GPoint(x, y) ((GPoint){ x, y })
#define GRect(x, y, w, h) ((GRect){ { x, y }, { w, h } })
typedef int GColor;
#define GColorBlack 0
#define GColorWhite 1
#define GColorClear 2
static bool mock_round;
#define PBL_IF_ROUND_ELSE(a, b) (mock_round ? (a) : (b))
#define APP_LOG(...) ((void)0)
#define ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))
#define FONT_KEY_BITHAM_42_BOLD "42b"
#define FONT_KEY_BITHAM_42_LIGHT "42l"
#define FONT_KEY_GOTHIC_28_BOLD "28b"
#define FONT_KEY_GOTHIC_28 "28"
#define FONT_KEY_GOTHIC_24_BOLD "24b"
#define FONT_KEY_GOTHIC_24 "24"
#define FONT_KEY_GOTHIC_18_BOLD "18b"
#define FONT_KEY_GOTHIC_18 "18"
#define FONT_KEY_GOTHIC_14 "14"
#define GCornerNone 0
#define RESOURCE_ID_FONT_ROBOTO_BOLD_54 1
#define RESOURCE_ID_FONT_ROBOTO_CONDENSED_54 2
#define RESOURCE_ID_FONT_ROBOTO_BOLD_48 3
#define RESOURCE_ID_FONT_ROBOTO_CONDENSED_48 4
typedef const char *GFont;
typedef int GContext;
typedef enum { GTextAlignmentCenter, GTextAlignmentLeft, GTextAlignmentRight } GTextAlignment;
enum { GTextOverflowModeWordWrap, GTextOverflowModeTrailingEllipsis, AnimationCurveEaseIn, AnimationCurveEaseOut, MINUTE_UNIT };
typedef struct Layer { GRect frame; bool hidden; } Layer;
typedef struct { Layer layer; const char *text; GFont font; } TextLayer;
typedef struct Window Window;
typedef struct { void (*load)(Window *); void (*unload)(Window *); } WindowHandlers;
struct Window { Layer layer; WindowHandlers handlers; };
typedef struct Animation Animation;
typedef struct { void (*started)(Animation *, void *); void (*stopped)(Animation *, bool, void *); } AnimationHandlers;
struct Animation { AnimationHandlers handlers; void *context; bool scheduled; bool destroyed; };
typedef struct { Animation animation; } PropertyAnimation;
typedef int AccelAxisType;
typedef int TimeUnits;
typedef int AppMessageResult;
typedef int DictionaryIterator;
typedef struct { uint32_t key; union { uint8_t uint8; int32_t int32; } *value; } Tuple;
typedef struct { int delay_ms; } AppTimer;
typedef struct { uint8_t charge_percent; bool is_charging; bool is_plugged; } BatteryChargeState;
typedef struct { void (*pebble_app_connection_handler)(bool); } ConnectionHandlers;
static BatteryChargeState mock_battery = { 100, false, false };
static bool mock_connected = true;
static void (*mock_battery_handler)(BatteryChargeState);
static void (*mock_connection_handler)(bool);
static void (*mock_tap_handler)(AccelAxisType, int32_t);
static int mock_battery_subscriptions;
static int mock_connection_subscriptions;
static int mock_tap_subscriptions;
static int mock_timer_count;

static int mock_layer_updates;
static int mock_text_measurements;
static int mock_animation_creates;
static int mock_animation_destroys;
static PropertyAnimation mock_animations[256];
static bool mock_quiet_time;
static bool mock_24h;
static bool mock_persist_exists[128];
static int mock_persist_values[128];
static GRect layer_get_frame(Layer *l) { return l->frame; }
static GRect layer_get_bounds(Layer *l) { return l->frame; }
static void layer_set_frame(Layer *l, GRect r) { l->frame = r; mock_layer_updates++; }
static void layer_set_hidden(Layer *l, bool h) { l->hidden = h; mock_layer_updates++; }
static void layer_mark_dirty(Layer *l) { (void)l; mock_layer_updates++; }
static Layer *layer_create(GRect r) { Layer *l = calloc(1, sizeof(*l)); l->frame = r; return l; }
static void layer_destroy(Layer *l) { free(l); }
static void layer_add_child(Layer *p, Layer *l) { (void)p; (void)l; }
static void layer_set_update_proc(Layer *l, void (*f)(Layer *, GContext *)) { (void)l; (void)f; }
static TextLayer *text_layer_create(GRect r) { TextLayer *l = calloc(1, sizeof(*l)); l->layer.frame = r; return l; }
static void text_layer_destroy(TextLayer *l) { free(l); }
static Layer *text_layer_get_layer(TextLayer *l) { return &l->layer; }
static const char *text_layer_get_text(TextLayer *l) { return l->text; }
static void text_layer_set_text(TextLayer *l, const char *s) { l->text = s; mock_layer_updates++; }
static void text_layer_set_font(TextLayer *l, GFont f) { l->font = f; mock_layer_updates++; }
static void text_layer_set_text_color(TextLayer *l, GColor c) { (void)l; (void)c; mock_layer_updates++; }
static void text_layer_set_background_color(TextLayer *l, GColor c) { (void)l; (void)c; mock_layer_updates++; }
static void text_layer_set_text_alignment(TextLayer *l, GTextAlignment a) { (void)l; (void)a; mock_layer_updates++; }
static void text_layer_set_overflow_mode(TextLayer *l, int m) { (void)l; (void)m; mock_layer_updates++; }
static Animation *property_animation_get_animation(PropertyAnimation *p) { assert(p); assert(!p->animation.destroyed); return &p->animation; }
static PropertyAnimation *property_animation_create_layer_frame(Layer *l, const GRect *a, const GRect *b) { (void)l; (void)a; (void)b; assert(mock_animation_creates < 256); return &mock_animations[mock_animation_creates++]; }
static void property_animation_destroy(PropertyAnimation *p) { assert(p); assert(!p->animation.destroyed); assert(!p->animation.scheduled); p->animation.destroyed = true; mock_animation_destroys++; }
static void animation_set_duration(Animation *a, int d) { (void)a; (void)d; }
static void animation_set_delay(Animation *a, int d) { (void)a; (void)d; }
static void animation_set_curve(Animation *a, int c) { (void)a; (void)c; }
static void animation_set_handlers(Animation *a, AnimationHandlers h, void *c) { assert(!a->scheduled); a->handlers = h; a->context = c; }
static void animation_schedule(Animation *a) { assert(!a->destroyed); a->scheduled = true; }
static void mock_animation_stop(Animation *a, bool finished) { assert(!a->destroyed); if (!a->scheduled) return; a->scheduled = false; if (a->handlers.stopped) a->handlers.stopped(a, finished, a->context); }
static void animation_unschedule(Animation *a) { mock_animation_stop(a, false); }
static bool clock_is_24h_style(void) { return mock_24h; }
static bool quiet_time_is_active(void) { return mock_quiet_time; }
static bool gcolor_equal(GColor a, GColor b) { return a == b; }
static GFont fonts_get_system_font(const char *s) { return s; }
static GFont fonts_load_custom_font(int r) { const char *names[] = { "", "custom54b", "custom54l", "custom48b", "custom48l" }; return names[r]; }
static void fonts_unload_custom_font(GFont f) { (void)f; }
static int resource_get_handle(int r) { return r; }
static void graphics_context_set_stroke_color(GContext *c, GColor g) { (void)c; (void)g; }
static void graphics_draw_rect(GContext *c, GRect r) { (void)c; (void)r; }
static void graphics_fill_rect(GContext *c, GRect r, int radius, int corners) { (void)c; (void)r; (void)radius; (void)corners; }
static void graphics_draw_line(GContext *c, GPoint a, GPoint b) { (void)c; (void)a; (void)b; }
static void graphics_context_set_fill_color(GContext *c, GColor g) { (void)c; (void)g; }
static void graphics_fill_circle(GContext *c, GPoint p, int r) { (void)c; (void)p; (void)r; }
static GSize graphics_text_layout_get_content_size(const char *s, GFont f, GRect r, int m, GTextAlignment a)
{
  (void)m;
  (void)a;
  mock_text_measurements++;
  if (r.size.w < 1024) {
    return (GSize){ (int16_t)(strlen(s) * (strcmp(f, FONT_KEY_GOTHIC_14) == 0 ? 6 : 7)), 18 };
  }
  assert(m == GTextOverflowModeWordWrap);
  int glyph = strncmp(f, "custom54", 8) == 0 ? 21
      : strncmp(f, "custom48", 8) == 0 ? 19
      : strncmp(f, "42", 2) == 0 ? 17
      : strncmp(f, "28", 2) == 0 ? 14
      : strncmp(f, "24", 2) == 0 ? 12 : 8;
  int width = 0;
  for (int i = 0; s[i]; i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c & 0xC0) == 0x80) continue;
    if (c == ' ') width += glyph / 2;
    else if (strchr("iljtfI", c)) width += glyph * 2 / 3;
    else if (strchr("mwMW", c)) width += glyph * 4 / 3;
    else width += glyph;
  }
  // Reproduce the real Emery clipping that the estimated Bitham metrics missed.
  if (strncmp(f, "42", 2) == 0 && strcmp(s, "dos quarts") == 0) {
    width = 220;
  }
  if (strcmp(f, "custom54b") == 0 && (strcmp(s, "twelve") == 0 || strcmp(s, "eleven") == 0)) {
    width = 190;
  }
  return (GSize){ (int16_t)width, 18 };
}
static bool persist_exists(int k) { return mock_persist_exists[k]; }
static bool persist_read_bool(int k) { return mock_persist_values[k] != 0; }
static int persist_read_int(int k) { return mock_persist_values[k]; }
static void persist_write_int(int k, int v) { mock_persist_exists[k] = true; mock_persist_values[k] = v; }
static void app_timer_cancel(AppTimer *t) { assert(t); mock_timer_count--; free(t); }
static AppTimer *app_timer_register(int d, void (*f)(void *), void *c) { (void)f; (void)c; AppTimer *t = calloc(1, sizeof(AppTimer)); t->delay_ms = d; mock_timer_count++; return t; }
static Tuple *dict_read_first(DictionaryIterator *d) { (void)d; return NULL; }
static Tuple *dict_read_next(DictionaryIterator *d) { (void)d; return NULL; }
static Window *window_create(void) { Window *w = calloc(1, sizeof(Window)); w->layer.frame = GRect(0, 0, 144, 168); return w; }
static void window_set_background_color(Window *w, GColor c) { (void)w; (void)c; }
static void window_set_window_handlers(Window *w, WindowHandlers h) { w->handlers = h; }
static Layer *window_get_root_layer(Window *w) { return &w->layer; }
static void window_stack_push(Window *w, bool a) { (void)a; w->handlers.load(w); }
static void window_destroy(Window *w) { w->handlers.unload(w); free(w); }
static void app_message_register_inbox_received(void (*f)(DictionaryIterator *, void *)) { (void)f; }
static void app_message_register_inbox_dropped(void (*f)(AppMessageResult, void *)) { (void)f; }
static void app_message_deregister_callbacks(void) {}
static void app_message_open(int i, int o) { (void)i; (void)o; }
#ifdef WORDISH_GENTLE_GESTURE
static bool mock_light_on;
static void (*mock_backlight_handler)(bool);
static int mock_backlight_subscriptions;
static bool light_is_on(void) { return mock_light_on; }
static void backlight_service_subscribe(void (*handler)(bool)) { assert(!mock_backlight_handler); mock_backlight_handler = handler; mock_backlight_subscriptions++; }
static void backlight_service_unsubscribe(void) { assert(mock_backlight_handler); mock_backlight_handler = NULL; mock_backlight_subscriptions--; }
#else
static void accel_tap_service_subscribe(void (*f)(AccelAxisType, int32_t)) { assert(!mock_tap_handler); mock_tap_handler = f; mock_tap_subscriptions++; }
static void accel_tap_service_unsubscribe(void) { assert(mock_tap_handler); mock_tap_handler = NULL; mock_tap_subscriptions--; }
#endif
static void tick_timer_service_subscribe(TimeUnits u, void (*f)(struct tm *, TimeUnits)) { (void)u; (void)f; }
static void tick_timer_service_unsubscribe(void) {}
static void app_event_loop(void) {}

static BatteryChargeState battery_state_service_peek(void) { return mock_battery; }
static void battery_state_service_subscribe(void (*handler)(BatteryChargeState)) { assert(!mock_battery_handler); mock_battery_handler = handler; mock_battery_subscriptions++; }
static void battery_state_service_unsubscribe(void) { assert(mock_battery_handler); mock_battery_handler = NULL; mock_battery_subscriptions--; }
static bool connection_service_peek_pebble_app_connection(void) { return mock_connected; }
static void connection_service_subscribe(ConnectionHandlers handlers) { assert(!mock_connection_handler); mock_connection_handler = handlers.pebble_app_connection_handler; mock_connection_subscriptions++; }
static void connection_service_unsubscribe(void) { assert(mock_connection_handler); mock_connection_handler = NULL; mock_connection_subscriptions--; }
