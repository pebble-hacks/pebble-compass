#include "compass_calibration_window.h"

#define CALIBRATION_NUM_SEGMENTS 80

typedef struct {
        GPoint inner;
        GPoint mid;
        GPoint outer;
} CompassCalibrationWindowHelperPoint;

typedef struct {
    Layer *indicator_layer;
    uint8_t segment_value[CALIBRATION_NUM_SEGMENTS];
    int32_t current_angle;
    CompassCalibrationWindowHelperPoint *helperPoints;
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

void compass_calibration_window_merge_value(CompassCalibrationWindow *window, int32_t angle, uint8_t intensity) {
    const Window *w = (Window*) window;
    CompassCalibrationWindowData *data = window_get_user_data(w);

    int segment = (angle * CALIBRATION_NUM_SEGMENTS / TRIG_MAX_ANGLE) % CALIBRATION_NUM_SEGMENTS;
    if(data->segment_value[segment] < intensity) {
        data->segment_value[segment] = intensity;
        layer_mark_dirty(window_get_root_layer(w));
    }
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

    CompassCalibrationWindowHelperPoint *points = data->helperPoints;

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
        const bool segment_visited = segment_value > CALIBRATION_THRESHOLD_VISITED;
        const bool next_segment_visited = next_segment_value > CALIBRATION_THRESHOLD_VISITED;
        const bool segment_filled = segment_value > CALIBRATION_THRESHOLD_FILLED;
        const bool segment_mid = segment_value > CALIBRATION_THRESHOLD_MID;
    
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

static void window_load(Window *window) {
    CompassCalibrationWindowData *data = window_get_user_data(window);
    struct Layer *window_layer = window_get_root_layer(window);

    GRect frame = layer_get_bounds(window_layer);
    frame = grect_crop(frame, 0);
    data->indicator_layer = layer_create_with_data(frame, sizeof(CompassCalibrationWindowDataPtr));
    data->current_angle = 20 * TRIG_MAX_ANGLE / 360;
    data->helperPoints = malloc(sizeof(CompassCalibrationWindowHelperPoint) * CALIBRATION_NUM_SEGMENTS);
    *(CompassCalibrationWindowDataPtr*)layer_get_data(data->indicator_layer) = data;
    layer_set_update_proc(data->indicator_layer, draw_indicator);
    layer_add_child(window_layer, data->indicator_layer);
}

static void window_unload(Window *window) {
    CompassCalibrationWindowData *data = window_get_user_data(window);
    layer_destroy(data->indicator_layer);
    free(data->helperPoints);
    memset(data, 0, sizeof(CompassCalibrationWindowData));
}

CompassCalibrationWindow *compass_calibration_window_create() {
    Window *window = window_create();
    window_set_background_color(window, CalibrationWindowBackgroundColor);

    CompassCalibrationWindowData *data = malloc(sizeof(CompassCalibrationWindowData));
    memset(data, 0, sizeof(CompassCalibrationWindowData));
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

