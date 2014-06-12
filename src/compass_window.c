#include "compass_window.h"
#include "ticks_layer.h"
#include "needle_layer.h"

typedef struct {
    GRect direction_layer_rect_rose;
    GRect direction_layer_rect_band;
    TextLayer *direction_layer;

    GRect angle_layer_rect_rose;
    GRect angle_layer_rect_band;
    TextLayer *angle_layer;

    TicksLayer *ticks_layer;
    NeedleLayer *needle_layer;

    GRect pointer_layer_rect_rose;
    GRect pointer_layer_rect_band;
    InverterLayer *pointer_layer;
} CompassWindowData;

Window *compass_window_get_window(CompassWindow *window) {
    return (Window *)window;
}

static void compass_layer_update_callback(NeedleLayer * layer);

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
//    needle_layer_set_angle(needle_layer, 0);
//    ticks_layer_set_transition_factor(ticks_layer, ticks_layer_get_transition_factor(ticks_layer) - 0.1f);
//    compass_layer_update_callback(needle_layer);
    CompassWindowData *data = context;
    ticks_layer_set_shows_band(data->ticks_layer, !ticks_layer_get_shows_band(data->ticks_layer));
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    //needle_layer_set_angle(needle_layer, needle_layer_get_angle(needle_layer) - TRIG_MAX_ANGLE / 10);
    CompassWindowData *data = context;
    ticks_layer_set_transition_factor(data->ticks_layer, ticks_layer_get_transition_factor(data->ticks_layer) + 0.1f);
    compass_layer_update_callback(data->needle_layer);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    CompassWindowData *data = context;
    needle_layer_set_angle(data->needle_layer, needle_layer_get_angle(data->needle_layer) + TRIG_MAX_ANGLE / 5);
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

// TODO: logic should be controlled by window, NOT an invisible layer...
// also, get rid of this global here

static CompassWindowData* single_compass_data;
static void compass_layer_update_callback(NeedleLayer * layer) {
    int32_t angle = needle_layer_get_presentation_angle(layer);
    ticks_layer_set_angle(single_compass_data->ticks_layer, angle);

    const float blend_factor = ticks_layer_get_transition_factor(single_compass_data->ticks_layer);

    static char angle_text[] = "123°";
    int32_t normalized_angle = ((int)(angle * 360 / TRIG_MAX_ANGLE) % 360 + 360) % 360;
    snprintf(angle_text, sizeof(angle_text), "%d°", (int)normalized_angle);
    text_layer_set_text(single_compass_data->angle_layer, angle_text);
    GRect r = rect_blend(&single_compass_data->angle_layer_rect_rose, &single_compass_data->angle_layer_rect_band, blend_factor);
    layer_set_frame(text_layer_get_layer(single_compass_data->angle_layer), r);
    // workound!
    // TODO: file a bug for tintin applib
    layer_set_bounds(text_layer_get_layer(single_compass_data->angle_layer), (GRect){.size=r.size});
    text_layer_set_text_alignment(single_compass_data->angle_layer, GTextAlignmentRight);

    static char *direction_texts[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    int32_t direction_index = ((normalized_angle + 23) / (360 / ARRAY_LENGTH(direction_texts))) % ARRAY_LENGTH(direction_texts);
//    int32_t direction_index = 1;
    text_layer_set_text(single_compass_data->direction_layer, direction_texts[direction_index]);
    layer_set_frame(text_layer_get_layer(single_compass_data->direction_layer), rect_blend(&single_compass_data->direction_layer_rect_rose, &single_compass_data->direction_layer_rect_band, blend_factor));

    layer_set_frame(inverter_layer_get_layer(single_compass_data->pointer_layer), rect_blend(&single_compass_data->pointer_layer_rect_rose, &single_compass_data->pointer_layer_rect_band, blend_factor));
}

static void compass_window_load(Window *window) {
    CompassWindowData *data = window_get_user_data(window);
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    const int16_t text_height_rose = 24;
    const int16_t text_height_band = 55;
    const GFont text_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

    const int16_t direction_layer_width = 40;
    const int16_t direction_layer_margin_band = 40;
    data->direction_layer_rect_rose = (GRect) { .origin = { bounds.size.w - direction_layer_width, (int16_t)(bounds.size.h- text_height_rose)}, .size = { direction_layer_width, text_height_rose} };
    data->direction_layer_rect_band = (GRect) { .origin = { bounds.size.w - direction_layer_width-direction_layer_margin_band, (int16_t)(bounds.size.h- text_height_band)}, .size = { direction_layer_width, text_height_rose} };
    data->direction_layer = text_layer_create(data->direction_layer_rect_rose);
    text_layer_set_text_alignment(data->direction_layer, GTextAlignmentCenter);
    text_layer_set_font(data->direction_layer, text_font);
    layer_add_child(window_layer, text_layer_get_layer(data->direction_layer));

    const int16_t angle_layer_width_rose = 40;
    const int16_t angle_layer_width_band = 65;

    data->angle_layer_rect_rose = (GRect) { .origin = { 0, (int16_t)(bounds.size.h- text_height_rose)}, .size = { angle_layer_width_rose, text_height_rose} };
    data->angle_layer_rect_band = (GRect) { .origin = { 0, (int16_t)(bounds.size.h- text_height_band)}, .size = { angle_layer_width_band, text_height_band} };
    data->angle_layer = text_layer_create(data->angle_layer_rect_rose);
    text_layer_set_text_alignment(data->angle_layer, GTextAlignmentRight);
    text_layer_set_font(data->angle_layer, text_font);
    layer_add_child(window_layer, text_layer_get_layer(data->angle_layer));


    GRect roseRect = ((GRect){.origin={0, 8}, .size={bounds.size.w, (int16_t)(bounds.size.h-15)}});

    data->ticks_layer = ticks_layer_create(roseRect);
    layer_add_child(window_layer, ticks_layer_get_layer(data->ticks_layer));

    roseRect.origin.x += 10;
    roseRect.origin.y += 10;
    roseRect.size.h -= 20;
    roseRect.size.w -= 20;
    data->needle_layer = needle_layer_create(roseRect);
    needle_layer_set_update_callback(data->needle_layer, compass_layer_update_callback);
    layer_set_hidden(needle_layer_get_layer(data->needle_layer), true);
    layer_add_child(window_layer, needle_layer_get_layer(data->needle_layer));

    InverterLayer *inverter_layer = inverter_layer_create(bounds);
    layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));

    data->pointer_layer_rect_rose = (GRect){{71,0}, {3,20}};
    data->pointer_layer_rect_band = (GRect){{71,18}, {3,40}};
    data->pointer_layer = inverter_layer_create(data->pointer_layer_rect_rose);
    layer_add_child(window_layer, inverter_layer_get_layer(data->pointer_layer));

    needle_layer_set_angle(data->needle_layer, 45 * TRIG_MAX_ANGLE / 360);

}

static void compass_window_unload(Window *window) {
    // TODO: free memory
}

CompassWindow *compass_window_create() {
    Window *window = window_create();
    CompassWindowData *data = malloc(sizeof(CompassWindowData));

    window_set_user_data(window, data);

    window_set_window_handlers(window, (WindowHandlers) {
            .load = compass_window_load,
            .unload = compass_window_unload,
    });
    window_set_click_config_provider_with_context(window, click_config_provider, data);
    single_compass_data = data;

    return (CompassWindow *) window;
}

void compass_window_destroy(CompassWindow *window) {
    free(window_get_user_data((Window *)window));
    window_destroy((Window *)window);
}