#include "pebble.h"
#include "data_provider.h"

typedef struct {
    int32_t target_angle;
    int32_t angular_velocity;
    int32_t presentation_angle;
    int32_t compass_delta_angle;
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
    AccelData damped_accel_data;

    CompassHeadingData heading;

    BatteryChargeState battery_charge_state;
} DataProviderState;

// TODO: get rid of floats throughout this file (see readme)

static const int DATA_PROVIDER_FPS = 22;

// TODO: get rid of this singleton. Unfortunately, compass API does not support a context object
DataProviderState* dataProviderStateSingleton;

static void schedule_update(DataProviderState *state);

static void call_handler_if_set(DataProviderState *state, DataProviderHandler handler) {
    if(handler) {
        handler((DataProvider *)state, state->user_data);
    }
}

static void update_state(DataProviderState *state) {
    state->presentation_angle = state->presentation_angle + state->angular_velocity;
    int32_t distance = state->target_angle - state->presentation_angle;
    while (distance < -TRIG_MAX_ANGLE / 2) distance += TRIG_MAX_ANGLE;
    while (distance > +TRIG_MAX_ANGLE / 2) distance -= TRIG_MAX_ANGLE;

    float attraction_factor = state->attraction;
    if (state->handlers.attraction_modifier) {
        attraction_factor = state->handlers.attraction_modifier((DataProvider *) state, attraction_factor, state->user_data);
    }
    int32_t attraction = (int32_t) (distance * attraction_factor);
    state->angular_velocity += attraction;
    float friction = state->friction;
    if (state->handlers.friction_modifier) {
        friction = state->handlers.friction_modifier((DataProvider *) state, friction, state->user_data);
    }
    state->angular_velocity = (int32_t) (state->angular_velocity * friction);

    call_handler_if_set(state, state->handlers.presented_angle_or_accel_data_changed);
    state->timer = NULL;
    schedule_update(state);

    // once could optimize this and stop the updates once,
    //
    //    (int32_t)(attraction*state->friction) != 0 || state->angular_velocity != 0
    //
    // but in reality the compass input constantly changes
    // to simplify dependent code (e.g. for transitions that require constant redraws)
    // we simply loop infinitely
    // be aware that this drains the battery!
}

int32_t data_provider_get_presentation_angle(DataProvider *provider) {
    DataProviderState *state = (DataProviderState *) provider;
    return state->presentation_angle;
}

void data_provider_set_presentation_angle(DataProvider *provider, int32_t angle) {
    DataProviderState *state = (DataProviderState *) provider;
    state->presentation_angle = angle;
    state->angular_velocity = 0;
}

static void schedule_update(DataProviderState *state) {
    if(!state->timer) {
        state->timer = app_timer_register(1000 / DATA_PROVIDER_FPS, (AppTimerCallback) update_state, state);
    }
}

int32_t data_provider_get_target_angle(DataProvider *provider) {
    DataProviderState *state = (DataProviderState *) provider;
    return state->target_angle;
}

void data_provider_set_target_angle(DataProvider *provider, int32_t angle) {
    DataProviderState *state = (DataProviderState *) provider;
    if (state->handlers.target_angle_modifier) {
        angle = state->handlers.target_angle_modifier(provider, angle, state->user_data);
    }
    state->target_angle = angle;
    schedule_update(state);
}

int32_t data_provider_get_compass_delta_angle(DataProvider *provider) {
    DataProviderState *state = (DataProviderState *) provider;
    return state->compass_delta_angle;
}

void data_provider_set_compass_delta_angle(DataProvider *provider, int32_t angle) {
    DataProviderState *state = (DataProviderState *) provider;
    state->compass_delta_angle = angle;
}

void data_provider_delta_heading_angle(DataProvider *provider, int32_t delta) {
    DataProviderState *state = (DataProviderState *) provider;
    state->target_angle += delta;
    while(state->target_angle < 0) state->target_angle += TRIG_MAX_ANGLE;
    while(state->target_angle > TRIG_MAX_ANGLE) state->target_angle -= TRIG_MAX_ANGLE;
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

// easing is currently only supported for PropertyAnimations, see PBL-6328
static float CubicEaseInOut(float p)
{
    if(p < 0.5f)
    {
        return 4 * p * p * p;
    }
    else
    {
        float f = ((2 * p) - 2);
        return 0.5f * f * f * f + 1;
    }
}

static void data_provider_update_transition_factor(struct Animation *animation, const uint32_t time_normalized) {
    DataProviderState *state = animation->context;
    float f = (float)time_normalized / ANIMATION_NORMALIZED_MAX;
    f = CubicEaseInOut(f); // manual easing, see PBL-6328

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
        state->orientation_animation->duration_ms = 450;
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


static void merge_accel_data(AccelData *dest, AccelData *next, float factor) {
    *dest = (AccelData){
            .did_vibrate = next->did_vibrate,
            .timestamp = next->timestamp,
            .x = (int16_t)(next->x * factor + (1 - factor) * dest->x),
            .y = (int16_t)(next->y * factor + (1 - factor) * dest->y),
            .z = (int16_t)(next->z * factor + (1 - factor) * dest->z),
    };
}

static void data_provider_handle_accel_data(AccelData *data, uint32_t num_samples) {
    DataProviderState *state = dataProviderStateSingleton;

    merge_accel_data(&state->last_accel_data, data, 0.99f);
    merge_accel_data(&state->damped_accel_data, data, 0.3f);
    call_handler_if_set(state, state->handlers.input_accel_data_changed);


    if(state->damped_accel_data.y < -700) {
        data_provider_set_orientation((DataProvider *)state, DataProviderOrientationUpright);
    } else if (state->damped_accel_data.y > -500) {
        data_provider_set_orientation((DataProvider *)state, DataProviderOrientationFlat);
    }
}

AccelData data_provider_last_accel_data(DataProvider *provider) {
    DataProviderState *state = dataProviderStateSingleton;
    return state->last_accel_data;
}

AccelData data_provider_get_damped_accel_data(DataProvider *provider) {
    DataProviderState *state = dataProviderStateSingleton;
    return state->damped_accel_data;
}

bool data_provider_compass_needs_calibration(DataProvider *provider) {
    DataProviderState *state = dataProviderStateSingleton;

    return state->heading.compass_status == CompassStatusDataInvalid;
}

bool data_provider_is_influenced_by_magnetic_interference(DataProvider *provider) {
    DataProviderState *state = dataProviderStateSingleton;
    bool result = state->battery_charge_state.is_plugged;
    return result;
}

static void data_provider_handle_compass_data(CompassHeadingData heading) {
    DataProviderState *state = dataProviderStateSingleton;

    state->heading = heading;

    // TODO: look at is_declination_valid and use true_heading if available (configured by user?)
    data_provider_set_target_angle((DataProvider*)dataProviderStateSingleton, TRIG_MAX_ANGLE-heading.magnetic_heading - state->compass_delta_angle);
    call_handler_if_set(state, state->handlers.input_heading_changed);
}

static void data_provider_handle_battery(BatteryChargeState charge) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "data_provider_handle_battery");

    DataProviderState *state = dataProviderStateSingleton;
    state->battery_charge_state = charge;
    call_handler_if_set(state, state->handlers.magnetic_interference_changed);
}

// ---------------
// lifecycle

DataProvider *data_provider_create(void *user_data, DataProviderHandlers handlers) {
    DataProviderState *result = malloc(sizeof(DataProviderState));
    memset(result, 0, sizeof(DataProviderState));
    result->user_data = user_data;
    result->handlers = handlers;

    result->friction = 0.9f;
    result->attraction = 0.05f;
    result->heading.compass_status = CompassStatusCalibrated; // assume calibrated data by default

    dataProviderStateSingleton = result;

    compass_service_subscribe(data_provider_handle_compass_data);

    accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
    accel_data_service_subscribe(1, data_provider_handle_accel_data);

    result->battery_charge_state = battery_state_service_peek();
    battery_state_service_subscribe(data_provider_handle_battery);

    schedule_update(result);

    return (DataProvider *)result;
}

void data_provider_destroy(DataProvider *provider) {
    if(!provider)return;

    DataProviderState *state = (DataProviderState *)provider;
    if(state->timer) {
        app_timer_cancel(state->timer);
    }
    accel_data_service_unsubscribe();
    compass_service_unsubscribe();

    free(provider);

    // singletons suck!
    if(dataProviderStateSingleton == state) {
        dataProviderStateSingleton = NULL;
    }
}

