#include <pebble.h>
#include "compass_window.h"

static CompassWindow *window;


static void init(void) {
    window = compass_window_create();

    window_stack_push(compass_window_get_window(window), true);
}

static void deinit(void) {
    compass_window_destroy(window);
}

int main(void) {
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();
    deinit();
    return 0;
}
