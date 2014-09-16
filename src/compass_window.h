#include "pebble.h"

typedef struct CompassWindow CompassWindow;

Window *compass_window_get_window(CompassWindow *window);

CompassWindow *compass_window_create();
void compass_window_destroy(CompassWindow *window);

