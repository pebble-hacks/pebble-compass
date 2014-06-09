#include <pebble.h>

struct CompassLayer;
typedef struct CompassLayer CompassLayer;

Layer *compass_layer_get_layer(CompassLayer* compassLayer);

CompassLayer *compass_layer_create(GRect frame);
void compass_layer_destroy(CompassLayer *layer);

int32_t compass_layer_get_angle(CompassLayer *layer);
void compass_layer_set_angle(CompassLayer *layer, int32_t angle);

