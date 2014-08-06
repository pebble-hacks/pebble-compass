#include <pebble.h>

typedef struct CompassCalibrationWindow CompassCalibrationWindow;

Window *compass_calibration_window_get_window(CompassCalibrationWindow *window);

CompassCalibrationWindow *compass_calibration_window_create();
void compass_calibration_window_destroy(CompassCalibrationWindow *window);

void compass_calibration_window_merge_value(CompassCalibrationWindow *window, int32_t angle, uint8_t intensity);

void compass_calibration_window_set_current_angle(CompassCalibrationWindow *window, int32_t angle);