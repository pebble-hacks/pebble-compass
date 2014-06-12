#include "needle_layer.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

typedef struct {
    GPathInfo points;
    int32_t target_angle;
    int32_t angular_velocity;
    int32_t presentation_angle;
    float friction;
    float attraction;
    AppTimer *timer;
    NeedleLayerCallback update_callback;
} NeedleLayerData;

Layer *needle_layer_get_layer(NeedleLayer *needleLayer) {
    return (Layer *) needleLayer;
}

static NeedleLayerData *layer_get_needle_data(Layer *layer) {
    return layer_get_data(layer);
}

static NeedleLayerData *needle_layer_get_needle_data(NeedleLayer *layer) {
    return layer_get_needle_data((Layer *) layer);
}


void neddle_layer_update_proc(struct Layer *layer, GContext *ctx) {

    const GRect r = layer_get_bounds(layer);
    const GPoint center = grect_center_point(&r);
    const uint16_t radius = (uint16_t) (MIN(r.size.w, r.size.h) / 2);
    graphics_context_set_fill_color(ctx, GColorWhite);

    const int16_t needle_width = (int16_t) (radius/4);
    const int16_t needle_length = radius;
    const GPathInfo points = {
        3,
        (GPoint[]) {
                {-needle_width, 0},
                {needle_width, 0},
                {0, -needle_length}
        }
    };

    GPath *path = gpath_create(&points);

    NeedleLayerData *data = layer_get_needle_data(layer);
    int32_t angle = data->presentation_angle;
    gpath_rotate_to(path, angle);
    gpath_move_to(path, center);
    graphics_context_set_fill_color(ctx, GColorBlack);
    gpath_draw_filled(ctx, path);

    gpath_rotate_to(path, angle + TRIG_MAX_ANGLE/2);
    graphics_context_set_fill_color(ctx, GColorWhite);
    gpath_draw_filled(ctx, path);
    gpath_draw_outline(ctx, path);

    gpath_destroy(path);
}

void needle_layer_update_state(NeedleLayer *layer);

void needle_layer_data_schedule_update(NeedleLayer *layer) {
    NeedleLayerData *data = needle_layer_get_needle_data(layer);
    if(!data->timer) {
        data->timer = app_timer_register(1000 / 30, (AppTimerCallback) needle_layer_update_state, layer);
    }
}

void needle_layer_update_state(NeedleLayer *layer) {
    NeedleLayerData *data = needle_layer_get_needle_data(layer);
    data->presentation_angle = data->presentation_angle + data->angular_velocity;
    int32_t attraction = (int32_t)((data->target_angle - data->presentation_angle) * data->attraction);
    data->angular_velocity += attraction;
    data->angular_velocity = (int32_t) (data->angular_velocity * data->friction);

    layer_mark_dirty(needle_layer_get_layer(layer));
    if(data->update_callback) {
        data->update_callback((NeedleLayer *)layer);
    }

    data->timer = NULL;
    if((int32_t)(attraction*data->friction) != 0 || data->angular_velocity != 0) {
        needle_layer_data_schedule_update(layer);
    } else {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "NeedleLayer rested");
    }
}

NeedleLayer *needle_layer_create(GRect frame) {
    Layer *result = layer_create_with_data(frame, sizeof(NeedleLayerData));
    memset(layer_get_data(result), 0, sizeof(NeedleLayerData)); // is this really needed?
    layer_get_needle_data(result)->friction = 0.9;
    layer_get_needle_data(result)->attraction = 0.1;

    layer_set_update_proc(result, neddle_layer_update_proc);
    return (NeedleLayer *) result;
}

void needle_layer_destroy(NeedleLayer *layer) {
    layer_destroy(needle_layer_get_layer(layer));
}

void needle_layer_set_angle(NeedleLayer *layer, int32_t angle) {
    NeedleLayerData *data = needle_layer_get_needle_data(layer);
    data->target_angle = angle;
    needle_layer_data_schedule_update(layer);
    // needle_layer_update_state will eventually call layer_mark_dirty
}

int32_t needle_layer_get_angle(NeedleLayer *layer) {
    return needle_layer_get_needle_data(layer)->target_angle;
}

void needle_layer_set_friction(NeedleLayer *layer, float friction) {
    needle_layer_get_needle_data(layer)->friction = friction;
}

float needle_layer_get_friction(NeedleLayer *layer) {
    return needle_layer_get_needle_data(layer)->friction;
}

void needle_layer_set_attraction(NeedleLayer *layer, float attraction) {
    needle_layer_get_needle_data(layer)->friction = attraction;
}

float needle_layer_get_attraction(NeedleLayer *layer) {
    return needle_layer_get_needle_data(layer)->attraction;
}

void needle_layer_set_update_callback(NeedleLayer *layer, NeedleLayerCallback cb) {
    needle_layer_get_needle_data(layer)->update_callback = cb;
}

int32_t needle_layer_get_presentation_angle(NeedleLayer *layer) {
    return needle_layer_get_needle_data(layer)->presentation_angle;
}

