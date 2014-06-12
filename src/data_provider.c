#include "data_provider.h"

typedef struct {
    int32_t target_angle;
    int32_t angular_velocity;
    int32_t presentation_angle;
    float friction;
    float attraction;
    AppTimer *timer;
    DataProviderHandlers handlers;
    void *user_data;

    DataProviderOrientation orientation;
    float orientation_transition_factor;
    float orientation_animation_start_value;
    Animation *orientation_animation;

    AccelData last_accel_data;
} DataProviderState;

// TODO: get rid of me
DataProviderState* dataProviderStateSingleton;


static void schedule_update(DataProviderState *state);

static void call_handler_if_set(DataProviderState *state, DataProviderHandler handler) {
    if(handler) {
        handler((DataProvider *)state, state->user_data);
    }
}

static void update_state(DataProviderState *state) {
    state->presentation_angle = state->presentation_angle + state->angular_velocity;
    int32_t attraction = (int32_t)((state->target_angle - state->presentation_angle) * state->attraction);
    state->angular_velocity += attraction;
    state->angular_velocity = (int32_t) (state->angular_velocity * state->friction);

    call_handler_if_set(state, state->handlers.presented_angle_changed);

    state->timer = NULL;
    if((int32_t)(attraction*state->friction) != 0 || state->angular_velocity != 0) {
        schedule_update(state);
    } else {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "NeedleLayer rested");
    }
}

int32_t data_provider_get_presentation_angle(DataProvider *provider) {
    DataProviderState *state = (DataProviderState *) provider;
    return state->presentation_angle;
}

static void schedule_update(DataProviderState *state) {
    if(!state->timer) {
        state->timer = app_timer_register(1000 / 30, (AppTimerCallback) update_state, state);
    }
}

int32_t data_provider_get_target_angle(DataProvider *provider) {
    DataProviderState *state = (DataProviderState *) provider;
    return state->target_angle;
}

void data_provider_set_target_angle(DataProvider *provider, int32_t angle) {
    DataProviderState *state = (DataProviderState *) provider;
    state->target_angle = angle;
    schedule_update(state);
}

// ---------------
// orientation

void data_provider_set_orientation_transition_factor(DataProvider* provider, float factor) {
    DataProviderState *state = (DataProviderState *) provider;
    state->orientation_transition_factor = factor;
    call_handler_if_set(state, state->handlers.orientation_transition_factor_changed);
}

float data_provider_get_orientation_transition_factor(DataProvider* provider) {
    DataProviderState *state = (DataProviderState *) provider;
    return state->orientation_transition_factor;
}

static void data_provider_update_transition_factor(struct Animation *animation, const uint32_t time_normalized) {
    DataProviderState *state = animation->context;
    float f = (float)time_normalized / ANIMATION_NORMALIZED_MAX;
    float target = state->orientation == DataProviderOrientationUpright ? 1 : 0;

    data_provider_set_orientation_transition_factor((DataProvider *)state, target * f + (1-f) * state->orientation_animation_start_value);
}

static AnimationImplementation transition_animation = {
        .update = data_provider_update_transition_factor,
};

void data_provider_set_orientation(DataProvider *provider, DataProviderOrientation orientation) {
    DataProviderState *state = (DataProviderState *) provider;
    if(state->orientation == orientation) return;

    state->orientation = orientation;
    call_handler_if_set(state, state->handlers.orientation_changed);

    // TODO: refactor to make this a property animation
    if(!state->orientation_animation) {
        state->orientation_animation = animation_create();
        state->orientation_animation->duration_ms = 200;
        state->orientation_animation->context = state;
        state->orientation_animation->implementation = &transition_animation;
    } else {
        animation_unschedule(state->orientation_animation);
    }

    state->orientation_animation_start_value = state->orientation_transition_factor;
    animation_schedule(state->orientation_animation);
}

DataProviderOrientation data_provider_get_orientation(DataProvider *provider) {
    DataProviderState *state = (DataProviderState *) provider;
    return state->orientation;
}

// ---------------
// accelerometer


static void data_provider_handle_accel_data(AccelData *data, uint32_t num_samples) {
    DataProviderState *state = dataProviderStateSingleton;

    const float f = 0.3;
    state->last_accel_data = (AccelData){
            .did_vibrate = data->did_vibrate,
            .timestamp = data->timestamp,
            .x = (int16_t) (data->x * f + (1-f) * state->last_accel_data.x),
            .y = (int16_t) (data->y * f + (1-f) * state->last_accel_data.y),
            .z = (int16_t) (data->z * f + (1-f) * state->last_accel_data.z),
    };
    call_handler_if_set(state, state->handlers.last_accel_data_changed);


    if(state->last_accel_data.y < -700) {
        data_provider_set_orientation((DataProvider *)state, DataProviderOrientationUpright);
    } else if (state->last_accel_data.y > -500) {
        data_provider_set_orientation((DataProvider *)state, DataProviderOrientationFlat);
    }
}

AccelData data_provider_last_accel_data(DataProvider *provider) {
    DataProviderState *state = dataProviderStateSingleton;
    return state->last_accel_data;
}

// ---------------
// lifecycle

DataProvider *data_provider_create(void *user_data, DataProviderHandlers handlers) {
    DataProviderState *result = malloc(sizeof(DataProviderState));
    memset(result, 0, sizeof(DataProviderState));
    result->user_data = user_data;
    result->handlers = handlers;

    result->friction = 0.9;
    result->attraction = 0.1;

    accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
    accel_data_service_subscribe(1, data_provider_handle_accel_data);

    dataProviderStateSingleton = result;

    return (DataProvider *)result;
}

void data_provider_destroy(DataProvider *provider) {
    DataProviderState *state = (DataProviderState *)provider;
    if(state->timer) {
        app_timer_cancel(state->timer);
    }
    accel_data_service_unsubscribe();

    free(provider);
}
