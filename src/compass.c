#include "pebble.h"
#include "compass_window.h"
#include "compass_calibration_window.h"

static CompassWindow *compass_window;

// uncomment this line to do nothing but showing the calibration screen
//#define DEMO_CALIBRATION_MODE

#ifdef DEMO_CALIBRATION_MODE
static CompassCalibrationWindow *calibration_window;

static void fake_calibration_window(void) {
    if(!calibration_window) return;

//    // completely fill the ring to see rendering problems due to PBL-5833
//    for(int i = 0; i < 360; i++)
//        compass_calibration_window_merge_value(calibration_window, i * TRIG_MAX_ANGLE / 360, 255);

    static int32_t angle;
    static uint16_t intensity;
    angle += 15 + (TRIG_MAX_ANGLE / 360);
    intensity += 3;

    compass_calibration_window_merge_value(calibration_window, angle, (uint8_t) (intensity / 5));
    compass_calibration_window_set_current_angle(calibration_window, angle);

    app_timer_register(1000/20, (AppTimerCallback) fake_calibration_window, 0);
}

#endif

static void init(void) {
#ifdef DEMO_CALIBRATION_MODE
    calibration_window = compass_calibration_window_create();
    window_stack_push(compass_calibration_window_get_window(calibration_window), true);
    fake_calibration_window();

#else
    compass_window = compass_window_create();

    window_stack_push(compass_window_get_window(compass_window), true);
#endif
}

static void deinit(void) {
#ifdef DEMO_CALIBRATION_MODE
    compass_calibration_window_destroy(calibration_window);
#else
    compass_window_destroy(compass_window);
#endif
}

int main(void) {
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", compass_window);

    app_event_loop();
    deinit();
    return 0;
}
