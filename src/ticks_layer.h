#include "pebble.h"

typedef struct TicksLayer TicksLayer;

Layer *ticks_layer_get_layer(TicksLayer* ticksLayer);

TicksLayer *ticks_layer_create(GRect frame);
void ticks_layer_destroy(TicksLayer *layer);

void ticks_layer_set_angle(TicksLayer* layer, int32_t angle);

float ticks_layer_get_transition_factor(TicksLayer *layer);
void ticks_layer_set_transition_factor(TicksLayer *layer, float factor);
