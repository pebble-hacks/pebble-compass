#include "pebble.h"

typedef struct InvertedCrossHairLayer InvertedCrossHairLayer;

Layer *inverted_cross_hair_layer_get_layer(InvertedCrossHairLayer* layer);

InvertedCrossHairLayer *inverted_cross_hair_layer_create(GRect frame);
void inverted_cross_hair_layer_destroy(InvertedCrossHairLayer *layer);
