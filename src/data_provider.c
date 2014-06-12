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
