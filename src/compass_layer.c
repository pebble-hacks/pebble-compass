#include "compass_layer.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

typedef struct {
    GPathInfo points;
    int32_t angle;
    int32_t presentation_angle;
    AppTimer *timer;
} CompassLayerData;

Layer *compass_layer_get_layer(CompassLayer *compassLayer) {
    return (Layer *) compassLayer;
}

CompassLayerData *layer_get_compass_data(Layer *layer) {
    return layer_get_data(layer);
}

CompassLayerData *compass_layer_get_compass_data(CompassLayer *layer) {
    return layer_get_compass_data((Layer*)layer);
}


void compass_layer_update_proc(struct Layer *layer, GContext *ctx) {

    const GRect r = layer_get_bounds(layer);
    const GPoint center = grect_center_point(&r);
    const uint16_t radius = (uint16_t) (MIN(r.size.w, r.size.h) / 2);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, center, radius);
    graphics_draw_circle(ctx, center, radius);

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

    int32_t angle = layer_get_compass_data(layer)->presentation_angle;
    gpath_rotate_to(path, angle);
    gpath_move_to(path, center);
    graphics_context_set_fill_color(ctx, GColorBlack);
    gpath_draw_filled(ctx, path);
    gpath_draw_outline(ctx, path);

    gpath_rotate_to(path, angle + TRIG_MAX_ANGLE/2);
    gpath_draw_outline(ctx, path);

    gpath_destroy(path);
}

void compass_layer_update_state(CompassLayer* layer);

void compass_layer_data_schedule_update(CompassLayer *layer) {
    CompassLayerData *data = compass_layer_get_compass_data(layer);
    data->timer = app_timer_register(1000 / 30, (AppTimerCallback)compass_layer_update_state, layer);
}

void compass_layer_update_state(CompassLayer* layer) {
    CompassLayerData *data = compass_layer_get_compass_data(layer);
    data->presentation_angle = (int32_t)(((int64_t)data->presentation_angle + (int64_t)data->angle) / 2);
    layer_mark_dirty(compass_layer_get_layer(layer));

    if(data->presentation_angle != data->angle) {
        compass_layer_data_schedule_update(layer);
    } else {
        data->timer = NULL;
    }
}

CompassLayer *compass_layer_create(GRect frame) {
    Layer *result = layer_create_with_data(frame, sizeof(CompassLayerData));
    memset(layer_get_data(result), 0, sizeof(CompassLayerData)); // is this really needed?
    layer_set_update_proc(result, compass_layer_update_proc);
    return (CompassLayer *) result;
}

void compass_layer_destroy(CompassLayer *layer) {
    layer_destroy(compass_layer_get_layer(layer));
}

void compass_layer_set_angle(CompassLayer *layer, int32_t angle) {
    CompassLayerData *data = compass_layer_get_compass_data(layer);
    data->angle = angle;
    if(!data->timer) {
        compass_layer_data_schedule_update(layer);

    }
    // compass_layer_update_state will eventually call layer_mark_dirty
}

int32_t compass_layer_get_angle(CompassLayer *layer) {
    return compass_layer_get_compass_data(layer)->angle;
}
