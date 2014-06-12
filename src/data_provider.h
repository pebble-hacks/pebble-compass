#include <pebble.h>

typedef struct DataProvider DataProvider;

typedef void (*DataProviderHandler)(DataProvider *provider, void *user_data);

typedef struct {
    DataProviderHandler input_angle_changed;
    DataProviderHandler presented_angle_changed;
    DataProviderHandler orientation_changed;
    DataProviderHandler orientation_transition_factor_changed;
} DataProviderHandlers;

typedef enum {
    DataProviderOrientationFlat = 0,
    DataProviderOrientationUpright = 1,
} DataProviderOrientation;

DataProvider *data_provider_create(void *user_data, DataProviderHandlers handlers);
void data_provider_destroy(DataProvider *pProvider);

int32_t data_provider_get_presentation_angle(DataProvider *provider);

int32_t data_provider_get_target_angle(DataProvider *provider);
void data_provider_set_target_angle(DataProvider *provider, int32_t angle);

DataProviderOrientation data_provider_get_orientation(DataProvider *provider);
void data_provider_set_orientation(DataProvider *provider, DataProviderOrientation orientation);

// NOTE: for debugging only
float data_provider_get_orientation_transition_factor(DataProvider* provider);
void data_provider_set_orientation_transition_factor(DataProvider* provider, float factor);

