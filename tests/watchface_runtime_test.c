// Exercise the actual watchface callbacks against a deterministic SDK host model.
#define main watchface_main
#include "../src/TextWatch.c"
#undef main
#include "../src/status_indicators.c"

#ifdef WORDISH_GENTLE_GESTURE
static void (*mock_gentle_handler)(void);
static bool mock_gentle_light;
static int mock_gentle_deinits;
void gentle_gesture_set_enabled(void (*handler)(void)) { mock_gentle_handler = handler; }
void gentle_gesture_set_light(bool on) { mock_gentle_light = on; }
void gentle_gesture_deinit(void) { mock_gentle_handler = NULL; mock_gentle_light = false; mock_gentle_deinits++; }
#endif

static void trigger_exact_gesture(void)
{
#ifdef WORDISH_GENTLE_GESTURE
  assert(mock_gentle_handler);
  mock_light_on = true;
  mock_backlight_handler(true);
  assert(mock_gentle_light);
  mock_gentle_handler();
#else
  mock_tap_handler(0, 1);
  mock_tap_handler(0, 1);
#endif
}

static void finish_animations(void)
{
  for (int i = 0; i < NUM_LINES; i++) {
    if (lines[i].animation1) {
      mock_animation_stop(property_animation_get_animation(lines[i].animation1), true);
    }
    if (lines[i].animation2) {
      mock_animation_stop(property_animation_get_animation(lines[i].animation2), true);
    }
  }
}

static void assert_animation_cancellation_is_owned_once(void)
{
  Line line = {0};
  screen_bounds = GRect(0, 0, 200, 228);
  init_line(&line);
  makeAnimationsForLayer(&line, 0);
  Animation *outgoing = property_animation_get_animation(line.animation1);
  Animation *incoming = property_animation_get_animation(line.animation2);
  destroy_property_animation(&line.animation1);
  assert(outgoing->destroyed);
  assert(!incoming->destroyed);
  destroy_property_animation(&line.animation2);
  assert(incoming->destroyed);
  assert(line.animation1 == NULL);
  assert(line.animation2 == NULL);
  destroy_line(&line);
}

static void assert_animation_completion_releases_only_itself(void)
{
  Line line = {0};
  init_line(&line);
  makeAnimationsForLayer(&line, 0);
  Animation *outgoing = property_animation_get_animation(line.animation1);
  Animation *incoming = property_animation_get_animation(line.animation2);
  mock_animation_stop(incoming, true);
  assert(line.animation2 == NULL);
  assert(!outgoing->destroyed);
  assert(outgoing->scheduled);
  mock_animation_stop(outgoing, true);
  assert(line.animation1 == NULL);
  destroy_line(&line);
}

static void assert_display_time_skips_unchanged_layers(void)
{
  struct tm now = { .tm_year = 126, .tm_mon = 9, .tm_mday = 19,
      .tm_wday = 1, .tm_hour = 3, .tm_min = 15 };
  screen_bounds = GRect(0, 0, 144, 168);
  date_position = DATE_POSITION_TOP;
  for (int i = 0; i < NUM_LINES; i++) {
    init_line(&lines[i]);
  }
  for (int i = 0; i < DATE_PART_COUNT; i++) {
    date_layers[i] = text_layer_create(date_layer_frame());
  }
  quiet_time_icon_layer = layer_create(GRect(0, 0, 18, 18));
  window_loaded = true;
  display_initial_time(&now);
  int updates = mock_layer_updates;
  int animations = mock_animation_creates;
  now.tm_min = 16;
  display_time(&now);
  assert(mock_layer_updates == updates);
  assert(mock_animation_creates == animations);

  // Quiet Time and midnight still refresh their visible indicators.
  mock_quiet_time = true;
  display_time(&now);
  assert(mock_layer_updates > updates);
  assert(!quiet_time_icon_layer->hidden);
  updates = mock_layer_updates;
  now.tm_mday++;
  display_time(&now);
  assert(mock_layer_updates > updates);
  assert(strcmp(date_text[0], "20-") == 0);

  // A font change must refresh every existing row, even when its words match.
  font_choice = FONT_CHOICE_SMALL;
  display_time(&now);
  assert(mock_animation_creates > animations);
  assert(strcmp(lines[0].currentLayer->font, FONT_KEY_GOTHIC_18) == 0);
  finish_animations();

  // Repeated mode changes interrupt in-flight animations without double frees.
  for (int i = 0; i < 4; i++) {
    exact_time_visible = !exact_time_visible;
    forceDisplayUpdate = true;
    display_time(&now);
  }
  finish_animations();
  window_unload(NULL);
  assert(mock_animation_creates == mock_animation_destroys);
}

static void assert_legacy_invert_migration(void)
{
  for (int has_custom_colours = 0; has_custom_colours < 2; has_custom_colours++) {
    memset(mock_persist_exists, 0, sizeof(mock_persist_exists));
    memset(mock_persist_values, 0, sizeof(mock_persist_values));
    foreground_color = 0xFFFFFF;
    background_color = 0x000000;
    date_position = DATE_POSITION_TOP;
    persist_write_int(INVERT_KEY, 1);
    if (has_custom_colours) {
      persist_write_int(FOREGROUND_COLOR_KEY, 0x123456);
      persist_write_int(BACKGROUND_COLOR_KEY, 0xABCDEF);
    }
    handle_init();
    assert(date_text[0][0] != '\0');
    assert(foreground_color == (has_custom_colours ? 0x123456 : 0x000000));
    assert(background_color == (has_custom_colours ? 0xABCDEF : 0xFFFFFF));
    assert(persist_read_int(FOREGROUND_COLOR_KEY) == foreground_color);
    assert(persist_read_int(BACKGROUND_COLOR_KEY) == background_color);
    assert(persist_read_int(SETTINGS_SCHEMA_KEY) == SETTINGS_SCHEMA_VERSION);
    handle_deinit();
  }
}

static void receive_setting(uint32_t key, int value)
{
  Tuple tuple = { .key = key };
  tuple.value = calloc(1, sizeof(*tuple.value));
  tuple.value->int32 = value;
  apply_settings_tuple(key, &tuple);
  free(tuple.value);
  if (window_loaded) {
    display_time(t);
  }
}

static void assert_optional_controls_and_services(void)
{
  memset(mock_persist_exists, 0, sizeof(mock_persist_exists));
  mock_quiet_time = false;
  handle_init();
  assert(animations_enabled && gesture_enabled && exact_time_seconds == 3);
  assert(!low_battery_enabled && !disconnect_enabled);
#ifdef WORDISH_GENTLE_GESTURE
  assert(mock_tap_subscriptions == 0 && !mock_tap_handler);
  assert(mock_backlight_subscriptions == 1 && mock_gentle_handler);
  assert(mock_gentle_light == mock_light_on);
#else
  assert(mock_tap_subscriptions == 1);
#endif
  assert(mock_battery_subscriptions == 0 && mock_connection_subscriptions == 0);
  assert(!status_indicators_visible());

  receive_setting(EXACT_TIME_SECONDS_KEY, 10);
  trigger_exact_gesture();
  assert(exact_time_visible);
  assert(exact_time_timer->delay_ms == 10000);
  receive_setting(GESTURE_ENABLED_KEY, 0);
  assert(!gesture_subscribed && !mock_tap_handler);
  assert(!exact_time_visible && !tap_pending);
  assert(!tap_reset_timer && !exact_time_timer && mock_timer_count == 0);
#ifdef WORDISH_GENTLE_GESTURE
  assert(!mock_gentle_handler);
  mock_backlight_handler(false);
  assert(!mock_gentle_light);
  receive_setting(GESTURE_ENABLED_KEY, 1);
  assert(mock_gentle_handler && !mock_gentle_light);
  assert(mock_backlight_subscriptions == 1 && mock_tap_subscriptions == 0);
#else
  tap_handler(0, 1);
  assert(!tap_pending);
  receive_setting(GESTURE_ENABLED_KEY, 1);
  mock_tap_handler(0, 1);
  assert(tap_pending);
#endif
  receive_setting(GESTURE_ENABLED_KEY, 0);
  assert(!tap_pending && mock_timer_count == 0);

  // Disabling animation safely interrupts an in-flight transition.
  forceDisplayUpdate = true;
  display_time(t);
  int created = mock_animation_creates;
  receive_setting(ANIMATIONS_ENABLED_KEY, 0);
  assert(mock_animation_creates == created);
  for (int i = 0; i < NUM_LINES; i++) {
    assert(!lines[i].animation1 && !lines[i].animation2);
    assert(lines[i].currentLayer->layer.frame.origin.x < screen_bounds.size.w);
    assert(lines[i].nextLayer->layer.frame.origin.x == screen_bounds.size.w);
  }
  assert(mock_animation_destroys == mock_animation_creates);

  receive_setting(LOW_BATTERY_ENABLED_KEY, 1);
  receive_setting(DISCONNECT_ENABLED_KEY, 1);
  assert(mock_battery_subscriptions == 1 && mock_connection_subscriptions == 1);
  assert(!status_indicators_visible());
  mock_battery = (BatteryChargeState){ 20, false, false };
  mock_battery_handler(mock_battery);
  assert(status_indicators_visible() && battery_warning);
  int updates = mock_layer_updates;
  mock_battery = (BatteryChargeState){ 19, false, false };
  mock_battery_handler(mock_battery);
  assert(mock_layer_updates == updates);
  mock_battery = (BatteryChargeState){ 19, true, true };
  mock_battery_handler(mock_battery);
  assert(!status_indicators_visible());
  mock_connected = false;
  mock_connection_handler(false);
  assert(status_indicators_visible() && disconnect_warning);
  receive_setting(DISCONNECT_ENABLED_KEY, 0);
  assert(!status_indicators_visible() && mock_connection_subscriptions == 0);
  receive_setting(LOW_BATTERY_ENABLED_KEY, 0);
  assert(mock_battery_subscriptions == 0);

  // Persisted controls survive relaunch; malformed values revert to safe defaults.
  handle_deinit();
  handle_init();
  assert(!animations_enabled && !gesture_enabled && exact_time_seconds == 10);
  assert(mock_tap_subscriptions == 0);
  receive_setting(EXACT_TIME_SECONDS_KEY, 4);
  assert(exact_time_seconds == 3);
  receive_setting(EXACT_TIME_SECONDS_KEY, 5);
  assert(exact_time_seconds == 5);
  receive_setting(ANIMATIONS_ENABLED_KEY, -1);
  receive_setting(GESTURE_ENABLED_KEY, 2);
  receive_setting(LOW_BATTERY_ENABLED_KEY, -1);
  receive_setting(DISCONNECT_ENABLED_KEY, 2);
  assert(animations_enabled && gesture_enabled);
  assert(!low_battery_enabled && !disconnect_enabled);
  handle_deinit();
  assert(mock_tap_subscriptions == 0 && mock_battery_subscriptions == 0
      && mock_connection_subscriptions == 0);
#ifdef WORDISH_GENTLE_GESTURE
  assert(mock_backlight_subscriptions == 0 && !mock_backlight_handler);
  assert(!mock_gentle_handler && !mock_gentle_light && mock_gentle_deinits > 0);
#endif
}

static void assert_frame_inside_screen(GRect frame)
{
  assert(frame.origin.x >= 0 && frame.origin.y >= 0);
  assert(frame.origin.x + frame.size.w <= screen_bounds.size.w);
  assert(frame.origin.y + frame.size.h <= screen_bounds.size.h);
  if (mock_round) {
    int radius = screen_bounds.size.w / 2;
    for (int side = 0; side < 2; side++) {
      int dx = frame.origin.x + (side ? frame.size.w : 0) - radius;
      for (int edge = 0; edge < 2; edge++) {
        int dy = frame.origin.y + (edge ? frame.size.h : 0) - radius;
        assert(dx * dx + dy * dy <= radius * radius);
      }
    }
  }
}

static void assert_decoration_frames(void)
{
  GRect content = time_content_frame();
  for (int i = 0; i < DATE_PART_COUNT; i++) {
    Layer *layer = text_layer_get_layer(date_layers[i]);
    if (!layer->hidden) {
      assert_frame_inside_screen(layer->frame);
      if (date_position == DATE_POSITION_TOP) {
        assert(layer->frame.origin.y + layer->frame.size.h <= content.origin.y);
      } else {
        assert(layer->frame.origin.y >= content.origin.y + content.size.h);
        if (status_indicators_visible()) {
          assert(layer->frame.origin.y + layer->frame.size.h <= status_layer->frame.origin.y);
        }
      }
    }
  }
  if (!quiet_time_icon_layer->hidden) {
    assert_frame_inside_screen(quiet_time_icon_layer->frame);
    assert(quiet_time_icon_layer->frame.origin.y + quiet_time_icon_layer->frame.size.h <= content.origin.y);
    if (date_position == DATE_POSITION_TOP) {
      assert(quiet_time_icon_layer->frame.origin.x + quiet_time_icon_layer->frame.size.w
          <= date_layers[0]->layer.frame.origin.x);
    }
  }
  if (status_indicators_visible()) {
    assert_frame_inside_screen(status_layer->frame);
    assert(status_layer->frame.origin.y >= content.origin.y + content.size.h);
  }
}

static void assert_decorated_layouts_fit(void)
{
  const struct { int width, height; bool round; } screens[] = {
    { 144, 168, false }, // Aplite
    { 144, 168, false }, // Basalt
    { 180, 180, true },  // Chalk
    { 144, 168, false }, // Diorite
    { 200, 228, false }, // Emery
    { 144, 168, false }, // Flint
    { 260, 260, true }   // Gabbro
  };
  struct tm now = { .tm_year = 126, .tm_mon = 9, .tm_mday = 19, .tm_wday = 1 };
  memset(mock_persist_exists, 0, sizeof(mock_persist_exists));
  persist_write_int(ANIMATIONS_ENABLED_KEY, 0);
  handle_init();
  t = &now;
  for (size_t screen = 0; screen < sizeof(screens) / sizeof(screens[0]); screen++) {
    screen_bounds = GRect(0, 0, screens[screen].width, screens[screen].height);
    status_screen = screen_bounds;
    mock_round = screens[screen].round;
    for (int date = DATE_POSITION_OFF; date <= DATE_POSITION_BOTTOM; date++) {
      date_position = date;
      for (int quiet = 0; quiet < 2; quiet++) {
        mock_quiet_time = quiet;
        for (int status = 0; status < 2; status++) {
          battery_warning = status;
          disconnect_warning = status;
          update_status_layout(!status);
          for (int format = DATE_FORMAT_DD_MM_YY; format <= DATE_FORMAT_MM_SLASH_DD; format++) {
            date_format = format;
            for (int alignment = TEXT_ALIGN_CENTER; alignment <= TEXT_ALIGN_RIGHT; alignment++) {
              text_align = alignment;
              forceDisplayUpdate = true;
              display_time(&now);
              assert_decoration_frames();
            }
          }
          for (int language = CA; language <= SV; language++) {
            lang = (Language)language;
            for (int hour = 0; hour < 24; hour++) {
              now.tm_hour = hour;
              for (int minute = 0; minute < 60; minute += 5) {
                now.tm_min = minute;
                forceDisplayUpdate = true;
                display_time(&now);
                GRect content = time_content_frame();
                for (int i = 0; i < currentNLines; i++) {
                  GRect frame = lines[i].currentLayer->layer.frame;
                  if (frame.origin.y < content.origin.y
                      || frame.origin.y + frame.size.h > content.origin.y + content.size.h
                      || text_estimated_width(render_font_choice, lines[i].currentLayer->text) > frame.size.w) {
                    fprintf(stderr, "decorated layout screen=%zu date=%d quiet=%d status=%d lang=%d %02d:%02d line=%d text=%s\n",
                        screen, date, quiet, status, language, hour, minute, i, lines[i].currentLayer->text);
                  }
                  assert_frame_inside_screen(frame);
                  assert(frame.origin.y >= content.origin.y);
                  assert(frame.origin.y + frame.size.h <= content.origin.y + content.size.h);
                  assert(text_estimated_width(render_font_choice, lines[i].currentLayer->text) <= frame.size.w);
                }
              }
            }
          }
        }
      }
    }
  }
  handle_deinit();
  mock_round = false;
}

static void assert_tick_time_is_owned(void)
{
  memset(mock_persist_exists, 0, sizeof(mock_persist_exists));
  persist_write_int(ANIMATIONS_ENABLED_KEY, 0);
  handle_init();
  mock_24h = true;
  struct tm borrowed = { .tm_year = 126, .tm_mon = 9, .tm_mday = 19,
      .tm_wday = 1, .tm_hour = 13, .tm_min = 42 };
  handle_minute_tick(&borrowed, MINUTE_UNIT);
  borrowed.tm_hour = 20;
  borrowed.tm_min = 59;
  assert(t != &borrowed && t->tm_hour == 13 && t->tm_min == 42);
  trigger_exact_gesture();
  assert(exact_time_visible);
  assert(strcmp(lines[0].currentLayer->text, "13:42") == 0);
  receive_setting(LOW_BATTERY_ENABLED_KEY, 1);
  mock_battery = (BatteryChargeState){ 20, false, false };
  mock_battery_handler(mock_battery);
  assert(strcmp(lines[0].currentLayer->text, "13:42") == 0);
  handle_deinit();
  assert(mock_timer_count == 0);
}

static void assert_real_font_measurement_overrides_estimates(void)
{
  char text[NUM_LINES][BUFFER_SIZE];
  char format[NUM_LINES];
  screen_bounds = GRect(0, 0, 200, 228);
  mock_round = false;
  date_position = DATE_POSITION_TOP;
  quiet_time_active = false;
  font_choice = FONT_CHOICE_CLASSIC;
  Layer parent = {0};
  status_indicators_init(&parent, screen_bounds, NULL);
  battery_warning = true;
  time_to_lines(CA, 0, 35, 0, text, format);
  assert(strcmp(text[0], "dos quarts") == 0);
  FontChoice fitted = choose_render_font(text, format);
  assert(fitted != FONT_CHOICE_CLASSIC && fitted != FONT_CHOICE_TALL);
  choose_time_lines(CA, 0, 35, 0, text, format);
  GRect content = time_content_frame();
  for (int i = 0; i < count_text_lines(text); i++) {
    TextFrame frame = text_frame_for_line_in_area(200, 228, false, render_font_choice,
        count_text_lines(text), i, content.origin.y, 228 - content.origin.y - content.size.h);
    assert(measured_line_width(render_font_choice, text[i], i, format) <= frame.w);
  }

  // A wide bold hour must fit the final circular row on Gabbro as well.
  battery_warning = false;
  screen_bounds = GRect(0, 0, 260, 260);
  mock_round = true;
  time_to_lines_with_limit(EN_GB, 11, 45, 0, 7, text, format);
  assert(strcmp(text[2], "twelve") == 0);
  assert(choose_render_font(text, format) != FONT_CHOICE_LARGE);
  time_to_lines(EN_GB, 11, 25, 0, text, format);
  assert(strcmp(text[2], "eleven") == 0);
  assert(choose_render_font(text, format) != FONT_CHOICE_LARGE);
  const int minutes[] = { 45, 25 };
  for (unsigned int c = 0; c < ARRAY_LENGTH(minutes); c++) {
    choose_time_lines(EN_GB, 11, minutes[c], 0, text, format);
    content = time_content_frame();
    for (int i = 0; i < count_text_lines(text); i++) {
      TextFrame frame = text_frame_for_line_in_area(260, 260, true, render_font_choice,
          count_text_lines(text), i, content.origin.y, 260 - content.origin.y - content.size.h);
      assert(measured_line_width(render_font_choice, text[i], i, format) <= frame.w);
    }
  }
  mock_round = false;

  char light[] = { ' ' };
  char bold[] = { 'b' };
  measured_line_width(FONT_CHOICE_CLASSIC, "cache-test", 0, light);
  int measurements = mock_text_measurements;
  measured_line_width(FONT_CHOICE_CLASSIC, "cache-test", 0, light);
  assert(mock_text_measurements == measurements);
  measured_line_width(FONT_CHOICE_CLASSIC, "cache-test", 0, bold);
  assert(mock_text_measurements == measurements + 1);
  status_indicators_deinit();
}

int main(void)
{
  assert_animation_cancellation_is_owned_once();
  assert_animation_completion_releases_only_itself();
  assert_display_time_skips_unchanged_layers();
  assert_legacy_invert_migration();
  assert_optional_controls_and_services();
  assert_decorated_layouts_fit();
  assert_tick_time_is_owned();
  assert_real_font_measurement_overrides_estimates();
  puts("watchface runtime model checks passed");
  return 0;
}
