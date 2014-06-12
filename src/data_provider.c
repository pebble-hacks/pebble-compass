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

} DataProviderState;

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
// lifecycle

DataProvider *data_provider_create(void *user_data, DataProviderHandlers handlers) {
    DataProviderState *result = malloc(sizeof(DataProviderState));
    memset(result, 0, sizeof(DataProviderState));
    result->user_data = user_data;
    result->handlers = handlers;

    result->friction = 0.9;
    result->attraction = 0.1;

    return (DataProvider *)result;
}

void data_provider_destroy(DataProvider *provider) {
    DataProviderState *state = (DataProviderState *)provider;
    if(state->timer) {
        app_timer_cancel(state->timer);
    }

    free(provider);
}
