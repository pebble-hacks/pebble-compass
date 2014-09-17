#include "ticks_layer.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

typedef struct {
    int32_t angle;
    float transition_factor;
} TicksLayerData;

Layer *ticks_layer_get_layer(TicksLayer* ticksLayer) {
    return (Layer*)ticksLayer;
}

static TicksLayerData *layer_get_ticks_data(Layer *layer) {
    return layer_get_data(layer);
}

static TicksLayerData *ticks_layer_get_ticks_data(TicksLayer *layer) {
    return layer_get_ticks_data((Layer*)layer);
}

static GPoint point_from_center(TicksLayer* layer, int32_t angle, int32_t radius) {
    // this is the heart of the smooth transition
    // it calculates two coordinates "rose" (polar) and "band" (cartesian) to blend between them

    // TODO: get rid of floats

    const GRect bounds = layer_get_bounds(ticks_layer_get_layer(layer));
    const GPoint center = grect_center_point(&bounds);
    const TicksLayerData *data = ticks_layer_get_ticks_data(layer);

    // polar
    const int32_t xx = sin_lookup(-data->angle + angle);
    const int32_t yy = -cos_lookup(-data->angle + angle);

    GPoint polar = (GPoint){
        (int16_t)(xx * radius / TRIG_MAX_RATIO) + center.x,
        (int16_t)(yy * radius / TRIG_MAX_RATIO) + center.y,
    };

    // cartesian
    int32_t delta = (angle - data->angle) % TRIG_MAX_ANGLE;
    while(delta > +TRIG_MAX_ANGLE / 2)delta -= TRIG_MAX_ANGLE;
    while(delta < -TRIG_MAX_ANGLE / 2)delta += TRIG_MAX_ANGLE;

    int32_t factor = bounds.size.w + bounds.size.h;
    GPoint cartesian = (GPoint){
        (int16_t) (center.x + (delta * factor / TRIG_MAX_ANGLE)),
        (int16_t) (center.y - 2*radius + bounds.size.h * 0.7f)
    };

    float f = data->transition_factor;

    return (GPoint){
            (int16_t) (cartesian.x * f + (1-f) * polar.x),
            (int16_t) (cartesian.y * f + (1-f) * polar.y),
    };
}

static bool ticks_layer_is_polar(TicksLayer *layer) {
    return ticks_layer_get_ticks_data(layer)->transition_factor <= 0.01f;
}

static int32_t tick_len(TicksLayer *layer, int tick_idx) {
    if(tick_idx == 0) {
        return ticks_layer_is_polar(layer) ? 0 : 10;
    }
    switch(tick_idx % 4) {
        case 0: return 10;
        case 2: return 5;
        default: return 2;
    }
}

static void ticks_layer_update_proc(Layer *layer, GContext *ctx) {
    TicksLayer *ticks_layer = (TicksLayer *)layer;
    TicksLayerData *data = layer_get_ticks_data(layer);
    const GRect bounds = layer_get_bounds(layer);

    const int32_t r2 = (MIN(bounds.size.w, bounds.size.h) / 2);

    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_text_color(ctx, GColorWhite);

    // draw ticks
    {
        const int num_ticks = 32;

        for (int i = 0; i < num_ticks; i++) {
            int32_t angle = (int32_t) (TRIG_MAX_ANGLE * i / num_ticks);
            int32_t len = tick_len(ticks_layer, i);
            const int32_t r1 = r2 - len;

            const GPoint inner = point_from_center(ticks_layer, angle, r1);
            const GPoint outer = point_from_center(ticks_layer, angle, r2);

            graphics_draw_line(ctx, inner, outer);
        }
    }

    // draw north (can be omitted if fully transitioned to cartesian representation)
    if(data->transition_factor < 1) {
        int32_t angle_polar = TRIG_MAX_ANGLE * 5 / 360;
        int32_t angle = (int32_t) (0 * data->transition_factor + (1-data->transition_factor) * angle_polar);
        int32_t ledge = 0;
        int32_t len = 10;
        GPathInfo points = {
                3,
                (GPoint[3]) {
                    point_from_center(ticks_layer, 0, ledge+r2),
                    point_from_center(ticks_layer, angle, ledge+r2-len),
                    point_from_center(ticks_layer, -angle, ledge+r2-len),
                },
        };
        GPath *path = gpath_create(&points);
        gpath_draw_filled(ctx, path);
        gpath_destroy(path);
    }

    // draw letters
    {

        typedef struct {
            char* caption;
            int32_t angle;
            GFont font;
        } point_helper;

        GFont font_large = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
//        GFont font_small = fonts_get_system_font(FONT_KEY_GOTHIC_14);
        point_helper point_helpers[] = {
                {"N", TRIG_MAX_ANGLE * 0 / 8, font_large},
//                {"NE",TRIG_MAX_ANGLE * 1 / 8, font_small},
                {"E", TRIG_MAX_ANGLE * 2 / 8, font_large},
//                {"SE",TRIG_MAX_ANGLE * 3 / 8, font_small},
                {"S", TRIG_MAX_ANGLE * 4 / 8, font_large},
//                {"SW",TRIG_MAX_ANGLE * 5 / 8, font_small},
                {"W", TRIG_MAX_ANGLE * 6 / 8, font_large},
//                {"NW",TRIG_MAX_ANGLE * 7 / 8, font_small},
        };

        {
            int32_t margin_letter = 19;
            const int32_t r0 = r2 - margin_letter;
            const int16_t vertical_text_offset = 3;

            for (uint32_t i = 0; i < ARRAY_LENGTH(point_helpers); i++) {
                char const *caption = point_helpers[i].caption;
                const GFont font = point_helpers[i].font;

                const GPoint p = point_from_center(ticks_layer, point_helpers[i].angle, r0);

                GSize size = graphics_text_layout_get_content_size(caption, font, GRect(0, 0, 100, 100), GTextOverflowModeFill, GTextAlignmentCenter);
                GRect text_box = (GRect) {{(int16_t) (p.x - size.w / 2), (int16_t) (p.y - size.h / 2 - vertical_text_offset)}, size};
                graphics_draw_text(ctx, caption, font, text_box, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
            }
        };

    }

}

TicksLayer *ticks_layer_create(GRect frame) {
    Layer *result = layer_create_with_data(frame, sizeof(TicksLayerData));

    layer_set_update_proc(result, ticks_layer_update_proc);
    return (TicksLayer *)result;
}

void ticks_layer_destroy(TicksLayer *layer) {
    layer_destroy((Layer *)layer);
}

void ticks_layer_set_angle(TicksLayer* layer, int32_t angle) {
    ticks_layer_get_ticks_data(layer)->angle = angle;
    layer_mark_dirty(ticks_layer_get_layer(layer));
}

void ticks_layer_set_transition_factor(TicksLayer *layer, float factor) {
    ticks_layer_get_ticks_data(layer)->transition_factor = MIN(MAX(0, factor), 1);
    layer_mark_dirty(ticks_layer_get_layer(layer));
}

float ticks_layer_get_transition_factor(TicksLayer *layer) {
    return ticks_layer_get_ticks_data(layer)->transition_factor;
}
