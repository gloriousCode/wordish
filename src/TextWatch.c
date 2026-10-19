#include <pebble.h>

#include "num2words.h"
#include "text_layout.h"
#include "text_lines.h"
#include "status_indicators.h"
#include "smooth_fonts.h"

#define DEBUG 0

#define NUM_LINES FUZZY_TEXT_NUM_LINES
#define LINE_LENGTH FUZZY_TEXT_LINE_LENGTH
#define BUFFER_SIZE FUZZY_TEXT_BUFFER_SIZE

#define INVERT_KEY 0
#define TEXT_ALIGN_KEY 1
#define LANGUAGE_KEY 2
#define FONT_CHOICE_KEY 3
#define FOREGROUND_COLOR_KEY 4
#define BACKGROUND_COLOR_KEY 5
#define DATE_POSITION_KEY 6
#define DATE_FORMAT_KEY 7
#define ROW_ONE_COLOR_KEY 8
#define ROW_TWO_COLOR_KEY 9
#define ROW_THREE_COLOR_KEY 10
#define DATE_PART_ONE_COLOR_KEY 11
#define DATE_PART_TWO_COLOR_KEY 12
#define DATE_PART_THREE_COLOR_KEY 13
#define ANIMATIONS_ENABLED_KEY 14
#define GESTURE_ENABLED_KEY 15
#define EXACT_TIME_SECONDS_KEY 16
#define LOW_BATTERY_ENABLED_KEY 17
#define DISCONNECT_ENABLED_KEY 18
#define TEST_EXACT_TIME_KEY 95
#define TEST_QUIET_TIME_KEY 96
#define SETTINGS_SCHEMA_KEY 97
#define TEST_HOUR_KEY 98
#define TEST_MINUTE_KEY 99

#define SETTINGS_SCHEMA_VERSION 5

#define TEXT_ALIGN_CENTER 0
#define TEXT_ALIGN_LEFT 1
#define TEXT_ALIGN_RIGHT 2

#define DATE_POSITION_OFF 0
#define DATE_POSITION_TOP 1
#define DATE_POSITION_BOTTOM 2

#define DATE_FORMAT_DD_MM_YY 0
#define DATE_FORMAT_MM_DD_YYYY 1
#define DATE_FORMAT_MON_D_AUG 2
#define DATE_FORMAT_DD_SLASH_MM 3
#define DATE_FORMAT_MM_SLASH_DD 4

#define TEXT_COLOUR_MATCH_FOREGROUND -1
#define CUSTOM_ROW_COUNT 3
#define DATE_PART_COUNT 3
#define DATE_LAYER_HEIGHT 22
#define DATE_LAYER_VERTICAL_MARGIN 2
#define QUIET_TIME_ICON_SIZE 18
#define QUIET_TIME_ICON_SPACING 3
#define DOUBLE_TAP_WINDOW_MS 700
#define STATUS_RESERVED_HEIGHT 24

// The time it takes for a layer to slide in or out.
#define ANIMATION_DURATION 400
// Delay between the layers animations, from top to bottom
#define ANIMATION_STAGGER_TIME 150
// Delay from the start of the current layer going out until the next layer slides in
#define ANIMATION_OUT_IN_DELAY 100

static int text_align = TEXT_ALIGN_CENTER;
static int foreground_color = 0xFFFFFF;
static int background_color = 0x000000;
static int row_colors[CUSTOM_ROW_COUNT] = {
	TEXT_COLOUR_MATCH_FOREGROUND,
	TEXT_COLOUR_MATCH_FOREGROUND,
	TEXT_COLOUR_MATCH_FOREGROUND,
};
static int date_part_colors[DATE_PART_COUNT] = {
	TEXT_COLOUR_MATCH_FOREGROUND,
	TEXT_COLOUR_MATCH_FOREGROUND,
	TEXT_COLOUR_MATCH_FOREGROUND,
};
static int date_position = DATE_POSITION_OFF;
static int date_format = DATE_FORMAT_DD_MM_YY;
static bool animations_enabled = true;
static bool gesture_enabled = true;
static bool gesture_subscribed;
static int exact_time_seconds = 3;
static bool low_battery_enabled;
static bool disconnect_enabled;
static FontChoice font_choice = FONT_CHOICE_CLASSIC;
static FontChoice render_font_choice = FONT_CHOICE_CLASSIC;
static Language lang = EN_GB;
static GFont custom_font_large_light;
static GFont custom_font_large_bold;
static GFont custom_font_medium_light;
static GFont custom_font_medium_bold;

static Window *window;
static GRect screen_bounds;
static bool window_loaded = false;
static TextLayer *date_layers[DATE_PART_COUNT];
static char date_text[DATE_PART_COUNT][8];
static Layer *quiet_time_icon_layer;
static bool quiet_time_active = false;

typedef struct {
	TextLayer *currentLayer;
	TextLayer *nextLayer;
	char lineStr1[BUFFER_SIZE];
	char lineStr2[BUFFER_SIZE];
	char currentFormat;
	char nextFormat;
	int currentRow;
	int nextRow;
	PropertyAnimation *animation1;
	PropertyAnimation *animation2;
} Line;

static Line lines[NUM_LINES];

static struct tm *t;
static struct tm current_time;
static struct tm test_time;
static bool use_test_time = false;
static bool pending_test_hour = false;
static bool pending_test_minute = false;
static bool test_exact_time_active = false;
static bool test_quiet_time_active = false;
static int requested_test_hour = 0;
static int requested_test_minute = 0;

static int currentNLines;
static bool forceDisplayUpdate = false;

static bool showTime = true;
static int dateTimeout = 0;
static bool tap_pending = false;
static bool exact_time_visible = false;
static AppTimer *tap_reset_timer;
static AppTimer *exact_time_timer;

static void display_time(struct tm *date_time);

static bool valid_toggle(int value, bool default_value)
{
	return value == 0 ? false : value == 1 ? true : default_value;
}

static int valid_exact_time_seconds(int value)
{
	return value == 3 || value == 5 || value == 10 ? value : 3;
}

static void status_visibility_changed(void)
{
	forceDisplayUpdate = true;
	if (t != NULL && window_loaded) {
		display_time(t);
	}
}

static void destroy_property_animation(PropertyAnimation **animation)
{
	if (*animation == NULL) {
		return;
	}

	PropertyAnimation *old_animation = *animation;
	*animation = NULL;
	animation_unschedule(property_animation_get_animation(old_animation));
	property_animation_destroy(old_animation);
}

static void destroy_finished_property_animation(PropertyAnimation **animation)
{
	if (*animation == NULL) {
		return;
	}

	property_animation_destroy(*animation);
	*animation = NULL;
}

static void animationStoppedHandler(struct Animation *animation, bool finished, void *context)
{
	Line *line = (Line *)context;
	if (line == NULL) {
		return;
	}

	if (line->animation1 != NULL
			&& property_animation_get_animation(line->animation1) == animation) {
		destroy_finished_property_animation(&line->animation1);
	} else if (line->animation2 != NULL
			&& property_animation_get_animation(line->animation2) == animation) {
		if (finished && line->nextLayer != NULL) {
			GRect rect = layer_get_frame((Layer *)line->nextLayer);
			rect.origin.x = screen_bounds.size.w;
			layer_set_frame((Layer *)line->nextLayer, rect);
		}
		destroy_finished_property_animation(&line->animation2);
	}
}

// Animate line
static void makeAnimationsForLayer(Line *line, int delay)
{
	TextLayer *current = line->currentLayer;
	TextLayer *next = line->nextLayer;
	if (current == NULL || next == NULL) {
		return;
	}

	// Destroy old animations
	destroy_property_animation(&line->animation1);
	destroy_property_animation(&line->animation2);

	// Configure animation for current layer to move out
	GRect rect = layer_get_frame((Layer *)current);
	rect.origin.x =  -screen_bounds.size.w;
	line->animation1 = property_animation_create_layer_frame((Layer *)current, NULL, &rect);
	if (line->animation1 == NULL) {
		return;
	}
	Animation *animation1 = property_animation_get_animation(line->animation1);
	animation_set_duration(animation1, ANIMATION_DURATION);
	animation_set_delay(animation1, delay);
	animation_set_curve(animation1, AnimationCurveEaseIn); // Accelerate
	animation_set_handlers(animation1, (AnimationHandlers) {
		.stopped = animationStoppedHandler
	}, line);

	// Configure animation for current layer to move in
	GRect rect2 = layer_get_frame((Layer *)next);
	rect2.origin.x = (screen_bounds.size.w - rect2.size.w) / 2;
	line->animation2 = property_animation_create_layer_frame((Layer *)next, NULL, &rect2);
	if (line->animation2 == NULL) {
		destroy_property_animation(&line->animation1);
		return;
	}
	Animation *animation2 = property_animation_get_animation(line->animation2);
	animation_set_duration(animation2, ANIMATION_DURATION);
	animation_set_delay(animation2, delay + ANIMATION_OUT_IN_DELAY);
	animation_set_curve(animation2, AnimationCurveEaseOut); // Deaccelerate

	// Set a handler to rearrange layers after animation is finished
	animation_set_handlers(animation2, (AnimationHandlers) {
		.stopped = animationStoppedHandler
	}, line);

	// Start the animations
	animation_schedule(animation1);
	animation_schedule(animation2);
}

static void updateLayerText(TextLayer* layer, char* text)
{
	if (layer == NULL) {
		return;
	}
	const char* layerText = text_layer_get_text(layer);
	if (layerText == NULL) {
		return;
	}
	strncpy((char*)layerText, text, BUFFER_SIZE);
	((char*)layerText)[BUFFER_SIZE - 1] = '\0';
	// To mark layer dirty
	text_layer_set_text(layer, layerText);
    //layer_mark_dirty(&layer->layer);
}

// Update line
static void updateLineTo(Line *line, char *value, int delay)
{
	updateLayerText(line->nextLayer, value);
	if (animations_enabled) {
		makeAnimationsForLayer(line, delay);
	} else {
		destroy_property_animation(&line->animation1);
		destroy_property_animation(&line->animation2);
		if (line->currentLayer != NULL) {
			GRect rect = layer_get_frame((Layer *)line->currentLayer);
			rect.origin.x = screen_bounds.size.w;
			layer_set_frame((Layer *)line->currentLayer, rect);
		}
		if (line->nextLayer != NULL) {
			GRect rect = layer_get_frame((Layer *)line->nextLayer);
			rect.origin.x = (screen_bounds.size.w - rect.size.w) / 2;
			layer_set_frame((Layer *)line->nextLayer, rect);
		}
	}

	// Swap current/next layers
	TextLayer *tmp = line->nextLayer;
	line->nextLayer = line->currentLayer;
	line->currentLayer = tmp;
	char tmp_format = line->nextFormat;
	line->nextFormat = line->currentFormat;
	line->currentFormat = tmp_format;
	int tmp_row = line->nextRow;
	line->nextRow = line->currentRow;
	line->currentRow = tmp_row;
}

// Check to see if the current line needs to be updated
static bool needToUpdateLine(Line *line, char *nextValue)
{
	if (line->currentLayer == NULL) {
		return true;
	}
	const char *currentStr = text_layer_get_text(line->currentLayer);
	if (currentStr == NULL) {
		return true;
	}

	if (strcmp(currentStr, nextValue) != 0) {
		return true;
	}
	return false;
}

static int count_text_lines(char text[NUM_LINES][BUFFER_SIZE])
{
	int count = 0;

	for (int i = 0; i < NUM_LINES; i++) {
		if (strlen(text[i]) == 0) {
			break;
		}
		count++;
	}

	return count;
}

static FontChoice choose_render_font(char text[NUM_LINES][BUFFER_SIZE], char format[]);

static void clear_text_lines(char text[NUM_LINES][BUFFER_SIZE], char format[])
{
	for (int i = 0; i < NUM_LINES; i++) {
		text[i][0] = '\0';
		format[i] = ' ';
	}
}

static void exact_time_to_lines(const struct tm *date_time,
		char text[NUM_LINES][BUFFER_SIZE], char format[])
{
	clear_text_lines(text, format);

	int hour = date_time->tm_hour;
	if (!clock_is_24h_style()) {
		hour %= 12;
		if (hour == 0) {
			hour = 12;
		}
	}

	snprintf(text[0], BUFFER_SIZE, "%d:%02d", hour, date_time->tm_min);
	format[0] = 'b';
	render_font_choice = choose_render_font(text, format);
}

static GTextAlignment lookup_text_alignment(int align_key)
{
	GTextAlignment alignment;
	switch (align_key)
	{
		case TEXT_ALIGN_LEFT:
			alignment = GTextAlignmentLeft;
			break;
		case TEXT_ALIGN_RIGHT:
			alignment = GTextAlignmentRight;
			break;
		default:
			alignment = GTextAlignmentCenter;
			break;
	}
	return alignment;
}

static int valid_text_align(int align_key)
{
	switch (align_key) {
		case TEXT_ALIGN_LEFT:
		case TEXT_ALIGN_RIGHT:
		case TEXT_ALIGN_CENTER:
			return align_key;
		default:
			return TEXT_ALIGN_CENTER;
	}
}

static int valid_date_position(int position)
{
	switch (position) {
		case DATE_POSITION_OFF:
		case DATE_POSITION_TOP:
		case DATE_POSITION_BOTTOM:
			return position;
		default:
			return DATE_POSITION_OFF;
	}
}

static int valid_date_format(int format)
{
	switch (format) {
		case DATE_FORMAT_DD_MM_YY:
		case DATE_FORMAT_MM_DD_YYYY:
		case DATE_FORMAT_MON_D_AUG:
		case DATE_FORMAT_DD_SLASH_MM:
		case DATE_FORMAT_MM_SLASH_DD:
			return format;
		default:
			return DATE_FORMAT_DD_MM_YY;
	}
}

static Language valid_language(Language language)
{
	switch (language) {
		case CA:
		case DE:
		case EN_GB:
		case EN_US:
		case ES:
		case FR:
		case NO:
		case SV:
			return language;
		default:
			return EN_GB;
	}
}

static void format_corner_date(char buffer[], size_t buffer_size, const struct tm *date_time)
{
	static const char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	if (buffer_size == 0) {
		return;
	}

	int day = date_time->tm_mday;
	if (day < 1) {
		day = 1;
	} else if (day > 31) {
		day = 31;
	}
	int month = date_time->tm_mon;
	if (month < 0) {
		month = 0;
	} else if (month > 11) {
		month = 11;
	}
	int weekday = date_time->tm_wday;
	if (weekday < 0) {
		weekday = 0;
	} else if (weekday > 6) {
		weekday = 6;
	}
	int year = date_time->tm_year + 1900;
	if (year < 0) {
		year = 0;
	} else if (year > 9999) {
		year = 9999;
	}

	switch (date_format) {
		case DATE_FORMAT_MM_DD_YYYY:
			snprintf(buffer, buffer_size, "%02d-%02d-%04d", month + 1, day, year);
			break;
		case DATE_FORMAT_MON_D_AUG:
			snprintf(buffer, buffer_size, "%s %d %s", days[weekday], day, months[month]);
			break;
		case DATE_FORMAT_DD_SLASH_MM:
			snprintf(buffer, buffer_size, "%02d/%02d", day, month + 1);
			break;
		case DATE_FORMAT_MM_SLASH_DD:
			snprintf(buffer, buffer_size, "%02d/%02d", month + 1, day);
			break;
		default:
			snprintf(buffer, buffer_size, "%02d-%02d-%02d", day, month + 1, year % 100);
			break;
	}
	buffer[buffer_size - 1] = '\0';
}

static GColor colour_from_rgb(int rgb)
{
#ifdef PBL_COLOR
	return GColorFromRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
#else
	int red = (rgb >> 16) & 0xFF;
	int green = (rgb >> 8) & 0xFF;
	int blue = rgb & 0xFF;
	return (red * 30 + green * 59 + blue * 11) >= 12800 ? GColorWhite : GColorBlack;
#endif
}

static GColor background_colour(void)
{
	return colour_from_rgb(background_color);
}

static GColor foreground_colour(void)
{
#ifdef PBL_COLOR
	return colour_from_rgb(foreground_color);
#else
	GColor foreground = colour_from_rgb(foreground_color);
	GColor background = background_colour();
	if (gcolor_equal(foreground, background)) {
		return gcolor_equal(background, GColorBlack) ? GColorWhite : GColorBlack;
	}
	return foreground;
#endif
}

static GColor configured_text_colour(int configured_colour)
{
	if (configured_colour == TEXT_COLOUR_MATCH_FOREGROUND) {
		return foreground_colour();
	}

	GColor colour = colour_from_rgb(configured_colour);
#ifndef PBL_COLOR
	GColor background = background_colour();
	if (gcolor_equal(colour, background)) {
		return gcolor_equal(background, GColorBlack) ? GColorWhite : GColorBlack;
	}
#endif
	return colour;
}

static GColor row_colour_for_index(int row)
{
	if (row < 0) {
		row = 0;
	} else if (row >= CUSTOM_ROW_COUNT) {
		row = CUSTOM_ROW_COUNT - 1;
	}

	return configured_text_colour(row_colors[row]);
}

static GColor date_part_colour_for_index(int part)
{
	if (part < 0) {
		part = 0;
	} else if (part >= DATE_PART_COUNT) {
		part = DATE_PART_COUNT - 1;
	}

	return configured_text_colour(date_part_colors[part]);
}

static GFont font_for_choice(FontChoice choice, bool bold)
{
#ifdef WORDISH_SMOOTH_FONTS
	GFont smooth = smooth_font_for_choice(choice, bold);
	if (smooth != NULL) {
		return smooth;
	}
#endif
	switch (choice)
	{
		case FONT_CHOICE_LARGE:
#ifdef WORDISH_SMOOTH_FONTS
			return fonts_get_system_font(bold ? FONT_KEY_BITHAM_42_BOLD : FONT_KEY_BITHAM_42_LIGHT);
#else
			if (bold) {
				if (custom_font_large_bold == NULL) {
					custom_font_large_bold = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_54));
				}
				return custom_font_large_bold ? custom_font_large_bold : fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
			}
			if (custom_font_large_light == NULL) {
				custom_font_large_light = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_54));
			}
			return custom_font_large_light ? custom_font_large_light : fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT);
#endif
		case FONT_CHOICE_MEDIUM:
#ifdef WORDISH_SMOOTH_FONTS
			return fonts_get_system_font(bold ? FONT_KEY_BITHAM_42_BOLD : FONT_KEY_BITHAM_42_LIGHT);
#else
			if (bold) {
				if (custom_font_medium_bold == NULL) {
					custom_font_medium_bold = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_48));
				}
				return custom_font_medium_bold ? custom_font_medium_bold : fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
			}
			if (custom_font_medium_light == NULL) {
				custom_font_medium_light = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_48));
			}
			return custom_font_medium_light ? custom_font_medium_light : fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT);
#endif
		case FONT_CHOICE_SHARP:
			return fonts_get_system_font(bold ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_28);
		case FONT_CHOICE_COMPACT:
			return fonts_get_system_font(bold ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_24);
		case FONT_CHOICE_TALL:
			return fonts_get_system_font(bold ? FONT_KEY_BITHAM_42_BOLD : FONT_KEY_BITHAM_42_LIGHT);
		case FONT_CHOICE_SMALL:
			return fonts_get_system_font(bold ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_18);
		default:
			return fonts_get_system_font(bold ? FONT_KEY_BITHAM_42_BOLD : FONT_KEY_BITHAM_42_LIGHT);
	}
}

static void unload_custom_font(GFont *font)
{
	if (*font != NULL) {
		fonts_unload_custom_font(*font);
		*font = NULL;
	}
}

static void unload_custom_fonts(void)
{
#ifdef WORDISH_SMOOTH_FONTS
	smooth_fonts_unload();
#endif
	unload_custom_font(&custom_font_large_light);
	unload_custom_font(&custom_font_large_bold);
	unload_custom_font(&custom_font_medium_light);
	unload_custom_font(&custom_font_medium_bold);
}

// Configure bold line of text
static void configureBoldLayer(TextLayer *textlayer)
{
	if (textlayer == NULL) {
		return;
	}
	text_layer_set_font(textlayer, font_for_choice(render_font_choice, true));
	text_layer_set_text_color(textlayer, row_colour_for_index(2));
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, lookup_text_alignment(text_align));
	text_layer_set_overflow_mode(textlayer, GTextOverflowModeTrailingEllipsis);
}

// Configure light line of text
static void configureLightLayer(TextLayer *textlayer, int row)
{
	if (textlayer == NULL) {
		return;
	}
	text_layer_set_font(textlayer, font_for_choice(render_font_choice, false));
	text_layer_set_text_color(textlayer, row_colour_for_index(row));
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, lookup_text_alignment(text_align));
	text_layer_set_overflow_mode(textlayer, GTextOverflowModeTrailingEllipsis);
}

static void configureLayerForFormat(TextLayer *textlayer, char format, int row)
{
	if (format == 'b') {
		configureBoldLayer(textlayer);
	} else {
		configureLightLayer(textlayer, row);
	}
}

static GRect date_layer_frame_for_position(int position)
{
	int horizontal_margin = 4;
	int y = PBL_IF_ROUND_ELSE(14, DATE_LAYER_VERTICAL_MARGIN);

	if (position == DATE_POSITION_BOTTOM) {
		y = screen_bounds.size.h - DATE_LAYER_HEIGHT
			- (status_indicators_visible() ? DATE_LAYER_VERTICAL_MARGIN
				: PBL_IF_ROUND_ELSE(18, DATE_LAYER_VERTICAL_MARGIN));
		if (status_indicators_visible()) {
			y -= STATUS_RESERVED_HEIGHT;
		}
	}
	if (PBL_IF_ROUND_ELSE(true, false)) {
		int radius = screen_bounds.size.w / 2;
		int top_distance = abs(y - screen_bounds.size.h / 2);
		int bottom_distance = abs(y + DATE_LAYER_HEIGHT - screen_bounds.size.h / 2);
		int distance = top_distance > bottom_distance ? top_distance : bottom_distance;
		int half_width = radius;
		while (half_width > 0 && half_width * half_width + distance * distance > radius * radius) {
			half_width--;
		}
		horizontal_margin = radius - half_width + 2;
	}

	return GRect(horizontal_margin, y, screen_bounds.size.w - (horizontal_margin * 2), DATE_LAYER_HEIGHT);
}

static GRect time_content_frame(void)
{
	int top = 0;
	int bottom = screen_bounds.size.h;
	if (date_position == DATE_POSITION_TOP || quiet_time_active) {
		GRect frame = date_layer_frame_for_position(DATE_POSITION_TOP);
		top = frame.origin.y + frame.size.h + 2;
	}
	if (date_position == DATE_POSITION_BOTTOM) {
		bottom = date_layer_frame_for_position(DATE_POSITION_BOTTOM).origin.y - 2;
	} else if (status_indicators_visible()) {
		bottom -= STATUS_RESERVED_HEIGHT;
	}
	return GRect(0, top, screen_bounds.size.w, bottom - top);
}

static GRect date_layer_frame(void)
{
	return date_layer_frame_for_position(date_position);
}

static GFont date_layer_font(void)
{
	bool tight_round_header = PBL_IF_ROUND_ELSE(true, false)
		&& date_position == DATE_POSITION_TOP && quiet_time_active
		&& date_layer_frame().size.w < 100;
	return fonts_get_system_font(tight_round_header ? FONT_KEY_GOTHIC_14 : FONT_KEY_GOTHIC_18);
}

static void quiet_time_icon_layer_update_proc(Layer *layer, GContext *ctx)
{
	if (!quiet_time_active) {
		return;
	}

	GRect bounds = layer_get_bounds(layer);
	int diameter = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
	int radius = diameter / 2;
	GPoint centre = GPoint(bounds.size.w / 2, bounds.size.h / 2);

	graphics_context_set_fill_color(ctx, date_part_colour_for_index(0));
	graphics_fill_circle(ctx, centre, radius);
	graphics_context_set_fill_color(ctx, background_colour());
	graphics_fill_circle(ctx, GPoint(centre.x + (radius / 2), centre.y - (radius / 4)), radius);
}

static int measured_date_part_width(int part, GFont font, int max_width)
{
	if (part < 0 || part >= DATE_PART_COUNT || date_text[part][0] == '\0') {
		return 0;
	}

	GSize size = graphics_text_layout_get_content_size(
		date_text[part],
		font,
		GRect(0, 0, max_width, DATE_LAYER_HEIGHT),
		GTextOverflowModeTrailingEllipsis,
		GTextAlignmentLeft
	);
	int width = size.w + 1;
	if (width < 1) {
		return 1;
	}
	if (width > max_width) {
		return max_width;
	}
	return width;
}

static int aligned_date_content_x(GRect frame, int content_width)
{
	int width = content_width;
	if (width > frame.size.w) {
		width = frame.size.w;
	}

	int x = frame.origin.x;
	if (text_align == TEXT_ALIGN_CENTER) {
		x += (frame.size.w - width) / 2;
	} else if (text_align == TEXT_ALIGN_RIGHT) {
		x += frame.size.w - width;
	}
	return x;
}

static void layoutDateLayers(void)
{
	GRect frame = date_layer_frame();
	GRect top_frame = date_layer_frame_for_position(DATE_POSITION_TOP);
	GFont font = date_layer_font();
	int widths[DATE_PART_COUNT];
	int total_width = 0;
	bool date_is_top = date_position == DATE_POSITION_TOP;
	int quiet_width = quiet_time_active ? QUIET_TIME_ICON_SIZE : 0;
	int quiet_spacing = quiet_time_active && date_is_top ? QUIET_TIME_ICON_SPACING : 0;
	int measured_width = frame.size.w - quiet_width - quiet_spacing;
	if (measured_width < 1) {
		measured_width = 1;
	}

	for (int i = 0; i < DATE_PART_COUNT; i++) {
		widths[i] = measured_date_part_width(i, font, date_is_top ? measured_width : frame.size.w);
		total_width += widths[i];
	}

	if (total_width > frame.size.w) {
		total_width = frame.size.w;
	}

	int group_width = total_width;
	if (date_is_top && quiet_time_active) {
		group_width += QUIET_TIME_ICON_SIZE + QUIET_TIME_ICON_SPACING;
	}

	int x = aligned_date_content_x(date_is_top ? top_frame : frame, group_width);
	if (date_is_top && quiet_time_active) {
		x += QUIET_TIME_ICON_SIZE + QUIET_TIME_ICON_SPACING;
	}

	for (int i = 0; i < DATE_PART_COUNT; i++) {
		if (date_layers[i] == NULL) {
			continue;
		}
		int width = widths[i] > 0 ? widths[i] : 1;
		layer_set_frame(text_layer_get_layer(date_layers[i]), GRect(x, frame.origin.y, width, frame.size.h));
		layer_set_hidden(text_layer_get_layer(date_layers[i]), date_position == DATE_POSITION_OFF || widths[i] == 0);
		x += widths[i];
	}

	if (quiet_time_icon_layer == NULL) {
		return;
	}

	if (!quiet_time_active) {
		layer_set_hidden(quiet_time_icon_layer, true);
		return;
	}

	int quiet_group_width = QUIET_TIME_ICON_SIZE;
	if (date_is_top && total_width > 0) {
		quiet_group_width += QUIET_TIME_ICON_SPACING + total_width;
	}
	int quiet_x = aligned_date_content_x(top_frame, quiet_group_width);
	int quiet_y = top_frame.origin.y + ((top_frame.size.h - QUIET_TIME_ICON_SIZE) / 2);
	layer_set_frame(quiet_time_icon_layer, GRect(quiet_x, quiet_y, QUIET_TIME_ICON_SIZE, QUIET_TIME_ICON_SIZE));
	layer_set_hidden(quiet_time_icon_layer, false);
	layer_mark_dirty(quiet_time_icon_layer);
}

static void configureDateLayer(TextLayer *textlayer, int part)
{
	if (textlayer == NULL) {
		return;
	}
	text_layer_set_font(textlayer, date_layer_font());
	text_layer_set_text_color(textlayer, date_part_colour_for_index(part));
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
	text_layer_set_overflow_mode(textlayer, GTextOverflowModeTrailingEllipsis);
}

static void configureDateLayers(void)
{
	for (int i = 0; i < DATE_PART_COUNT; i++) {
		configureDateLayer(date_layers[i], i);
	}
	layoutDateLayers();
}

static bool is_date_separator(char value)
{
	return value == '-' || value == '/' || value == ' ';
}

static void split_corner_date(char source[])
{
	for (int i = 0; i < DATE_PART_COUNT; i++) {
		date_text[i][0] = '\0';
	}

	int part = 0;
	int offset = 0;
	for (int i = 0; source[i] != '\0' && part < DATE_PART_COUNT; i++) {
		if (offset >= (int)sizeof(date_text[part]) - 1) {
			continue;
		}

		date_text[part][offset++] = source[i];
		date_text[part][offset] = '\0';

		if (is_date_separator(source[i]) && source[i + 1] != '\0' && part < DATE_PART_COUNT - 1) {
			part++;
			offset = 0;
		}
	}
}

static void update_corner_date(struct tm *date_time)
{
	static char previous_date[16];
	static int previous_position = -1;
	bool next_quiet_time = quiet_time_is_active() || test_quiet_time_active;
	bool quiet_changed = quiet_time_active != next_quiet_time;
	quiet_time_active = next_quiet_time;
	if (quiet_changed) {
		forceDisplayUpdate = true;
	}

	if (date_time == NULL) {
		configureDateLayers();
		return;
	}

	char formatted_date[16] = "";
	if (date_position != DATE_POSITION_OFF) {
		format_corner_date(formatted_date, sizeof(formatted_date), date_time);
	}
	if (!forceDisplayUpdate && !quiet_changed && previous_position == date_position
			&& strcmp(previous_date, formatted_date) == 0) {
		return;
	}
	strncpy(previous_date, formatted_date, sizeof(previous_date));
	previous_position = date_position;

	if (date_position == DATE_POSITION_OFF) {
		for (int i = 0; i < DATE_PART_COUNT; i++) {
			date_text[i][0] = '\0';
			if (date_layers[i] != NULL) {
				text_layer_set_text(date_layers[i], date_text[i]);
				layer_set_hidden(text_layer_get_layer(date_layers[i]), true);
			}
		}
		configureDateLayers();
		return;
	}

	split_corner_date(formatted_date);
	for (int i = 0; i < DATE_PART_COUNT; i++) {
		if (date_layers[i] != NULL) {
			text_layer_set_text(date_layers[i], date_text[i]);
		}
	}
	configureDateLayers();
}

static void apply_window_colours(void)
{
	if (!window) {
		return;
	}
	window_set_background_color(window, background_colour());
	if (quiet_time_icon_layer != NULL) {
		layer_mark_dirty(quiet_time_icon_layer);
	}
}

static void apply_layer_styles(void)
{
	if (!window_loaded) {
		forceDisplayUpdate = true;
		return;
	}

	for (int i = 0; i < NUM_LINES; i++)
	{
		configureLayerForFormat(lines[i].currentLayer, lines[i].currentFormat, lines[i].currentRow);
		configureLayerForFormat(lines[i].nextLayer, lines[i].nextFormat, lines[i].nextRow);
		if (lines[i].currentLayer != NULL) {
			layer_mark_dirty(text_layer_get_layer(lines[i].currentLayer));
		}
		if (lines[i].nextLayer != NULL) {
			layer_mark_dirty(text_layer_get_layer(lines[i].nextLayer));
		}
	}
	configureDateLayers();
	status_indicators_configure(low_battery_enabled, disconnect_enabled, foreground_colour());
	forceDisplayUpdate = true;
}

static bool should_use_high_resolution_layout(void);

static int measured_line_width(FontChoice choice, const char *text, int line_index, void *context)
{
	typedef struct {
		char text[BUFFER_SIZE];
		FontChoice choice;
		bool bold;
		int width;
	} WidthCacheEntry;
	static WidthCacheEntry cache[16];
	static unsigned int next_entry;
	const char *format = context;
	bool bold = format[line_index] == 'b';
	for (unsigned int i = 0; i < ARRAY_LENGTH(cache); i++) {
		if (cache[i].width > 0 && cache[i].choice == choice && cache[i].bold == bold
				&& strcmp(cache[i].text, text) == 0) {
			return cache[i].width;
		}
	}

	// A wide measurement box prevents wrapping or ellipsis from hiding the true width.
	GSize size = graphics_text_layout_get_content_size(text, font_for_choice(choice, bold),
		GRect(0, 0, 1024, 128), GTextOverflowModeWordWrap, GTextAlignmentLeft);
	WidthCacheEntry *entry = &cache[next_entry++ % ARRAY_LENGTH(cache)];
	strncpy(entry->text, text, BUFFER_SIZE);
	entry->text[BUFFER_SIZE - 1] = '\0';
	entry->choice = choice;
	entry->bold = bold;
	entry->width = size.w + 2;
	return entry->width;
}

static FontChoice choose_render_font(char text[NUM_LINES][BUFFER_SIZE], char format[])
{
	GRect content = time_content_frame();
	return text_font_choice_that_fits_measured(screen_bounds.size.w, screen_bounds.size.h,
		PBL_IF_ROUND_ELSE(true, false), font_choice, text, content.origin.y,
		screen_bounds.size.h - content.origin.y - content.size.h, measured_line_width, format);
}

static bool should_use_high_resolution_layout(void)
{
	return screen_bounds.size.w >= 200 && screen_bounds.size.h >= 200;
}

static int font_visual_rank(FontChoice choice)
{
	switch (choice) {
		case FONT_CHOICE_LARGE:
			return 0;
		case FONT_CHOICE_MEDIUM:
			return 1;
		case FONT_CHOICE_TALL:
			return 2;
		case FONT_CHOICE_CLASSIC:
			return 3;
		case FONT_CHOICE_SHARP:
			return 4;
		case FONT_CHOICE_COMPACT:
			return 5;
		default:
			return 6;
	}
}

static void copy_text_lines(char destination[NUM_LINES][BUFFER_SIZE],
		char destination_format[], char source[NUM_LINES][BUFFER_SIZE],
		char source_format[])
{
	for (int i = 0; i < NUM_LINES; i++) {
		strncpy(destination[i], source[i], BUFFER_SIZE);
		destination[i][BUFFER_SIZE - 1] = '\0';
		destination_format[i] = source_format[i];
	}
}

static void choose_time_lines(Language language, int hours, int minutes, int seconds,
		char text[NUM_LINES][BUFFER_SIZE], char format[])
{
	static const int high_resolution_limits[] = { 7, 8, 9, 10, FUZZY_TEXT_LINE_LENGTH };
	char candidate[NUM_LINES][BUFFER_SIZE];
	char candidate_format[NUM_LINES];
	FontChoice best_font = FONT_CHOICE_SMALL;
	int best_line_count = 0;
	bool found = false;

	if (!should_use_high_resolution_layout()) {
		time_to_lines(language, hours, minutes, seconds, text, format);
		render_font_choice = choose_render_font(text, format);
		return;
	}

	for (int i = 0; i < (int)(sizeof(high_resolution_limits) / sizeof(high_resolution_limits[0])); i++) {
		time_to_lines_with_limit(language, hours, minutes, seconds, high_resolution_limits[i],
			candidate, candidate_format);
		FontChoice candidate_font = choose_render_font(candidate, candidate_format);
		int candidate_line_count = count_text_lines(candidate);

		if (!found || font_visual_rank(candidate_font) < font_visual_rank(best_font)
				|| (candidate_font == best_font && candidate_line_count > best_line_count)) {
			copy_text_lines(text, format, candidate, candidate_format);
			best_font = candidate_font;
			best_line_count = candidate_line_count;
			found = true;
		}

		if (candidate_font == FONT_CHOICE_LARGE && candidate_line_count >= 3) {
			break;
		}
	}

	render_font_choice = best_font;
}

static void choose_date_lines(Language language, int day, int date, int month,
		char text[NUM_LINES][BUFFER_SIZE], char format[])
{
	static const int high_resolution_limits[] = { 7, 8, 9, 10, FUZZY_TEXT_LINE_LENGTH };
	char candidate[NUM_LINES][BUFFER_SIZE];
	char candidate_format[NUM_LINES];
	FontChoice best_font = FONT_CHOICE_SMALL;
	int best_line_count = 0;
	bool found = false;

	if (!should_use_high_resolution_layout()) {
		date_to_lines(language, day, date, month, text, format);
		render_font_choice = choose_render_font(text, format);
		return;
	}

	for (int i = 0; i < (int)(sizeof(high_resolution_limits) / sizeof(high_resolution_limits[0])); i++) {
		date_to_lines_with_limit(language, day, date, month, high_resolution_limits[i],
			candidate, candidate_format);
		FontChoice candidate_font = choose_render_font(candidate, candidate_format);
		int candidate_line_count = count_text_lines(candidate);

		if (!found || font_visual_rank(candidate_font) < font_visual_rank(best_font)
				|| (candidate_font == best_font && candidate_line_count > best_line_count)) {
			copy_text_lines(text, format, candidate, candidate_format);
			best_font = candidate_font;
			best_line_count = candidate_line_count;
			found = true;
		}

		if (candidate_font == FONT_CHOICE_LARGE && candidate_line_count >= 3) {
			break;
		}
	}

	render_font_choice = best_font;
}

// Configure the layers for the given text
static int configureLayersForText(char text[NUM_LINES][BUFFER_SIZE], char format[])
{
	int numLines = 0;

	// Set bold layer.
	int i;
	for (i = 0; i < NUM_LINES; i++) {
		if (strlen(text[i]) > 0) {
			lines[i].nextFormat = format[i];
			lines[i].nextRow = format[i] == 'b' ? 2 : i;
			configureLayerForFormat(lines[i].nextLayer, format[i], lines[i].nextRow);
		}
		else
		{
			break;
		}
	}
	numLines = i;

	// Set y positions for the lines
	for (int i = 0; i < numLines; i++)
	{
		GRect content = time_content_frame();
		TextFrame frame = text_frame_for_line_in_area(screen_bounds.size.w, screen_bounds.size.h,
			PBL_IF_ROUND_ELSE(true, false), render_font_choice, numLines, i,
			content.origin.y, screen_bounds.size.h - content.origin.y - content.size.h);
		if (lines[i].nextLayer != NULL) {
			layer_set_frame((Layer *)lines[i].nextLayer, GRect(screen_bounds.size.w, frame.y, frame.w, frame.h));
		}
	}

	return numLines;
}

// Update screen based on new time
static void display_time(struct tm *t)
{
  // The current time text will be stored in the following strings
  char textLine[NUM_LINES][BUFFER_SIZE];
  char format[NUM_LINES];
  FontChoice previous_font = render_font_choice;
  update_corner_date(t);
  
  if (exact_time_visible || test_exact_time_active) {
  	exact_time_to_lines(t, textLine, format);
  } else if (showTime || dateTimeout > 1) {
  	choose_time_lines(lang, t->tm_hour, t->tm_min, t->tm_sec, textLine, format);
    dateTimeout = 0;
    showTime = true;
  } else {
    choose_date_lines(lang, t->tm_wday, t->tm_mday, t->tm_mon, textLine, format);
  }
  
  int nextNLines = count_text_lines(textLine);
  bool layout_changed = forceDisplayUpdate || previous_font != render_font_choice
      || nextNLines != currentNLines;
  bool text_changed = layout_changed;
  for (int i = 0; i < NUM_LINES && !text_changed; i++) {
    text_changed = needToUpdateLine(&lines[i], textLine[i])
        || (textLine[i][0] != '\0' && lines[i].currentFormat != format[i]);
  }
  if (!text_changed) {
    return;
  }
  configureLayersForText(textLine, format);

  int delay = 0;
  for (int i = 0; i < NUM_LINES; i++) {
    if (layout_changed || needToUpdateLine(&lines[i], textLine[i])
        || (textLine[i][0] != '\0' && lines[i].currentFormat != format[i])) {
      updateLineTo(&lines[i], textLine[i], delay);
      delay += ANIMATION_STAGGER_TIME;
    }
  }

  currentNLines = nextNLines;
  forceDisplayUpdate = false;
}

static void initLineForStart(Line* line)
{
	// Switch current and next layer
	TextLayer* tmp  = line->currentLayer;
	line->currentLayer = line->nextLayer;
	line->nextLayer = tmp;
	char tmp_format = line->currentFormat;
	line->currentFormat = line->nextFormat;
	line->nextFormat = tmp_format;
	int tmp_row = line->currentRow;
	line->currentRow = line->nextRow;
	line->nextRow = tmp_row;
	if (line->currentLayer == NULL) {
		return;
	}

	// Move current layer to screen;
	GRect rect = layer_get_frame((Layer *)line->currentLayer);
	rect.origin.x = (screen_bounds.size.w - rect.size.w) / 2;
	layer_set_frame((Layer *)line->currentLayer, rect);
}

// Update screen without animation first time we start the watchface
static void display_initial_time(struct tm *t)
{
	// The current time text will be stored in the following strings
	char textLine[NUM_LINES][BUFFER_SIZE];
	char format[NUM_LINES];
	forceDisplayUpdate = true;
	update_corner_date(t);

	choose_time_lines(lang, t->tm_hour, t->tm_min, t->tm_sec, textLine, format);

	// This configures the nextLayer for each line
	currentNLines = configureLayersForText(textLine, format);

	// Set the text and configure layers to the start position
	for (int i = 0; i < currentNLines; i++)
	{
		updateLayerText(lines[i].nextLayer, textLine[i]);
		// This call switches current- and nextLayer
		initLineForStart(&lines[i]);
	}	
	forceDisplayUpdate = false;
}

// Time handler called every minute by the system
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed)
{
	if (use_test_time) {
		return;
	}
	current_time = *tick_time;
	t = &current_time;
  
  if (!showTime) {
    dateTimeout++;
  }
  
	display_time(t);
}

static void cancel_timer(AppTimer **timer)
{
	if (*timer == NULL) {
		return;
	}

	app_timer_cancel(*timer);
	*timer = NULL;
}

static void tap_reset_timer_callback(void *context)
{
	(void)context;

	tap_reset_timer = NULL;
	tap_pending = false;
}

static void exact_time_timer_callback(void *context)
{
	(void)context;

	exact_time_timer = NULL;
	exact_time_visible = false;
	forceDisplayUpdate = true;
	if (t != NULL && window_loaded) {
		display_time(t);
	}
}

static void show_exact_time_briefly(void)
{
	cancel_timer(&exact_time_timer);
	exact_time_visible = true;
	forceDisplayUpdate = true;
	if (t != NULL && window_loaded) {
		display_time(t);
	}
	exact_time_timer = app_timer_register(exact_time_seconds * 1000, exact_time_timer_callback, NULL);
	if (exact_time_timer == NULL) {
		exact_time_timer_callback(NULL);
	}
}

static void tap_handler(AccelAxisType axis, int32_t direction)
{
	(void)axis;
	(void)direction;
	if (!gesture_enabled) {
		return;
	}

	if (tap_pending) {
		cancel_timer(&tap_reset_timer);
		tap_pending = false;
		show_exact_time_briefly();
		return;
	}

	tap_pending = true;
	tap_reset_timer = app_timer_register(DOUBLE_TAP_WINDOW_MS, tap_reset_timer_callback, NULL);
	if (tap_reset_timer == NULL) {
		tap_pending = false;
	}
}

static void configure_gesture_service(void)
{
	if (gesture_enabled != gesture_subscribed) {
		gesture_subscribed = gesture_enabled;
		if (gesture_enabled) {
			accel_tap_service_subscribe(tap_handler);
		} else {
			accel_tap_service_unsubscribe();
		}
	}
	if (!gesture_enabled) {
		cancel_timer(&tap_reset_timer);
		cancel_timer(&exact_time_timer);
		tap_pending = false;
		exact_time_visible = false;
		forceDisplayUpdate = true;
	}
}

/**
 * Debug methods. For quickly debugging enable debug macro on top to transform the watchface into
 * a standard app and you will be able to change the time with the up and down buttons
 */
#if DEBUG

static void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
	(void)recognizer;
	(void)window;
	
	t->tm_min += 5;
	if (t->tm_min >= 60) {
		t->tm_min = 0;
		t->tm_hour += 1;
		
		if (t->tm_hour >= 24) {
			t->tm_hour = 0;
		}
	}
	display_time(t);
}


static void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
	(void)recognizer;
	(void)window;
	
	t->tm_min -= 5;
	if (t->tm_min < 0) {
		t->tm_min = 55;
		t->tm_hour -= 1;
		
		if (t->tm_hour < 0) {
			t->tm_hour = 23;
		}
	}
	display_time(t);
}

static void click_config_provider(ClickConfig **config, Window *window) {
  (void)window;

  config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
  config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;

  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
  config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 100;
}

#endif

static void apply_settings_tuple(const uint32_t key, const Tuple* new_tuple) {
	switch (key) {
		case TEXT_ALIGN_KEY:
			text_align = valid_text_align(new_tuple->value->uint8);
			persist_write_int(TEXT_ALIGN_KEY, text_align);
			APP_LOG(APP_LOG_LEVEL_DEBUG, "Set text alignment: %u", text_align);
			apply_layer_styles();
			break;
		case INVERT_KEY:
			if (new_tuple->value->uint8 == 1)
			{
				foreground_color = 0x000000;
				background_color = 0xFFFFFF;
			}
			else
			{
				foreground_color = 0xFFFFFF;
				background_color = 0x000000;
			}
			persist_write_int(FOREGROUND_COLOR_KEY, foreground_color);
			persist_write_int(BACKGROUND_COLOR_KEY, background_color);
			apply_window_colours();
			apply_layer_styles();
			break;
		case LANGUAGE_KEY:
			lang = valid_language((Language) new_tuple->value->uint8);
			persist_write_int(LANGUAGE_KEY, lang);
			APP_LOG(APP_LOG_LEVEL_DEBUG, "Set language: %u", lang);
			break;
		case FONT_CHOICE_KEY:
			font_choice = (FontChoice) new_tuple->value->uint8;
			if (font_choice >= FONT_CHOICE_COUNT)
			{
				font_choice = FONT_CHOICE_CLASSIC;
			}
			persist_write_int(FONT_CHOICE_KEY, font_choice);
			apply_layer_styles();
			break;
		case FOREGROUND_COLOR_KEY:
			foreground_color = new_tuple->value->int32;
			persist_write_int(FOREGROUND_COLOR_KEY, foreground_color);
			apply_layer_styles();
			break;
		case BACKGROUND_COLOR_KEY:
			background_color = new_tuple->value->int32;
			persist_write_int(BACKGROUND_COLOR_KEY, background_color);
			apply_window_colours();
			break;
		case DATE_POSITION_KEY:
			forceDisplayUpdate = true;
			date_position = valid_date_position(new_tuple->value->uint8);
			persist_write_int(DATE_POSITION_KEY, date_position);
			update_corner_date(t);
			break;
		case DATE_FORMAT_KEY:
			date_format = valid_date_format(new_tuple->value->uint8);
			persist_write_int(DATE_FORMAT_KEY, date_format);
			update_corner_date(t);
			break;
		case ROW_ONE_COLOR_KEY:
			row_colors[0] = new_tuple->value->int32;
			persist_write_int(ROW_ONE_COLOR_KEY, row_colors[0]);
			apply_layer_styles();
			break;
		case ROW_TWO_COLOR_KEY:
			row_colors[1] = new_tuple->value->int32;
			persist_write_int(ROW_TWO_COLOR_KEY, row_colors[1]);
			apply_layer_styles();
			break;
		case ROW_THREE_COLOR_KEY:
			row_colors[2] = new_tuple->value->int32;
			persist_write_int(ROW_THREE_COLOR_KEY, row_colors[2]);
			apply_layer_styles();
			break;
		case DATE_PART_ONE_COLOR_KEY:
			date_part_colors[0] = new_tuple->value->int32;
			persist_write_int(DATE_PART_ONE_COLOR_KEY, date_part_colors[0]);
			configureDateLayers();
			break;
		case DATE_PART_TWO_COLOR_KEY:
			date_part_colors[1] = new_tuple->value->int32;
			persist_write_int(DATE_PART_TWO_COLOR_KEY, date_part_colors[1]);
			configureDateLayers();
			break;
		case DATE_PART_THREE_COLOR_KEY:
			date_part_colors[2] = new_tuple->value->int32;
			persist_write_int(DATE_PART_THREE_COLOR_KEY, date_part_colors[2]);
			configureDateLayers();
			break;
		case ANIMATIONS_ENABLED_KEY:
			animations_enabled = valid_toggle(new_tuple->value->int32, true);
			persist_write_int(ANIMATIONS_ENABLED_KEY, animations_enabled);
			forceDisplayUpdate = true;
			break;
		case GESTURE_ENABLED_KEY:
			gesture_enabled = valid_toggle(new_tuple->value->int32, true);
			persist_write_int(GESTURE_ENABLED_KEY, gesture_enabled);
			configure_gesture_service();
			break;
		case EXACT_TIME_SECONDS_KEY:
			exact_time_seconds = valid_exact_time_seconds(new_tuple->value->int32);
			persist_write_int(EXACT_TIME_SECONDS_KEY, exact_time_seconds);
			break;
		case LOW_BATTERY_ENABLED_KEY:
			low_battery_enabled = valid_toggle(new_tuple->value->int32, false);
			persist_write_int(LOW_BATTERY_ENABLED_KEY, low_battery_enabled);
			status_indicators_configure(low_battery_enabled, disconnect_enabled, foreground_colour());
			break;
		case DISCONNECT_ENABLED_KEY:
			disconnect_enabled = valid_toggle(new_tuple->value->int32, false);
			persist_write_int(DISCONNECT_ENABLED_KEY, disconnect_enabled);
			status_indicators_configure(low_battery_enabled, disconnect_enabled, foreground_colour());
			break;
		case TEST_EXACT_TIME_KEY:
			test_exact_time_active = new_tuple->value->uint8 == 1;
			if (t != NULL && window_loaded) {
				display_time(t);
			}
			break;
		case TEST_QUIET_TIME_KEY:
			test_quiet_time_active = new_tuple->value->uint8 == 1;
			update_corner_date(t);
			break;
		case TEST_HOUR_KEY:
			requested_test_hour = new_tuple->value->uint8;
			pending_test_hour = true;
			break;
		case TEST_MINUTE_KEY:
			requested_test_minute = new_tuple->value->uint8;
			pending_test_minute = true;
			break;
	}
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context)
{
	Tuple *tuple = dict_read_first(iterator);

	while (tuple != NULL) {
		apply_settings_tuple(tuple->key, tuple);
		tuple = dict_read_next(iterator);
	}

	if (pending_test_hour && pending_test_minute) {
		time_t raw_time;
		time(&raw_time);
		test_time = *localtime(&raw_time);
		test_time.tm_hour = requested_test_hour;
		test_time.tm_min = requested_test_minute;
		test_time.tm_sec = 0;
		t = &test_time;
		use_test_time = true;
	}
	pending_test_hour = false;
	pending_test_minute = false;

	if (t && window_loaded) {
		display_time(t);
	}
}

static void inbox_dropped_callback(AppMessageResult reason, void *context)
{
	APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message dropped: %d", reason);
}

#ifdef WORDISH_SMOOTH_FONTS
// Retain TextLayer storage/animation, drawing the same measured font in three masks.
static void smooth_time_layer_update(Layer *layer, GContext *ctx)
{
	for (int i = 0; i < NUM_LINES; i++) {
		bool current = lines[i].currentLayer != NULL
			&& layer == text_layer_get_layer(lines[i].currentLayer);
		if (!current && (lines[i].nextLayer == NULL
				|| layer != text_layer_get_layer(lines[i].nextLayer))) {
			continue;
		}
		TextLayer *text_layer = current ? lines[i].currentLayer : lines[i].nextLayer;
		bool bold = (current ? lines[i].currentFormat : lines[i].nextFormat) == 'b';
		int row = current ? lines[i].currentRow : lines[i].nextRow;
		const char *text = text_layer_get_text(text_layer);
		if (text == NULL || text[0] == '\0') {
			return;
		}
		GRect bounds = layer_get_bounds(layer);
		GTextAlignment alignment = lookup_text_alignment(text_align);
		GColor colour = row_colour_for_index(row);
		if (!smooth_font_draw(ctx, text, render_font_choice, bold, bounds,
				alignment, colour, background_colour())) {
			graphics_context_set_text_color(ctx, colour);
			graphics_draw_text(ctx, text, font_for_choice(render_font_choice, bold), bounds,
				GTextOverflowModeTrailingEllipsis, alignment, NULL);
		}
		return;
	}
}
#endif

static void init_line(Line* line)
{
	// Create layers with dummy position to the right of the screen
	line->currentLayer = text_layer_create(GRect(screen_bounds.size.w, 0, screen_bounds.size.w, 50));
	line->nextLayer = text_layer_create(GRect(screen_bounds.size.w, 0, screen_bounds.size.w, 50));
	if (line->currentLayer == NULL || line->nextLayer == NULL) {
		APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create text layers");
		return;
	}

#ifdef WORDISH_SMOOTH_FONTS
	layer_set_update_proc(text_layer_get_layer(line->currentLayer), smooth_time_layer_update);
	layer_set_update_proc(text_layer_get_layer(line->nextLayer), smooth_time_layer_update);
#endif

	// Set the text buffers
	line->lineStr1[0] = '\0';
	line->lineStr2[0] = '\0';
	line->currentFormat = ' ';
	line->nextFormat = ' ';
	line->currentRow = 0;
	line->nextRow = 0;
	// Configure a style
	configureLightLayer(line->currentLayer, line->currentRow);
	configureLightLayer(line->nextLayer, line->nextRow);
	text_layer_set_text(line->currentLayer, line->lineStr1);
	text_layer_set_text(line->nextLayer, line->lineStr2);

	// Initially there are no animations
	line->animation1 = NULL;
	line->animation2 = NULL;
}

static void destroy_line(Line* line)
{
	destroy_property_animation(&line->animation1);
	destroy_property_animation(&line->animation2);

	// Free layers
	if (line->currentLayer != NULL) {
		text_layer_destroy(line->currentLayer);
		line->currentLayer = NULL;
	}
	if (line->nextLayer != NULL) {
		text_layer_destroy(line->nextLayer);
		line->nextLayer = NULL;
	}
}

static void window_load(Window *window)
{
	Layer *window_layer = window_get_root_layer(window);
	screen_bounds = layer_get_bounds(window_layer);
	apply_window_colours();

	// Init and load lines
	for (int i = 0; i < NUM_LINES; i++)
	{
		init_line(&lines[i]);
		if (lines[i].currentLayer != NULL) {
			layer_add_child(window_layer, (Layer *)lines[i].currentLayer);
		}
		if (lines[i].nextLayer != NULL) {
			layer_add_child(window_layer, (Layer *)lines[i].nextLayer);
		}
	}

	for (int i = 0; i < DATE_PART_COUNT; i++) {
		date_layers[i] = text_layer_create(date_layer_frame());
		if (date_layers[i] == NULL) {
			APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create date layer part %d", i);
			continue;
		}
		date_text[i][0] = '\0';
		text_layer_set_text(date_layers[i], date_text[i]);
		configureDateLayer(date_layers[i], i);
		layer_add_child(window_layer, text_layer_get_layer(date_layers[i]));
	}

	quiet_time_icon_layer = layer_create(GRect(0, 0, QUIET_TIME_ICON_SIZE, QUIET_TIME_ICON_SIZE));
	if (quiet_time_icon_layer == NULL) {
		APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create quiet time icon layer");
	} else {
		layer_set_update_proc(quiet_time_icon_layer, quiet_time_icon_layer_update_proc);
		layer_set_hidden(quiet_time_icon_layer, true);
		layer_add_child(window_layer, quiet_time_icon_layer);
	}
	status_indicators_init(window_layer, screen_bounds, status_visibility_changed);
	status_indicators_configure(low_battery_enabled, disconnect_enabled, foreground_colour());
	window_loaded = true;

	// Configure time on init
	time_t raw_time;

	time(&raw_time);
	current_time = *localtime(&raw_time);
	t = &current_time;
	display_initial_time(t);

}

static void window_unload(Window *window)
{
	window_loaded = false;
	status_indicators_deinit();

	for (int i = 0; i < NUM_LINES; i++)
	{
		destroy_line(&lines[i]);
	}
	for (int i = 0; i < DATE_PART_COUNT; i++) {
		if (date_layers[i] != NULL) {
			text_layer_destroy(date_layers[i]);
			date_layers[i] = NULL;
		}
	}
	if (quiet_time_icon_layer != NULL) {
		layer_destroy(quiet_time_icon_layer);
		quiet_time_icon_layer = NULL;
	}
	unload_custom_fonts();
}

static void handle_init() {
	if (!persist_exists(SETTINGS_SCHEMA_KEY)
			|| persist_read_int(SETTINGS_SCHEMA_KEY) < SETTINGS_SCHEMA_VERSION)
	{
		if (!persist_exists(TEXT_ALIGN_KEY)) {
			persist_write_int(TEXT_ALIGN_KEY, text_align);
		}
		if (!persist_exists(FOREGROUND_COLOR_KEY)) {
			if (persist_exists(INVERT_KEY) && persist_read_bool(INVERT_KEY)) {
				foreground_color = 0x000000;
			}
			persist_write_int(FOREGROUND_COLOR_KEY, foreground_color);
		}
		if (!persist_exists(BACKGROUND_COLOR_KEY)) {
			if (persist_exists(INVERT_KEY) && persist_read_bool(INVERT_KEY)) {
				background_color = 0xFFFFFF;
			}
			persist_write_int(BACKGROUND_COLOR_KEY, background_color);
		}
		if (!persist_exists(FONT_CHOICE_KEY)) {
			persist_write_int(FONT_CHOICE_KEY, font_choice);
		}
		if (!persist_exists(LANGUAGE_KEY)) {
			persist_write_int(LANGUAGE_KEY, lang);
		}
		if (!persist_exists(DATE_POSITION_KEY)) {
			persist_write_int(DATE_POSITION_KEY, date_position);
		}
		if (!persist_exists(DATE_FORMAT_KEY)) {
			persist_write_int(DATE_FORMAT_KEY, date_format);
		}
		if (!persist_exists(ROW_ONE_COLOR_KEY)) {
			persist_write_int(ROW_ONE_COLOR_KEY, row_colors[0]);
		}
		if (!persist_exists(ROW_TWO_COLOR_KEY)) {
			persist_write_int(ROW_TWO_COLOR_KEY, row_colors[1]);
		}
		if (!persist_exists(ROW_THREE_COLOR_KEY)) {
			persist_write_int(ROW_THREE_COLOR_KEY, row_colors[2]);
		}
		if (!persist_exists(DATE_PART_ONE_COLOR_KEY)) {
			persist_write_int(DATE_PART_ONE_COLOR_KEY, date_part_colors[0]);
		}
		if (!persist_exists(DATE_PART_TWO_COLOR_KEY)) {
			persist_write_int(DATE_PART_TWO_COLOR_KEY, date_part_colors[1]);
		}
		if (!persist_exists(DATE_PART_THREE_COLOR_KEY)) {
			persist_write_int(DATE_PART_THREE_COLOR_KEY, date_part_colors[2]);
		}
		persist_write_int(SETTINGS_SCHEMA_KEY, SETTINGS_SCHEMA_VERSION);
	}

	animations_enabled = !persist_exists(ANIMATIONS_ENABLED_KEY)
		|| valid_toggle(persist_read_int(ANIMATIONS_ENABLED_KEY), true);
	gesture_enabled = !persist_exists(GESTURE_ENABLED_KEY)
		|| valid_toggle(persist_read_int(GESTURE_ENABLED_KEY), true);
	exact_time_seconds = persist_exists(EXACT_TIME_SECONDS_KEY)
		? valid_exact_time_seconds(persist_read_int(EXACT_TIME_SECONDS_KEY)) : 3;
	low_battery_enabled = persist_exists(LOW_BATTERY_ENABLED_KEY)
		&& valid_toggle(persist_read_int(LOW_BATTERY_ENABLED_KEY), false);
	disconnect_enabled = persist_exists(DISCONNECT_ENABLED_KEY)
		&& valid_toggle(persist_read_int(DISCONNECT_ENABLED_KEY), false);

	// Load settings from persistent storage
	if (persist_exists(TEXT_ALIGN_KEY))
	{
		text_align = valid_text_align(persist_read_int(TEXT_ALIGN_KEY));
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read text alignment from store: %u", text_align);
	}
	if (persist_exists(LANGUAGE_KEY))
	{
		lang = valid_language((Language) persist_read_int(LANGUAGE_KEY));
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read language from store: %u", lang);
	}
	if (persist_exists(FONT_CHOICE_KEY))
	{
		font_choice = (FontChoice) persist_read_int(FONT_CHOICE_KEY);
		if (font_choice >= FONT_CHOICE_COUNT)
		{
			font_choice = FONT_CHOICE_CLASSIC;
		}
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read font choice from store: %u", font_choice);
	}
	if (persist_exists(FOREGROUND_COLOR_KEY))
	{
		foreground_color = persist_read_int(FOREGROUND_COLOR_KEY);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read foreground colour from store: %u", foreground_color);
	}
	else if (persist_exists(INVERT_KEY) && persist_read_bool(INVERT_KEY))
	{
		foreground_color = 0x000000;
	}
	if (persist_exists(BACKGROUND_COLOR_KEY))
	{
		background_color = persist_read_int(BACKGROUND_COLOR_KEY);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read background colour from store: %u", background_color);
	}
	else if (persist_exists(INVERT_KEY) && persist_read_bool(INVERT_KEY))
	{
		background_color = 0xFFFFFF;
	}
	if (persist_exists(DATE_POSITION_KEY))
	{
		date_position = valid_date_position(persist_read_int(DATE_POSITION_KEY));
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read date position from store: %u", date_position);
	}
	if (persist_exists(DATE_FORMAT_KEY))
	{
		date_format = valid_date_format(persist_read_int(DATE_FORMAT_KEY));
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read date format from store: %u", date_format);
	}
	if (persist_exists(ROW_ONE_COLOR_KEY))
	{
		row_colors[0] = persist_read_int(ROW_ONE_COLOR_KEY);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read row one colour from store: %d", row_colors[0]);
	}
	if (persist_exists(ROW_TWO_COLOR_KEY))
	{
		row_colors[1] = persist_read_int(ROW_TWO_COLOR_KEY);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read row two colour from store: %d", row_colors[1]);
	}
	if (persist_exists(ROW_THREE_COLOR_KEY))
	{
		row_colors[2] = persist_read_int(ROW_THREE_COLOR_KEY);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read row three colour from store: %d", row_colors[2]);
	}
	if (persist_exists(DATE_PART_ONE_COLOR_KEY))
	{
		date_part_colors[0] = persist_read_int(DATE_PART_ONE_COLOR_KEY);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read date part one colour from store: %d", date_part_colors[0]);
	}
	if (persist_exists(DATE_PART_TWO_COLOR_KEY))
	{
		date_part_colors[1] = persist_read_int(DATE_PART_TWO_COLOR_KEY);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read date part two colour from store: %d", date_part_colors[1]);
	}
	if (persist_exists(DATE_PART_THREE_COLOR_KEY))
	{
		date_part_colors[2] = persist_read_int(DATE_PART_THREE_COLOR_KEY);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Read date part three colour from store: %d", date_part_colors[2]);
	}

	window = window_create();
	window_set_background_color(window, colour_from_rgb(background_color));
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload
	});

	const bool animated = true;
	window_stack_push(window, animated);

	// Initialize message queue after window load so startup settings cannot race layer creation.
	const int inbound_size = 256;
	const int outbound_size = 64;
	app_message_register_inbox_received(inbox_received_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_open(inbound_size, outbound_size);
  
	// Subscribe to minute ticks
	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
	configure_gesture_service();

#if DEBUG
	// Button functionality
	window_set_click_config_provider(window, (ClickConfigProvider) click_config_provider);
#endif
}

static void handle_deinit()
{
	cancel_timer(&tap_reset_timer);
	cancel_timer(&exact_time_timer);
	if (gesture_subscribed) {
		accel_tap_service_unsubscribe();
		gesture_subscribed = false;
	}
	tick_timer_service_unsubscribe();
	app_message_deregister_callbacks();

	// Free window
	window_destroy(window);
}

int main(void)
{
	handle_init();
	app_event_loop();
	handle_deinit();
	return 0;
}
