#include <pebble.h>
#include "compass_layer.h"
#include "ticks_layer.h"

static Window *window;
static TextLayer *angle_layer;
static GRect angle_layer_rect_rose;
static GRect angle_layer_rect_band;

static TextLayer *direction_layer;
static GRect direction_layer_rect_rose;
static GRect direction_layer_rect_band;
static CompassLayer *compass_layer;

static TicksLayer *ticks_layer;

static InverterLayer *needle_layer;
static GRect needle_layer_rect_rose;
static GRect needle_layer_rect_band;


static void compass_layer_update_callback(CompassLayer* layer);

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
//    compass_layer_set_angle(compass_layer, 0);
//    ticks_layer_set_transition_factor(ticks_layer, ticks_layer_get_transition_factor(ticks_layer) - 0.1f);
//    compass_layer_update_callback(compass_layer);

    ticks_layer_set_shows_band(ticks_layer, !ticks_layer_get_shows_band(ticks_layer));
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    //compass_layer_set_angle(compass_layer, compass_layer_get_angle(compass_layer) - TRIG_MAX_ANGLE / 10);
    ticks_layer_set_transition_factor(ticks_layer, ticks_layer_get_transition_factor(ticks_layer) + 0.1f);
    compass_layer_update_callback(compass_layer);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    compass_layer_set_angle(compass_layer, compass_layer_get_angle(compass_layer) + TRIG_MAX_ANGLE / 5);
}

static void click_config_provider(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

GRect rect_blend(GRect *r1, GRect *r2, float f) {
    return (GRect){
        {
            (int16_t) (r1->origin.x * (1-f) + f * r2->origin.x),
            (int16_t) (r1->origin.y * (1-f) + f * r2->origin.y),
        },
        {
            (int16_t) (r1->size.w * (1-f) + f * r2->size.w),
            (int16_t) (r1->size.h * (1-f) + f * r2->size.h),
        },
    };
}

static void compass_layer_update_callback(CompassLayer* layer) {
    int32_t angle = compass_layer_get_presentation_angle(layer);
    ticks_layer_set_angle(ticks_layer, angle);

    const float blend_factor = ticks_layer_get_transition_factor(ticks_layer);

    static char angle_text[] = "123°";
    int32_t normalized_angle = ((int)(angle * 360 / TRIG_MAX_ANGLE) % 360 + 360) % 360;
    snprintf(angle_text, sizeof(angle_text), "%d°", (int)normalized_angle);
    text_layer_set_text(angle_layer, angle_text);
    GRect r = rect_blend(&angle_layer_rect_rose, &angle_layer_rect_band, blend_factor);
    layer_set_frame(text_layer_get_layer(angle_layer), r);
    // workound!
    // TODO: file a bug for tintin applib
    layer_set_bounds(text_layer_get_layer(angle_layer), (GRect){.size=r.size});
    text_layer_set_text_alignment(angle_layer, GTextAlignmentRight);

    static char *direction_texts[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    int32_t direction_index = ((normalized_angle + 23) / (360 / ARRAY_LENGTH(direction_texts))) % ARRAY_LENGTH(direction_texts);
//    int32_t direction_index = 1;
    text_layer_set_text(direction_layer, direction_texts[direction_index]);
    layer_set_frame(text_layer_get_layer(direction_layer), rect_blend(&direction_layer_rect_rose, &direction_layer_rect_band, blend_factor));

    layer_set_frame(inverter_layer_get_layer(needle_layer), rect_blend(&needle_layer_rect_rose, &needle_layer_rect_band, blend_factor));
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    const int16_t text_height_rose = 24;
    const int16_t text_height_band = 55;
    const GFont text_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

    const int16_t direction_layer_width = 40;
    const int16_t direction_layer_margin_band = 40;
    direction_layer_rect_rose = (GRect) { .origin = { bounds.size.w - direction_layer_width, (int16_t)(bounds.size.h- text_height_rose)}, .size = { direction_layer_width, text_height_rose} };
    direction_layer_rect_band = (GRect) { .origin = { bounds.size.w - direction_layer_width-direction_layer_margin_band, (int16_t)(bounds.size.h- text_height_band)}, .size = { direction_layer_width, text_height_rose} };
    direction_layer = text_layer_create(direction_layer_rect_rose);
    text_layer_set_text_alignment(direction_layer, GTextAlignmentCenter);
    text_layer_set_font(direction_layer, text_font);
    layer_add_child(window_layer, text_layer_get_layer(direction_layer));

    const int16_t angle_layer_width_rose = 40;
    const int16_t angle_layer_width_band = 65;

    angle_layer_rect_rose = (GRect) { .origin = { 0, (int16_t)(bounds.size.h- text_height_rose)}, .size = { angle_layer_width_rose, text_height_rose} };
    angle_layer_rect_band = (GRect) { .origin = { 0, (int16_t)(bounds.size.h- text_height_band)}, .size = { angle_layer_width_band, text_height_band} };
    angle_layer = text_layer_create(angle_layer_rect_rose);
    text_layer_set_text_alignment(angle_layer, GTextAlignmentRight);
    text_layer_set_font(angle_layer, text_font);
    layer_add_child(window_layer, text_layer_get_layer(angle_layer));


    GRect roseRect = ((GRect){.origin={0, 8}, .size={bounds.size.w, (int16_t)(bounds.size.h-15)}});

    ticks_layer = ticks_layer_create(roseRect);
    layer_add_child(window_layer, ticks_layer_get_layer(ticks_layer));

    roseRect.origin.x += 10;
    roseRect.origin.y += 10;
    roseRect.size.h -= 20;
    roseRect.size.w -= 20;
    compass_layer = compass_layer_create(roseRect);
    compass_layer_set_update_callback(compass_layer, compass_layer_update_callback);
    layer_set_hidden(compass_layer_get_layer(compass_layer), true);
    layer_add_child(window_layer, compass_layer_get_layer(compass_layer));

    InverterLayer *inverter_layer = inverter_layer_create(bounds);
    layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));

    needle_layer_rect_rose = (GRect){{71,0}, {3,20}};
    needle_layer_rect_band = (GRect){{71,18}, {3,40}};
    needle_layer = inverter_layer_create(needle_layer_rect_rose);
    layer_add_child(window_layer, inverter_layer_get_layer(needle_layer));

    compass_layer_set_angle(compass_layer, 45* TRIG_MAX_ANGLE/360);
}

static void window_unload(Window *window) {
    text_layer_destroy(angle_layer);
    compass_layer_destroy(compass_layer);
}

static void init(void) {
    window = window_create();
//    window_set_background_color(window, GColorBlack);
    window_set_click_config_provider(window, click_config_provider);
    window_set_window_handlers(window, (WindowHandlers) {
            .load = window_load,
            .unload = window_unload,
    });
    const bool animated = true;
    window_stack_push(window, animated);
}

static void deinit(void) {
    window_destroy(window);
}

int main(void) {
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();
    deinit();
    return 0;
}
