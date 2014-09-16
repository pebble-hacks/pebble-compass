#include "inverted_cross_hair_layer.h"

typedef struct {
    InverterLayer *horiz_line;
    InverterLayer *top_vert_line;
    InverterLayer *bottom_vert_line;
} InvertedCrossHairLayerData;

Layer *inverted_cross_hair_layer_get_layer(InvertedCrossHairLayer* layer) {
    return (Layer *) layer;
}

static void inverted_cross_hair_update_proc(Layer *layer, GContext *ctx) {
    // I wish we had an inverted compositing Op for bitmaps...
    // we wouldn't need this layer at all...
    // see comment on SDKRMAP-34
    // alternatively, having officially supported access to the framebuffer
    // one could do this simpler, too PBL-8249

    InvertedCrossHairLayerData *data = layer_get_data(layer);
    GRect b = layer_get_bounds(layer);

    // changing layout in update proc... hmm...
    int16_t y = (int16_t)(b.size.h/2);
    int16_t x = (int16_t)(b.size.w/2);
    layer_set_frame(inverter_layer_get_layer(data->horiz_line), (GRect){
            .origin = {0, y}, .size = {b.size.w, 1},
    });
    layer_set_frame(inverter_layer_get_layer(data->top_vert_line), (GRect){
            .origin = {x, 0}, .size = {1, y},
    });
    layer_set_frame(inverter_layer_get_layer(data->bottom_vert_line), (GRect){
            .origin = {x, (int16_t) (y+1)}, .size = {1, y},
    });
}

InvertedCrossHairLayer *inverted_cross_hair_layer_create(GRect frame) {
    Layer *result = layer_create_with_data(frame, sizeof(InvertedCrossHairLayerData));

    InvertedCrossHairLayerData *data = layer_get_data(result);
    data->horiz_line = inverter_layer_create(GRectZero);
    layer_add_child(result, inverter_layer_get_layer(data->horiz_line));
    data->top_vert_line = inverter_layer_create(GRectZero);
    layer_add_child(result, inverter_layer_get_layer(data->top_vert_line));
    data->bottom_vert_line = inverter_layer_create(GRectZero);
    layer_add_child(result, inverter_layer_get_layer(data->bottom_vert_line));

    layer_set_update_proc(result, inverted_cross_hair_update_proc);
    return (InvertedCrossHairLayer *)result;
}

void inverted_cross_hair_layer_destroy(InvertedCrossHairLayer* layer) {
    InvertedCrossHairLayerData *data = layer_get_data((Layer *) layer);
    inverter_layer_destroy(data->horiz_line);
    inverter_layer_destroy(data->top_vert_line);
    inverter_layer_destroy(data->bottom_vert_line);
    layer_destroy((Layer *) layer);
}
