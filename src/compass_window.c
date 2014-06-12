#include "compass_window.h"
#include "ticks_layer.h"
#include "data_provider.h"

typedef struct {
    GRect direction_layer_rect_rose;
    GRect direction_layer_rect_band;
    TextLayer *direction_layer;

    GRect angle_layer_rect_rose;
    GRect angle_layer_rect_band;
    TextLayer *angle_layer;

    TicksLayer *ticks_layer;

    GRect pointer_layer_rect_rose;
    GRect pointer_layer_rect_band;
    InverterLayer *pointer_layer;

    DataProvider* data_provider;

    bool shows_band;
    Animation *transition_animation;
    float transition_animation_start_value;
} CompassWindowData;

Window *compass_window_get_window(CompassWindow *window) {
    return (Window *)window;
}

static void compass_layer_update_layout(CompassWindowData *data);
static void set_shows_band(CompassWindowData *data, bool shows_band);

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
//    needle_layer_set_angle(needle_layer, 0);
//    ticks_layer_set_transition_factor(ticks_layer, ticks_layer_get_transition_factor(ticks_layer) - 0.1f);
//    compass_layer_update_layout(needle_layer);
    CompassWindowData *data = context;
    set_shows_band(data, !data->shows_band);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    //needle_layer_set_angle(needle_layer, needle_layer_get_angle(needle_layer) - TRIG_MAX_ANGLE / 10);
    CompassWindowData *data = context;
    ticks_layer_set_transition_factor(data->ticks_layer, ticks_layer_get_transition_factor(data->ticks_layer) + 0.1f);
    compass_layer_update_layout(data);
}

static void set_transition_factor(CompassWindowData *data, float factor) {
    ticks_layer_set_transition_factor(data->ticks_layer, factor);
    compass_layer_update_layout(data);
}

static void compass_window_update_transition_factor(struct Animation *animation, const uint32_t time_normalized) {
    CompassWindowData *data = animation->context;
    float f = (float)time_normalized / ANIMATION_NORMALIZED_MAX;
    float target = data->shows_band ? 1 : 0;

    set_transition_factor(data, target * f + (1-f) * data->transition_animation_start_value);
}

static AnimationImplementation transition_animation = {
    .update = compass_window_update_transition_factor,
};

static void set_shows_band(CompassWindowData *data, bool shows_band) {
    if(data->shows_band == shows_band) return;

    // TODO: refactor to make this a property animation
    data->shows_band = shows_band;
    if(!data->transition_animation) {
        data->transition_animation = animation_create();
        data->transition_animation->duration_ms = 200;
        data->transition_animation->context = data;
        data->transition_animation->implementation = &transition_animation;
    } else {
        animation_unschedule(data->transition_animation);
    }

    data->transition_animation_start_value = ticks_layer_get_transition_factor(data->ticks_layer);
    animation_schedule(data->transition_animation);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
    CompassWindowData *data = context;
    data_provider_set_target_angle(data->data_provider, data_provider_get_target_angle(data->data_provider) + TRIG_MAX_ANGLE / 5);
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

static void compass_layer_update_layout(CompassWindowData *data) {
    int32_t angle = data_provider_get_presentation_angle(data->data_provider);
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

    InverterLayer *inverter_layer = inverter_layer_create(bounds);
    layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));

    data->pointer_layer_rect_rose = (GRect){{71,0}, {3,20}};
    data->pointer_layer_rect_band = (GRect){{71,18}, {3,40}};
    data->pointer_layer = inverter_layer_create(data->pointer_layer_rect_rose);
    layer_add_child(window_layer, inverter_layer_get_layer(data->pointer_layer));

    data_provider_set_target_angle(data->data_provider, 45 * TRIG_MAX_ANGLE / 360);
}

static void compass_window_unload(Window *window) {
    CompassWindowData *data = window_get_user_data(window);
    ticks_layer_destroy(data->ticks_layer);
    text_layer_destroy(data->angle_layer);
    text_layer_destroy(data->direction_layer);
    inverter_layer_destroy(data->pointer_layer);
}

void handle_data_provider_update(DataProvider *provider, void* user_data) {
    CompassWindowData *data = user_data;
    compass_layer_update_layout(data);
}

CompassWindow *compass_window_create() {
    Window *window = window_create();
    CompassWindowData *data = malloc(sizeof(CompassWindowData));

    data->data_provider = data_provider_create(data, (DataProviderHandlers) {
        .presented_angle_changed = handle_data_provider_update,
    });

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
    CompassWindowData *data = window_get_user_data((Window *)window);
    data_provider_destroy(data->data_provider);
    free(data);
    window_destroy((Window *)window);
}

