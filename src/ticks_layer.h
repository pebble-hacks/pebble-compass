#include <pebble.h>

typedef struct TicksLayer TicksLayer;

Layer *ticks_layer_get_layer(TicksLayer* ticksLayer);

TicksLayer *ticks_layer_create(GRect frame);
void ticks_layer_destroy(TicksLayer *layer);
