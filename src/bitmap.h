/**
 * Get and Set pixel colors in bitmaps for Pebble SDK 3.0
 * Author: Jon Barlow
 * License: MIT
 */

#pragma once

#include <pebble.h>



GColor get_bitmap_color_from_palette_index(GBitmap *bitmap, uint8_t index);

GColor get_bitmap_pixel_color(GBitmap *bitmap, GBitmapFormat bitmap_format, int y, int x);

void set_bitmap_pixel_color(GBitmap *bitmap, GBitmapFormat bitmap_format, int y, int x, GColor color);
