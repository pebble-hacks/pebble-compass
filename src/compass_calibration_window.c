#include "compass_calibration_window.h"

#define CALIBRATION_NUM_SEGMENTS 80

typedef struct {
    // actual ring, custom update_proc
    Layer *indicator_layer;

    TextLayer *headline_layer;
    TextLayer *description_layer;

    bool influenced_by_interferences;

    // internal state
    uint8_t segment_value[CALIBRATION_NUM_SEGMENTS];
    int32_t current_angle;
} CompassCalibrationWindowData;

typedef CompassCalibrationWindowData* CompassCalibrationWindowDataPtr;

static const int CALIBRATION_THRESHOLD_VISITED = 10;
static const int CALIBRATION_THRESHOLD_MID = 100;
static const int CALIBRATION_THRESHOLD_FILLED = 150;

static const int CALIBRATION_WINDOW_RING_MARGIN = 0;

static const enum GColor CalibrationWindowForegroundColor = GColorWhite;
static const enum GColor CalibrationWindowBackgroundColor = GColorBlack;

static char *const INITIAL_HEADLINE = "Calibration";
static char *const INITIAL_DESCRIPTION = "Tilt Pebble to\nroll ball around";
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

void compass_calibration_window_set_current_angle(CompassCalibrationWindow *window, int32_t angle) {
    const Window *w = (Window*) window;
    CompassCalibrationWindowData *data = window_get_user_data(w);

    data->current_angle = angle;
    layer_mark_dirty(window_get_root_layer(w));
}

void update_description_if_needed(CompassCalibrationWindowData *data) {// update instructions of necessary
    if(!data->headline_layer) return;

    char *headline = NULL;
    char *description = NULL;

    if(data->influenced_by_interferences) {
        headline = "Interferences";
        description = "Please unplug\nthe charger.";
    } else {
        bool all_fully_filled = true;
        for (int i = 0; i < CALIBRATION_NUM_SEGMENTS; i++) {
            if (data->segment_value[i] < CALIBRATION_THRESHOLD_VISITED) {
                headline = INITIAL_HEADLINE;
                description = INITIAL_DESCRIPTION;
                break;
            }
            if (data->segment_value[i] < CALIBRATION_THRESHOLD_FILLED) {
                all_fully_filled = false;
            }
        }

        if (headline == NULL) {
            if (all_fully_filled) {
                headline = "More!";
                description = "Try a fancy\ndance?";
            } else {
                // all segments have been visited, encourage user to tilt more!
                headline = "Tilt more!";
                description = "Fill the ring\ncompletely";
            }
        }
    }

    // try to minimize updates
    // should Pebble's applib do this itself?
    if(strcmp(headline, text_layer_get_text(data->headline_layer))) {
        text_layer_set_text(data->headline_layer, headline);
    }
    if(strcmp(description, text_layer_get_text(data->description_layer))) {
        text_layer_set_text(data->description_layer, description);
    }
}

void compass_calibration_window_merge_value(CompassCalibrationWindow *window, int32_t angle, uint8_t intensity) {
    const Window *w = (Window*) window;
    CompassCalibrationWindowData *data = window_get_user_data(w);

    if(data->influenced_by_interferences) {
        return;
    }

    int segment = (angle * CALIBRATION_NUM_SEGMENTS / TRIG_MAX_ANGLE) % CALIBRATION_NUM_SEGMENTS;
    if(data->segment_value[segment] < intensity) {
        data->segment_value[segment] = intensity;
        layer_mark_dirty(window_get_root_layer(w));
    }
    update_description_if_needed(data);
}

void compass_calibration_window_apply_accel_data(CompassCalibrationWindow *calibration_window, AccelData accel_data) {
    int32_t angle = atan2_lookup(accel_data.y, accel_data.x) + 90 * TRIG_MAX_ANGLE / 360;
    int intensity = abs(accel_data.z / 5);
    intensity = intensity>255 ? 255 : intensity;
    intensity = 255 - intensity;

    compass_calibration_window_set_current_angle(calibration_window, angle);
    compass_calibration_window_merge_value(calibration_window, angle, (uint8_t) intensity);
}

static void reset_segment_data(CompassCalibrationWindowData *data, bool fill_with_fake_data) {
    memset(&data->segment_value, 0, sizeof(data->segment_value));

    if(fill_with_fake_data) {
        int b = CALIBRATION_NUM_SEGMENTS * 6 / 10;
        data->segment_value[b+0] = CALIBRATION_THRESHOLD_VISITED;
        data->segment_value[b+1] = CALIBRATION_THRESHOLD_MID;
        data->segment_value[b+2] = CALIBRATION_THRESHOLD_FILLED;
    }
}

void compass_calibration_window_set_influenced_by_magnetic_interferences(CompassCalibrationWindow *window, bool influenced) {
    const Window *w = (Window*) window;
    CompassCalibrationWindowData *data = window_get_user_data(w);

    if(data->influenced_by_interferences == influenced) {
        return;
    }
    data->influenced_by_interferences = influenced;

    reset_segment_data(data, !influenced);
    update_description_if_needed(data);
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
    const uint16_t outer_radius = (uint16_t) (MIN(rect.size.h, rect.size.w) / 2);
    const uint16_t inner_radius = outer_radius - ring_thickness;
    const uint16_t mid_radius = (uint16_t const) ((outer_radius + inner_radius) / 2);

    const GPoint c = grect_center_point(&rect);

    graphics_context_set_stroke_color(ctx, CalibrationWindowForegroundColor);
    graphics_context_set_fill_color(ctx, CalibrationWindowForegroundColor);

    // as we don't have a graphics_fill_ring_segment() we fake a circle here
    // in reality we are painting and filling a regular polygon
    // helper points for rendering below
    typedef struct {
        GPoint inner;
        GPoint mid;
        GPoint outer;
    } CompassCalibrationWindowHelperPoint;

    CompassCalibrationWindowHelperPoint *points = malloc(sizeof(CompassCalibrationWindowHelperPoint) * CALIBRATION_NUM_SEGMENTS);
    for (int s = 0; s < CALIBRATION_NUM_SEGMENTS; s++) {
        // (s-0.5) * 360 / num_segments
        int angle = (s * TRIG_MAX_ANGLE - TRIG_MAX_ANGLE / 2) / CALIBRATION_NUM_SEGMENTS;
        points[s].inner = point_at_angle(c, angle, inner_radius);
        points[s].mid = point_at_angle(c, angle, mid_radius);
        points[s].outer = point_at_angle(c, angle, outer_radius);
    }

    // we create a gpath only once and modify it one the fly
    GPathInfo path_info = (GPathInfo) {
            .num_points = 4,
            .points = (GPoint[4]) {},
    };
    GPath *path = gpath_create(&path_info);

    // go around the ring and draw elements as needed
    for (int s = 0; s < CALIBRATION_NUM_SEGMENTS; s++) {
        const int s2 = (s + 1) % CALIBRATION_NUM_SEGMENTS;
        const uint8_t segment_value = data->segment_value[s];
        const uint8_t next_segment_value = data->segment_value[s2];
        const bool segment_visited = segment_value >= CALIBRATION_THRESHOLD_VISITED;
        const bool next_segment_visited = next_segment_value >= CALIBRATION_THRESHOLD_VISITED;
        const bool segment_filled = segment_value >= CALIBRATION_THRESHOLD_FILLED;
        const bool segment_mid = segment_value >= CALIBRATION_THRESHOLD_MID;

        graphics_draw_line(ctx, points[s].inner, points[s2].inner);
        if (segment_visited) {
            graphics_draw_line(ctx, points[s].outer, points[s2].outer);
        }
        if (segment_visited || next_segment_visited) {
            graphics_draw_line(ctx, points[s2].inner, points[s2].outer);
        }
        if (segment_filled || segment_mid) {
            path_info.points[0] = points[s].inner;
            path_info.points[1] = segment_filled ? points[s].outer : points[s].mid;
            path_info.points[2] = segment_filled ? points[s2].outer : points[s2].mid;
            // gpath_draw_filled does not support chaning .num_points after gpath has been created
            // hence, put 4th point into 3rd
            path_info.points[3] = points[s2].inner;
            gpath_draw_filled(ctx, path);
        }
    }
    gpath_destroy(path);
    free(points);

    // draw current angle
    graphics_fill_circle(ctx, point_at_angle(c, data->current_angle, (int16_t) (inner_radius - 6)), 4);

}

TextLayer * create_and_add_text_layer(Layer *window_layer, GRect *all_text_rect, GAlign alignment, char *font_key, char *text) {
    const GFont font = fonts_get_system_font(font_key);
    const GTextAlignment text_alignment = GTextAlignmentCenter;

    GRect label_rect = (GRect){.size = graphics_text_layout_get_content_size(text, font, *all_text_rect, GTextOverflowModeWordWrap, text_alignment)};
    // graphics_text_layout_get_content_size does not care about anything below baseline :(
    label_rect.size.h += 5;
    label_rect.size.w = all_text_rect->size.w;
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
    frame = grect_crop(frame, CALIBRATION_WINDOW_RING_MARGIN);
    data->indicator_layer = layer_create_with_data(frame, sizeof(CompassCalibrationWindowDataPtr));
    data->current_angle = 20 * TRIG_MAX_ANGLE / 360;
    // unfortunately, once cannot pass a pointer to your own data in layer_create_with_data
    // hence, we interpret the allocated space as pointer to pointer...
    *(CompassCalibrationWindowDataPtr*)layer_get_data(data->indicator_layer) = data;
    layer_set_update_proc(data->indicator_layer, draw_indicator);
    layer_add_child(window_layer, data->indicator_layer);

    // TODO: get rid of absolute coordinates
    // note, as there's not way to detect bounds changes of a layer, one has to do this during update
    // or here, assuming the size of a window won't change
    GRect all_text_rect = (GRect){.size = GSize(100, 60)};
    grect_align(&all_text_rect, &frame, GAlignCenter, true);
    data->headline_layer = create_and_add_text_layer(window_layer, &all_text_rect, GAlignTop, FONT_KEY_GOTHIC_18_BOLD, INITIAL_HEADLINE);
    data->description_layer = create_and_add_text_layer(window_layer, &all_text_rect, GAlignBottom, FONT_KEY_GOTHIC_18, INITIAL_DESCRIPTION);
    update_description_if_needed(data);
}

static void window_unload(Window *window) {
    CompassCalibrationWindowData *data = window_get_user_data(window);
    layer_destroy(data->indicator_layer);
    text_layer_destroy(data->headline_layer);
    text_layer_destroy(data->description_layer);
    memset(data, 0, sizeof(CompassCalibrationWindowData));
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

    reset_segment_data(data, true);

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

