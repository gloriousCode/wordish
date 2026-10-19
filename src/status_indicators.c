#include "status_indicators.h"

#define STATUS_ICON_WIDTH 14
#define STATUS_ICON_HEIGHT 14
#define STATUS_ICON_GAP 6
#define STATUS_BOTTOM_MARGIN 8

static Layer *status_layer;
static GRect status_screen;
static GColor status_colour;
static bool battery_subscribed;
static bool connection_subscribed;
static bool battery_warning;
static bool disconnect_warning;
static void (*status_changed)(void);

bool status_indicators_visible(void)
{
  return status_layer != NULL && (battery_warning || disconnect_warning);
}

static void update_status_layout(bool was_visible)
{
  if (status_layer == NULL) {
    return;
  }
  int width = STATUS_ICON_WIDTH;
  if (battery_warning && disconnect_warning) {
    width = (2 * STATUS_ICON_WIDTH) + STATUS_ICON_GAP;
  }
  layer_set_frame(status_layer, GRect((status_screen.size.w - width) / 2,
      status_screen.size.h - STATUS_ICON_HEIGHT - STATUS_BOTTOM_MARGIN,
      width, STATUS_ICON_HEIGHT));
  bool visible = status_indicators_visible();
  layer_set_hidden(status_layer, !visible);
  layer_mark_dirty(status_layer);
  if (visible != was_visible && status_changed != NULL) {
    status_changed();
  }
}

static void status_layer_update(Layer *layer, GContext *ctx)
{
  (void)layer;
  graphics_context_set_stroke_color(ctx, status_colour);
  graphics_context_set_fill_color(ctx, status_colour);
  int x = 0;
  if (battery_warning) {
    graphics_draw_rect(ctx, GRect(x, 3, 12, 8));
    graphics_fill_rect(ctx, GRect(x + 12, 5, 2, 4), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(x + 2, 5, 2, 4), 0, GCornerNone);
    x += STATUS_ICON_WIDTH + STATUS_ICON_GAP;
  }
  if (disconnect_warning) {
    graphics_draw_rect(ctx, GRect(x + 3, 1, 8, 12));
    graphics_draw_line(ctx, GPoint(x + 6, 10), GPoint(x + 8, 10));
    graphics_draw_line(ctx, GPoint(x, 13), GPoint(x + 13, 0));
  }
}

static void status_battery_handler(BatteryChargeState state)
{
  bool next_warning = battery_subscribed && state.charge_percent <= 20 && !state.is_charging;
  if (next_warning == battery_warning) {
    return;
  }
  bool was_visible = status_indicators_visible();
  battery_warning = next_warning;
  update_status_layout(was_visible);
}

static void status_connection_handler(bool connected)
{
  bool next_warning = connection_subscribed && !connected;
  if (next_warning == disconnect_warning) {
    return;
  }
  bool was_visible = status_indicators_visible();
  disconnect_warning = next_warning;
  update_status_layout(was_visible);
}

void status_indicators_init(Layer *parent, GRect bounds, void (*changed)(void))
{
  status_screen = bounds;
  status_changed = changed;
  status_layer = layer_create(GRect(0, 0, STATUS_ICON_WIDTH, STATUS_ICON_HEIGHT));
  if (status_layer != NULL) {
    layer_set_update_proc(status_layer, status_layer_update);
    layer_set_hidden(status_layer, true);
    layer_add_child(parent, status_layer);
  }
}

void status_indicators_configure(bool battery_enabled, bool disconnect_enabled, GColor colour)
{
  bool was_visible = status_indicators_visible();
  status_colour = colour;
  if (battery_enabled != battery_subscribed) {
    battery_subscribed = battery_enabled;
    if (battery_enabled) {
      battery_state_service_subscribe(status_battery_handler);
    } else {
      battery_state_service_unsubscribe();
    }
  }
  if (disconnect_enabled != connection_subscribed) {
    connection_subscribed = disconnect_enabled;
    if (disconnect_enabled) {
      connection_service_subscribe((ConnectionHandlers) {
        .pebble_app_connection_handler = status_connection_handler
      });
    } else {
      connection_service_unsubscribe();
    }
  }
  battery_warning = false;
  if (battery_enabled) {
    BatteryChargeState state = battery_state_service_peek();
    battery_warning = state.charge_percent <= 20 && !state.is_charging;
  }
  disconnect_warning = disconnect_enabled && !connection_service_peek_pebble_app_connection();
  update_status_layout(was_visible);
}

void status_indicators_deinit(void)
{
  if (battery_subscribed) {
    battery_state_service_unsubscribe();
  }
  if (connection_subscribed) {
    connection_service_unsubscribe();
  }
  battery_subscribed = false;
  connection_subscribed = false;
  battery_warning = false;
  disconnect_warning = false;
  status_changed = NULL;
  if (status_layer != NULL) {
    layer_destroy(status_layer);
    status_layer = NULL;
  }
}
