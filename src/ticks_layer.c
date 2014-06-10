#include "ticks_layer.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

typedef struct {
} TicksLayerData;

Layer *ticks_layer_get_layer(TicksLayer* ticksLayer) {
    return (Layer*)ticksLayer;
}

void ticks_layer_update_proc(Layer *layer, GContext *ctx) {
    const GRect bounds = layer_get_bounds(layer);
    const GPoint center = grect_center_point(&bounds);


    const int32_t r2 = (MIN(bounds.size.w, bounds.size.h) / 2);

//    const int32_t r1 = 0;
    
    const int num_ticks = 16;
    const int half_ticks_angle = (TRIG_MAX_ANGLE * 2 / 360) / 2;

    GPathInfo points = {
            4,
            (GPoint[4]) {},
    };
    GPath *path = gpath_create(&points);

    for(int i=0; i<num_ticks; i++) {
        int32_t angle = (int32_t)(TRIG_MAX_ANGLE * i / num_ticks);
        const int32_t len = i % 2 == 0 ? 10 : 6;
        const int32_t r1 = r2 - len;

        const int32_t yy1 = -cos_lookup(angle-half_ticks_angle);
        const int32_t xx1 = sin_lookup(angle-half_ticks_angle);
        const int32_t yy2 = -cos_lookup(angle+half_ticks_angle);
        const int32_t xx2 = sin_lookup(angle+half_ticks_angle);

        points.points[0].x = (int16_t)(xx1 * r1 / TRIG_MAX_RATIO) + center.x;
        points.points[0].y = (int16_t)(yy1 * r1 / TRIG_MAX_RATIO) + center.y;
        points.points[1].x = (int16_t)(xx1 * r2 / TRIG_MAX_RATIO) + center.x;
        points.points[1].y = (int16_t)(yy1 * r2 / TRIG_MAX_RATIO) + center.y;
        points.points[2].x = (int16_t)(xx2 * r2 / TRIG_MAX_RATIO) + center.x;
        points.points[2].y = (int16_t)(yy2 * r2 / TRIG_MAX_RATIO) + center.y;
        points.points[3].x = (int16_t)(xx2 * r1 / TRIG_MAX_RATIO) + center.x;
        points.points[3].y = (int16_t)(yy2 * r1 / TRIG_MAX_RATIO) + center.y;

        gpath_draw_filled(ctx, path);
        gpath_draw_outline(ctx, path);
    }

    gpath_destroy(path);

}

TicksLayer *ticks_layer_create(GRect frame) {
    Layer *result = layer_create_with_data(frame, sizeof(TicksLayerData));

    layer_set_update_proc(result, ticks_layer_update_proc);
    return (TicksLayer *)result;
}

void ticks_layer_destroy(TicksLayer *layer) {
    layer_destroy((Layer *)layer);
}
