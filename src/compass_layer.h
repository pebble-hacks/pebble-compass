#include <pebble.h>

typedef struct CompassLayer CompassLayer;

Layer *compass_layer_get_layer(CompassLayer* compassLayer);
typedef void (*CompassLayerCallback)(CompassLayer *animation);

CompassLayer *compass_layer_create(GRect frame);
void compass_layer_destroy(CompassLayer *layer);

int32_t compass_layer_get_angle(CompassLayer *layer);
void compass_layer_set_angle(CompassLayer *layer, int32_t angle);

int32_t compass_layer_get_presentation_angle(CompassLayer *layer);

void compass_layer_set_friction(CompassLayer *layer, float friction);
float compass_layer_get_friction(CompassLayer *layer);

void compass_layer_set_attraction(CompassLayer *layer, float attraction);
float compass_layer_get_attraction(CompassLayer *layer);

void compass_layer_set_update_callback(CompassLayer *layer, CompassLayerCallback cb);