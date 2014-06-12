#include <pebble.h>
#include "compass_layer.h"
#include "ticks_layer.h"

static Window *window;
static TextLayer *text_layer;
static CompassLayer *compass_layer;
static TicksLayer *ticks_layer;

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    compass_layer_set_angle(compass_layer, 0);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    compass_layer_set_angle(compass_layer, compass_layer_get_angle(compass_layer) + TRIG_MAX_ANGLE / 10);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    compass_layer_set_angle(compass_layer, compass_layer_get_angle(compass_layer) - TRIG_MAX_ANGLE / 5);
}

static void click_config_provider(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void compass_layer_update_callback(CompassLayer* layer) {
    ticks_layer_set_angle(ticks_layer, compass_layer_get_presentation_angle(layer));
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    GRect roseRect = ((GRect){.size={bounds.size.w, (int16_t)(bounds.size.h-30)}});

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

    text_layer = text_layer_create((GRect) { .origin = { 0, (int16_t)(bounds.size.h-20)}, .size = { bounds.size.w, 20 } });
    text_layer_set_text(text_layer, "Press a button");
    text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(text_layer));

    InverterLayer *inverter_layer = inverter_layer_create(bounds);
    layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));
}

static void window_unload(Window *window) {
    text_layer_destroy(text_layer);
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
