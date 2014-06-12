#include <pebble.h>

typedef struct NeedleLayer NeedleLayer;

Layer *needle_layer_get_layer(NeedleLayer *needleLayer);
typedef void (*NeedleLayerCallback)(NeedleLayer *animation);

NeedleLayer *needle_layer_create(GRect frame);
void needle_layer_destroy(NeedleLayer *layer);

int32_t needle_layer_get_angle(NeedleLayer *layer);
void needle_layer_set_angle(NeedleLayer *layer, int32_t angle);

int32_t needle_layer_get_presentation_angle(NeedleLayer *layer);

void needle_layer_set_friction(NeedleLayer *layer, float friction);
float needle_layer_get_friction(NeedleLayer *layer);

void needle_layer_set_attraction(NeedleLayer *layer, float attraction);
float needle_layer_get_attraction(NeedleLayer *layer);

void needle_layer_set_update_callback(NeedleLayer *layer, NeedleLayerCallback cb);