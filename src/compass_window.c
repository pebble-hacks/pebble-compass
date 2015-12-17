#include "compass_window.h"
#include "ticks_layer.h"
#include "data_provider.h"
#include "compass_calibration_window.h"
#include "bitmap.h"

// window smoothly blends between to modes: "rose"(0.0f) and "band" (1.0f)
// it adjusts various coordinates in compass_layer_update_layout()

typedef struct {
    // direction string "N", "NE", ...
    GRect direction_layer_rect_rose;
    GRect direction_layer_rect_band;
    TextLayer *direction_layer;

    // current angle
    GRect angle_layer_rect_rose;
    GRect angle_layer_rect_band;
    TextLayer *angle_layer;

    // will render as ticks and indicator for "rose" or "band"
    GRect pointer_layer_rect_rose;
    GRect pointer_layer_rect_band;
    TicksLayer *ticks_layer;
    Layer *pointer_layer;

    DataProvider* data_provider;

    Animation *transition_animation;
    float transition_animation_start_value;

    // background for level indicator
    BitmapLayer *large_cross_hair_layer;
    GBitmap *large_cross_hair;

    // current level
    Layer *small_cross_hair_layer;
    GBitmap *small_cross_hair;
    GBitmapFormat small_cross_hair_format;

    // will be created on demand
    bool window_appeared;
    CompassCalibrationWindow *calibration_window;
} CompassWindowData;

Window *compass_window_get_window(CompassWindow *window) {
    return (Window *)window;
}

static GSize size_blend(GSize s1, GSize s2, float f) {
    return (GSize){
        (int16_t) (s1.w * (1-f) + f * s2.w),
        (int16_t) (s1.h * (1-f) + f * s2.h),
    };
}

static GRect rect_blend(GRect *r1, GRect *r2, float f) {
    return (GRect){
        .origin = {
            (int16_t) (r1->origin.x * (1-f) + f * r2->origin.x),
            (int16_t) (r1->origin.y * (1-f) + f * r2->origin.y),
        },
       .size = size_blend(r1->size, r2->size, f),
    };
}

static GRect rect_centered_with_size_and_offset(GRect *r1, GSize size, GPoint offset) {
    return (GRect){
            .origin = {
                    (int16_t) ((r1->size.w - size.w) / 2 + r1->origin.x + offset.x),
                    (int16_t) ((r1->size.h - size.h) / 2 + r1->origin.y + offset.y),
            },
            .size = size,
    };
}

static void small_cross_hair_layer_update(Layer *layer, GContext *ctx) {
  Window *window = layer_get_window(layer);
  CompassWindowData *data = window_get_user_data(window);

  GBitmap *bg_image = graphics_capture_frame_buffer(ctx);
  GBitmapFormat bg_format = gbitmap_get_format(bg_image);

  GRect fg_frame = layer_get_frame(layer);

  for(int16_t y = 0; y < fg_frame.size.h; y++) {
    for(int16_t x = 0; x < fg_frame.size.w; x++) {

      GColor bg_pixel = get_bitmap_pixel_color(bg_image, bg_format, fg_frame.origin.y + y, fg_frame.origin.x + x);
      GColor fg_pixel = get_bitmap_pixel_color(data->small_cross_hair, data->small_cross_hair_format, y, x);

      if(gcolor_equal(bg_pixel, GColorWhite)) {
        set_bitmap_pixel_color(bg_image, bg_format, fg_frame.origin.y + y, fg_frame.origin.x + x, PBL_IF_COLOR_ELSE(GColorRed, GColorBlack));
      }
      else {
        set_bitmap_pixel_color(bg_image, bg_format, fg_frame.origin.y + y, fg_frame.origin.x + x, fg_pixel);
      }

    }
  }

  graphics_release_frame_buffer(ctx, bg_image);
}

static void compass_layer_update_layout(CompassWindowData *data) {
    int32_t angle = data_provider_get_presentation_angle(data->data_provider);
    ticks_layer_set_angle(data->ticks_layer, angle);

    const float transition_factor = data_provider_get_orientation_transition_factor(data->data_provider);
    ticks_layer_set_transition_factor(data->ticks_layer, transition_factor);

    static char angle_text[] = "123°";
    int32_t normalized_angle = ((int)(angle * 360 / TRIG_MAX_ANGLE) % 360 + 360) % 360;
    snprintf(angle_text, sizeof(angle_text), "%d°", (int)normalized_angle);
    text_layer_set_text(data->angle_layer, angle_text);
    GRect r = rect_blend(&data->angle_layer_rect_rose, &data->angle_layer_rect_band, transition_factor);
    layer_set_frame(text_layer_get_layer(data->angle_layer), r);
    // workaround for PBL-8492, manually call set_bounds after changing the frame
    layer_set_bounds(text_layer_get_layer(data->angle_layer), (GRect){.size=r.size});

    static char *direction_texts[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    const int degrees_per_text = 360 / ARRAY_LENGTH(direction_texts);
    int32_t direction_index = ((normalized_angle + (degrees_per_text / 2)) / degrees_per_text) % ARRAY_LENGTH(direction_texts);
    text_layer_set_text(data->direction_layer, direction_texts[direction_index]);
    layer_set_frame(text_layer_get_layer(data->direction_layer), rect_blend(&data->direction_layer_rect_rose, &data->direction_layer_rect_band, transition_factor));

    layer_set_frame(data->pointer_layer, rect_blend(&data->pointer_layer_rect_rose, &data->pointer_layer_rect_band, transition_factor));

    GRect frame = layer_get_frame(ticks_layer_get_layer(data->ticks_layer));

    // make crosses move down outside the screen when transition to cartesian representation
    int16_t transition_dy = (int16_t) (transition_factor * frame.size.h);

    layer_set_frame(bitmap_layer_get_layer(data->large_cross_hair_layer),
            rect_centered_with_size_and_offset(&frame, gbitmap_get_bounds(data->large_cross_hair).size, GPoint(1, transition_dy)));

    int16_t d = 40;   // TODO: get rid of magic number
    AccelData ad = data_provider_last_accel_data(data->data_provider);
    GPoint small_cross_hair_offset = GPoint((int16_t)(1 - ad.x / d), ad.y / d);
    // make small cross hair stick to center during transition until almost back to polar representation
    float tf2 = (1-transition_factor) * (1-transition_factor);
    tf2 *= tf2;
    small_cross_hair_offset.x *= tf2;
    small_cross_hair_offset.y *= tf2;
    small_cross_hair_offset.y += transition_dy;

    layer_set_frame(data->small_cross_hair_layer,
            rect_centered_with_size_and_offset(&frame, GSize(17, 17), small_cross_hair_offset));

}

static void pointer_layer_update(Layer *layer, GContext *ctx) {

  Window *window = layer_get_window(layer);
  CompassWindowData *data = window_get_user_data(window);

  GBitmap *bg_image = graphics_capture_frame_buffer(ctx);
  GBitmapFormat bg_format = gbitmap_get_format(bg_image);

  GRect fg_frame = layer_get_frame(layer);

  for(int16_t y = 0; y < fg_frame.size.h; y++) {
    for(int16_t x = 0; x < fg_frame.size.w; x++) {

      GColor bg_pixel = get_bitmap_pixel_color(bg_image, bg_format, fg_frame.origin.y + y, fg_frame.origin.x + x);

      if(!gcolor_equal(bg_pixel, GColorBlack)) {
        set_bitmap_pixel_color(bg_image, bg_format, fg_frame.origin.y + y, fg_frame.origin.x + x, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
      }
      else {
        set_bitmap_pixel_color(bg_image, bg_format, fg_frame.origin.y + y, fg_frame.origin.x + x, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
      }
    }
  }

  graphics_release_frame_buffer(ctx, bg_image);

  //GRect bounds = layer_get_bounds(layer);
  //graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  //graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void compass_window_load(Window *window) {
    // TODO: get rid of absolute coordinates
    // one day... I hope... we will have an interface builder... and transitioning by the firmware...
    CompassWindowData *data = window_get_user_data(window);
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    const int16_t text_height_rose = 24;
    const int16_t text_height_band = 55;
    const GFont text_font = fonts_get_system_font(PBL_IF_ROUND_ELSE(FONT_KEY_GOTHIC_18_BOLD, FONT_KEY_GOTHIC_24_BOLD));

    const int16_t rose_text_offset_top = PBL_IF_ROUND_ELSE(bounds.size.h/2, bounds.size.h) - PBL_IF_ROUND_ELSE((text_height_rose/2), text_height_rose);

    const int16_t direction_layer_width = 40;
    const int16_t direction_layer_margin_band = PBL_IF_ROUND_ELSE(20, 10);
    data->direction_layer_rect_rose = (GRect){.origin = {PBL_IF_ROUND_ELSE(bounds.size.w - direction_layer_width - PBL_IF_ROUND_ELSE(22, 0), bounds.size.w - direction_layer_width), rose_text_offset_top}, .size = {direction_layer_width, text_height_rose}};
    data->direction_layer_rect_band = (GRect){.origin = {bounds.size.w - direction_layer_width - direction_layer_margin_band, (int16_t)(bounds.size.h - text_height_band)}, .size = {direction_layer_width, text_height_rose}};
    data->direction_layer = text_layer_create(data->direction_layer_rect_rose);
    text_layer_set_text_alignment(data->direction_layer, GTextAlignmentLeft);
    text_layer_set_font(data->direction_layer, text_font);
    text_layer_set_text_color(data->direction_layer, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
    text_layer_set_background_color(data->direction_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(data->direction_layer));

    const int16_t angle_layer_width_rose = 40;
    const int16_t angle_layer_width_band = 65;

    data->angle_layer_rect_rose = (GRect){.origin = {PBL_IF_ROUND_ELSE(27, 0), rose_text_offset_top}, .size = {angle_layer_width_rose, text_height_rose}};
    data->angle_layer_rect_band = (GRect){.origin = {0, (int16_t)(bounds.size.h - text_height_band)}, .size = {angle_layer_width_band, text_height_band}};
    data->angle_layer = text_layer_create(data->angle_layer_rect_rose);
    text_layer_set_text_alignment(data->angle_layer, GTextAlignmentRight);
    text_layer_set_font(data->angle_layer, text_font);
    text_layer_set_text_color(data->angle_layer, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
    text_layer_set_background_color(data->angle_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(data->angle_layer));

    GRect roseRect = ((GRect){.origin={0, 8}, .size={bounds.size.w, (int16_t)(bounds.size.h - 15)}});

    data->ticks_layer = ticks_layer_create(roseRect);
    layer_add_child(window_layer, ticks_layer_get_layer(data->ticks_layer));

    data->pointer_layer_rect_rose = (GRect){{(bounds.size.w-2)/2, 0}, {3, 20}};
    data->pointer_layer_rect_band = (GRect){{(bounds.size.w-2)/2, 18}, {3, 40}};
    data->pointer_layer = layer_create(data->pointer_layer_rect_rose);
    layer_set_update_proc(data->pointer_layer, pointer_layer_update);
    layer_add_child(window_layer, (data->pointer_layer));

    data->large_cross_hair_layer = bitmap_layer_create(GRectZero);
    data->large_cross_hair = gbitmap_create_with_resource(RESOURCE_ID_CROSS_HAIR_LARGE);
    bitmap_layer_set_bitmap(data->large_cross_hair_layer, data->large_cross_hair);
    layer_add_child(window_layer, bitmap_layer_get_layer(data->large_cross_hair_layer));

    data->small_cross_hair_layer = layer_create(GRectZero);
    data->small_cross_hair = gbitmap_create_with_resource(RESOURCE_ID_CROSS_HAIR_SMALL);
    data->small_cross_hair_format = gbitmap_get_format(data->small_cross_hair);
    layer_add_child(window_layer, data->small_cross_hair_layer);
    layer_set_update_proc(data->small_cross_hair_layer, small_cross_hair_layer_update);

    data_provider_set_target_angle(data->data_provider, (360 - 45) * TRIG_MAX_ANGLE / 360);
}

static void compass_window_appear(Window *window) {
    // needed to prevent calibration window appearing before the compass was presented
    CompassWindowData *data = window_get_user_data(window);
    data->window_appeared = true;
}

static void compass_window_unload(Window *window) {
    CompassWindowData *data = window_get_user_data(window);
    ticks_layer_destroy(data->ticks_layer);
    text_layer_destroy(data->angle_layer);
    text_layer_destroy(data->direction_layer);
    layer_destroy(data->pointer_layer);

    gbitmap_destroy(data->large_cross_hair);
    bitmap_layer_destroy(data->large_cross_hair_layer);

    gbitmap_destroy(data->small_cross_hair);
    layer_destroy(data->small_cross_hair_layer);
}

void propagate_interference_to_calibration_window(CompassWindowData *window_data) {
    if(window_data->calibration_window) {
        bool influenced = data_provider_is_influenced_by_magnetic_interference(window_data->data_provider);
        compass_calibration_window_set_influenced_by_magnetic_interference(window_data->calibration_window, influenced);
    }
}

static void handle_data_provider_update(DataProvider *provider, void *user_data) {
    // switch between calibration window and actual compass

    CompassWindowData *data = user_data;
    if (data->calibration_window && window_stack_get_top_window() == compass_calibration_window_get_window(data->calibration_window)) {
        AccelData accel_data = data_provider_get_damped_accel_data(provider);
        compass_calibration_window_apply_accel_data(data->calibration_window, accel_data);

        if (!data_provider_compass_needs_calibration(provider)) {
            window_stack_pop(true);
            vibes_long_pulse();
        }
    } else {
        compass_layer_update_layout(data);

        if (data->window_appeared && data_provider_compass_needs_calibration(provider)) {
            if (!data->calibration_window) {
                data->calibration_window = compass_calibration_window_create();
            }
            propagate_interference_to_calibration_window(data);
            window_stack_push(compass_calibration_window_get_window(data->calibration_window), true);
        }
    }
}

static void handle_data_provider_interference_update(DataProvider *provider, void* user_data) {
    CompassWindowData *data = user_data;
    propagate_interference_to_calibration_window(data);
}

CompassWindow *compass_window_create() {
    Window *window = window_create();
    CompassWindowData *data = malloc(sizeof(CompassWindowData));

    data->data_provider = data_provider_create(data, (DataProviderHandlers) {
            .presented_angle_or_accel_data_changed = handle_data_provider_update,
            .orientation_transition_factor_changed = handle_data_provider_update,
            .magnetic_interference_changed = handle_data_provider_interference_update,
    });

    window_set_user_data(window, data);

    window_set_window_handlers(window, (WindowHandlers) {
            .load = compass_window_load,
            .unload = compass_window_unload,
            .appear = compass_window_appear,
    });
    window_set_background_color(window, GColorBlack);

    return (CompassWindow *) window;
}

void compass_window_destroy(CompassWindow *window) {
    if(!window)return;

    CompassWindowData *data = window_get_user_data((Window *)window);
    data_provider_destroy(data->data_provider);
    compass_calibration_window_destroy(data->calibration_window);
    free(data);

    window_destroy((Window *)window);
}
