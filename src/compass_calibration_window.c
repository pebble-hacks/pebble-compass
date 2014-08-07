#include "compass_calibration_window.h"

#define CALIBRATION_NUM_SEGMENTS 80

typedef struct {
        GPoint inner;
        GPoint mid;
        GPoint outer;
} CompassCalibrationWindowHelperPoint;

typedef struct {
    Layer *indicator_layer;
    TextLayer *headline_layer;
    TextLayer *description_layer;
    uint8_t segment_value[CALIBRATION_NUM_SEGMENTS];
    int32_t current_angle;
    CompassCalibrationWindowHelperPoint *helper_points;
} CompassCalibrationWindowData;

typedef CompassCalibrationWindowData* CompassCalibrationWindowDataPtr;

static const int CALIBRATION_THRESHOLD_VISITED = 10;
static const int CALIBRATION_THRESHOLD_MID = 100;
static const int CALIBRATION_THRESHOLD_FILLED = 150;

static const enum GColor CalibrationWindowForegroundColor = GColorWhite;
static const enum GColor CalibrationWindowBackgroundColor = GColorBlack;

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

void compass_calibration_window_set_current_angle(CompassCalibrationWindow *window, int32_t angle) {
    const Window *w = (Window*) window;
    CompassCalibrationWindowData *data = window_get_user_data(w);

    data->current_angle = angle;
    layer_mark_dirty(window_get_root_layer(w));
}

void update_description_if_neeed(CompassCalibrationWindowData *data) {// update instructions of necessary
    bool all_fully_filled = true;
    for(int i=0; i<CALIBRATION_NUM_SEGMENTS; i++) {
       if(data->segment_value[i] < CALIBRATION_THRESHOLD_VISITED) return;
       if(data->segment_value[i] < CALIBRATION_THRESHOLD_FILLED) {
           all_fully_filled = false;
       }
    }

    if(all_fully_filled) {
        text_layer_set_text(data->headline_layer, "More!");
        text_layer_set_text(data->description_layer, "Try a fancy dance?");
    } else {
        // all segments have been visited, encourage user to tilt more!
        text_layer_set_text(data->headline_layer, "Tilt more!");
        text_layer_set_text(data->description_layer, "Fill the ring\ncompletely");
    }
}

void compass_calibration_window_merge_value(CompassCalibrationWindow *window, int32_t angle, uint8_t intensity) {
    const Window *w = (Window*) window;
    CompassCalibrationWindowData *data = window_get_user_data(w);

    int segment = (angle * CALIBRATION_NUM_SEGMENTS / TRIG_MAX_ANGLE) % CALIBRATION_NUM_SEGMENTS;
    if(data->segment_value[segment] < intensity) {
        data->segment_value[segment] = intensity;
        layer_mark_dirty(window_get_root_layer(w));
    }
    update_description_if_neeed(data);
}

Window *compass_calibration_window_get_window(CompassCalibrationWindow *window) {
    return (Window *)window;
}

static GPoint point_at_angle(GPoint center, int angle, int16_t radius) {
    return GPoint(center.x + (int16_t)(sin_lookup(angle)*radius/ TRIG_MAX_RATIO), center.y + (int16_t)(cos_lookup(angle)*radius/ TRIG_MAX_RATIO));
}

static void draw_indicator(Layer *layer, GContext* ctx) {
    CompassCalibrationWindowData *data = *(CompassCalibrationWindowDataPtr*)layer_get_data(layer);

    const GRect rect = layer_get_bounds(layer);
    const uint16_t ring_thickness = 10;
    const uint16_t outer_radius = (uint16_t)(MIN(rect.size.h, rect.size.w) / 2);
    const uint16_t inner_radius = outer_radius - ring_thickness;
    const uint16_t mid_radius = (uint16_t const)((outer_radius + inner_radius)/2);

    const GPoint c = grect_center_point(&rect);

    CompassCalibrationWindowHelperPoint *points = data->helper_points;

    for(int s = 0; s<CALIBRATION_NUM_SEGMENTS; s++) {
        int angle = (int) ((s-0.5)* TRIG_MAX_ANGLE / CALIBRATION_NUM_SEGMENTS);
        points[s].inner = point_at_angle(c, angle, inner_radius);
        points[s].mid = point_at_angle(c, angle, mid_radius);
        points[s].outer = point_at_angle(c, angle, outer_radius);
    }

    GPathInfo path_info = (GPathInfo){
            .num_points = 4,
            .points = (GPoint[4]){},
    };
    GPath *path = gpath_create(&path_info);

    graphics_context_set_stroke_color(ctx, CalibrationWindowForegroundColor);
    graphics_context_set_fill_color(ctx, CalibrationWindowForegroundColor);

    for(int s = 0; s<CALIBRATION_NUM_SEGMENTS; s++) {
        const int s2 = (s+1)%CALIBRATION_NUM_SEGMENTS;
        const uint8_t segment_value = data->segment_value[s];
        const uint8_t next_segment_value = data->segment_value[s2];
        const bool segment_visited = segment_value >= CALIBRATION_THRESHOLD_VISITED;
        const bool next_segment_visited = next_segment_value >= CALIBRATION_THRESHOLD_VISITED;
        const bool segment_filled = segment_value >= CALIBRATION_THRESHOLD_FILLED;
        const bool segment_mid = segment_value >= CALIBRATION_THRESHOLD_MID;
    
        graphics_draw_line(ctx, points[s].inner, points[s2].inner);
        if(segment_visited){
            graphics_draw_line(ctx, points[s].outer, points[s2].outer);
        }
        if(segment_visited || next_segment_visited) {
            graphics_draw_line(ctx, points[s2].inner, points[s2].outer);
        }
        if(segment_filled || segment_mid) {
            path_info.points[0] = points[s].inner;
            path_info.points[1] = segment_filled ? points[s].outer : points[s].mid;
            path_info.points[2] = segment_filled ? points[s2].outer : points[s2].mid;
            path_info.points[3] = points[s2].inner;
            gpath_draw_filled(ctx, path);
        }
    }

    // draw current angle
    graphics_fill_circle(ctx, point_at_angle(c, data->current_angle, (int16_t)(inner_radius - 6)), 4);

    gpath_destroy(path);

}

TextLayer * create_and_add_text_layer(Layer *window_layer, GRect *all_text_rect, GAlign alignment, char *font_key, char *text) {
    const GFont font = fonts_get_system_font(font_key);
    const GTextAlignment text_alignment = GTextAlignmentCenter;

    GRect label_rect = (GRect){.size = graphics_text_layout_get_content_size(text, font, *all_text_rect, GTextOverflowModeWordWrap, text_alignment)};
    label_rect.size.h += 5; // everything below baseline...
    grect_align(&label_rect, all_text_rect, alignment, true);

    TextLayer *layer = text_layer_create(label_rect);
    text_layer_set_text(layer, text);
    text_layer_set_background_color(layer, GColorClear);
    text_layer_set_text_color(layer, CalibrationWindowForegroundColor);
    text_layer_set_font(layer, font);
    text_layer_set_text_alignment(layer, text_alignment);
    layer_add_child(window_layer, text_layer_get_layer(layer));
    return layer;
}

static void window_load(Window *window) {
    CompassCalibrationWindowData *data = window_get_user_data(window);
    struct Layer *window_layer = window_get_root_layer(window);

    GRect frame = layer_get_bounds(window_layer);
    frame = grect_crop(frame, 0);
    data->indicator_layer = layer_create_with_data(frame, sizeof(CompassCalibrationWindowDataPtr));
    data->current_angle = 20 * TRIG_MAX_ANGLE / 360;
    data->helper_points = malloc(sizeof(CompassCalibrationWindowHelperPoint) * CALIBRATION_NUM_SEGMENTS);
    *(CompassCalibrationWindowDataPtr*)layer_get_data(data->indicator_layer) = data;
    layer_set_update_proc(data->indicator_layer, draw_indicator);
    layer_add_child(window_layer, data->indicator_layer);

    GRect all_text_rect = (GRect){.size = GSize(100, 60)};
    grect_align(&all_text_rect, &frame, GAlignCenter, true);
    data->headline_layer = create_and_add_text_layer(window_layer, &all_text_rect, GAlignTop, FONT_KEY_GOTHIC_18_BOLD, "Calibration");
    data->description_layer = create_and_add_text_layer(window_layer, &all_text_rect, GAlignBottom, FONT_KEY_GOTHIC_18, "Tilt Pebble to\nroll ball around");
}

static void window_unload(Window *window) {
    CompassCalibrationWindowData *data = window_get_user_data(window);
    layer_destroy(data->indicator_layer);
    text_layer_destroy(data->headline_layer);
    text_layer_destroy(data->description_layer);

    free(data->helper_points);
    memset(data, 0, sizeof(CompassCalibrationWindowData));
}

static void fill_fake_data(CompassCalibrationWindowData *data) {
    int b = (int) (CALIBRATION_NUM_SEGMENTS * 0.6);
    data->segment_value[b+0] = CALIBRATION_THRESHOLD_VISITED;
    data->segment_value[b+1] = CALIBRATION_THRESHOLD_MID;
    data->segment_value[b+2] = CALIBRATION_THRESHOLD_FILLED;
}

static void do_nothing_click_handler(ClickRecognizerRef recognizer, void *context) {
    // prevents window from being dismissed by user
}

static void click_config_provider(void *context) {
    window_single_click_subscribe(BUTTON_ID_BACK, do_nothing_click_handler);
}

CompassCalibrationWindow *compass_calibration_window_create() {
    Window *window = window_create();
    window_set_background_color(window, CalibrationWindowBackgroundColor);
    window_set_click_config_provider(window, click_config_provider);

    CompassCalibrationWindowData *data = malloc(sizeof(CompassCalibrationWindowData));
    memset(data, 0, sizeof(CompassCalibrationWindowData));
    fill_fake_data(data);

    window_set_user_data(window, data);

    window_set_window_handlers(window, (WindowHandlers){
            .load = window_load,
            .unload = window_unload,
    });

    return (CompassCalibrationWindow *) window;
}

void compass_calibration_window_destroy(CompassCalibrationWindow *window) {
    if(!window)return;

    CompassCalibrationWindowData *data = window_get_user_data((Window *)window);
    free(data);

    window_destroy((Window *)window);
}

