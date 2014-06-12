#include "ticks_layer.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

typedef struct {
    int32_t angle;
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
    const GRect bounds = layer_get_bounds(ticks_layer_get_layer(layer));
    const GPoint center = grect_center_point(&bounds);

    const TicksLayerData *data = ticks_layer_get_ticks_data(layer);
    const int32_t xx = sin_lookup(data->angle + angle);
    const int32_t yy = -cos_lookup(data->angle + angle);

    return (GPoint){
        (int16_t)(xx * radius / TRIG_MAX_RATIO) + center.x,
        (int16_t)(yy * radius / TRIG_MAX_RATIO) + center.y,
    };
}


void ticks_layer_update_proc(Layer *layer, GContext *ctx) {
    const GRect bounds = layer_get_bounds(layer);
    const GPoint center = grect_center_point(&bounds);

    const int32_t r2 = (MIN(bounds.size.w, bounds.size.h) / 2);

    // TODO: put GPaths into structure to avoid repetitive malloc

//    const int32_t r1 = 0;

    // draw ticks
    {
        const int num_ticks = 32;

        GPathInfo points = {
                4,
                (GPoint[4]) {},
        };
        GPath *path = gpath_create(&points);

        int32_t base_angle = layer_get_ticks_data(layer)->angle;

        for (int i = 0; i < num_ticks; i++) {
            if(i == 0){
                // for now, skip north to avoid flicker with triangle below
                continue;
            }

            int32_t angle = base_angle + (int32_t) (TRIG_MAX_ANGLE * i / num_ticks);
            int32_t len;
            int16_t d = 0;
            switch (i % 4) {
                case 0:
                    len = 10;
                    break;
                case 2:
                    len = 5;
                    break;
                default:
                    len = 2;
            }

            const int32_t r1 = r2 - len;

            const int32_t yy = -cos_lookup(angle);
            const int32_t xx = sin_lookup(angle);

            const GPoint inner = {
                    (int16_t) (xx * r1 / TRIG_MAX_RATIO) + center.x,
                    (int16_t) (yy * r1 / TRIG_MAX_RATIO) + center.y,
            };
            const GPoint outer = {
                    (int16_t) (xx * r2 / TRIG_MAX_RATIO) + center.x,
                    (int16_t) (yy * r2 / TRIG_MAX_RATIO) + center.y,
            };

            const int16_t oy = (int16_t) (-cos_lookup(angle + TRIG_MAX_ANGLE / 4) * d / TRIG_MAX_RATIO);
            const int16_t ox = (int16_t) (+sin_lookup(angle + TRIG_MAX_ANGLE / 4) * d / TRIG_MAX_RATIO);
            const int16_t py = (int16_t) (-cos_lookup(angle - TRIG_MAX_ANGLE / 4) * d / TRIG_MAX_RATIO);
            const int16_t px = (int16_t) (+sin_lookup(angle - TRIG_MAX_ANGLE / 4) * d / TRIG_MAX_RATIO);

            points.points[0].x = inner.x + ox;
            points.points[0].y = inner.y + oy;
            points.points[1].x = outer.x + ox;
            points.points[1].y = outer.y + oy;
            points.points[2].y = outer.y + py;
            points.points[2].x = outer.x + px;
            points.points[3].x = inner.x + px;
            points.points[3].y = inner.y + py;

            gpath_draw_filled(ctx, path);
            gpath_draw_outline(ctx, path);
        }

        gpath_destroy(path);
    }

    // draw north
    {
        int32_t angle = TRIG_MAX_ANGLE * 5 / 360;
        int32_t ledge = 0;
        int32_t len = 10;
        GPathInfo points = {
                3,
                (GPoint[3]) {
                    point_from_center((TicksLayer *)layer, 0, ledge+r2),
                    point_from_center((TicksLayer *)layer, angle, ledge+r2-len),
                    point_from_center((TicksLayer *)layer, -angle, ledge+r2-len),
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
        GFont font_small = fonts_get_system_font(FONT_KEY_GOTHIC_14);
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

                const GPoint p = point_from_center((TicksLayer *) layer, point_helpers[i].angle, r0);

                GSize size = graphics_text_layout_get_content_size(caption, font, GRect(0, 0, 100, 100), GTextOverflowModeFill, GTextAlignmentCenter);
                GRect text_box = (GRect) {{(int16_t) (p.x - size.w / 2), (int16_t) (p.y - size.h / 2 - vertical_text_offset)}, size};
                graphics_context_set_text_color(ctx, GColorBlack);
                graphics_draw_text(ctx, caption, font, text_box, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
            }
        }

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