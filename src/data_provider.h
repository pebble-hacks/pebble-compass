#include "pebble.h"

typedef struct DataProvider DataProvider;

typedef void (*DataProviderHandler)(DataProvider *provider, void *user_data);
typedef int32_t (*DataProviderModifyAngleHandler)(DataProvider *provider, int32_t angle, void *user_data);
typedef float (*DataProviderModifyFactorHandler)(DataProvider *provider, float factor, void *user_data);

typedef struct {
    DataProviderHandler input_heading_changed;
//    DataProviderHandler presented_angle_changed;
    DataProviderHandler orientation_changed;
    DataProviderHandler orientation_transition_factor_changed;
    DataProviderHandler input_accel_data_changed;
    DataProviderHandler presented_angle_or_accel_data_changed;
    DataProviderHandler magnetic_interference_changed;
    DataProviderModifyAngleHandler target_angle_modifier;
    DataProviderModifyFactorHandler attraction_modifier;
    DataProviderModifyFactorHandler friction_modifier;
} DataProviderHandlers;

typedef enum {
    DataProviderOrientationFlat = 0,
    DataProviderOrientationUpright = 1,
} DataProviderOrientation;

DataProvider *data_provider_create(void *user_data, DataProviderHandlers handlers);
void data_provider_destroy(DataProvider *pProvider);

int32_t data_provider_get_presentation_angle(DataProvider *provider);
void data_provider_set_presentation_angle(DataProvider *provider, int32_t angle);

int32_t data_provider_get_target_angle(DataProvider *provider);
void data_provider_set_target_angle(DataProvider *provider, int32_t angle);

int32_t data_provider_get_compass_delta_angle(DataProvider *provider);
void data_provider_set_compass_delta_angle(DataProvider *provider, int32_t angle);


void data_provider_delta_heading_angle(DataProvider *provider, int32_t angle);

DataProviderOrientation data_provider_get_orientation(DataProvider *provider);
void data_provider_set_orientation(DataProvider *provider, DataProviderOrientation orientation);

AccelData data_provider_last_accel_data(DataProvider *provider);
AccelData data_provider_get_damped_accel_data(DataProvider *provider);

bool data_provider_compass_needs_calibration(DataProvider *provider);

bool data_provider_is_influenced_by_magnetic_interference(DataProvider *provider);

// NOTE: for debugging only
float data_provider_get_orientation_transition_factor(DataProvider* provider);
void data_provider_set_orientation_transition_factor(DataProvider* provider, float factor);

