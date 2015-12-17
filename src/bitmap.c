#include <pebble.h>
#include "bitmap.h"

void set_bitmap_pixel_color(GBitmap *bitmap, GBitmapFormat bitmap_format, int y, int x, GColor color) {
  GBitmapDataRowInfo row = gbitmap_get_data_row_info(bitmap, y);
  if ((x >= row.min_x) && (x <= row.max_x)) {
    switch(bitmap_format) {
      case GBitmapFormat1Bit :
        row.data[x / 8] ^= (-(gcolor_equal(color, GColorWhite)? 1 : 0) ^ row.data[x / 8]) & (1 << (x % 8));
        break;
      case GBitmapFormat1BitPalette :
        //TODO
        break;
      case GBitmapFormat2BitPalette :
        //TODO
        break;
      case GBitmapFormat4BitPalette :
        //TODO
        break;
      case GBitmapFormat8BitCircular :
      case GBitmapFormat8Bit :
        row.data[x] = color.argb;
    }
  }
}

GColor get_bitmap_color_from_palette_index(GBitmap *bitmap, uint8_t index) {
  GColor *palette = gbitmap_get_palette(bitmap);
  return palette[index];
}

GColor get_bitmap_pixel_color(GBitmap *bitmap, GBitmapFormat bitmap_format, int y, int x) {
  GBitmapDataRowInfo row = gbitmap_get_data_row_info(bitmap, y);
  switch(bitmap_format) {
    case GBitmapFormat1Bit :
      return ((row.data[x / 8] >> (x % 8)) & 1) == 1 ? GColorWhite : GColorBlack;
    case GBitmapFormat1BitPalette :
      return get_bitmap_color_from_palette_index(bitmap, ((row.data[x / 8] << (x % 8)) & 128) == 128 ? 1 : 0);
    case GBitmapFormat2BitPalette :
      return GColorBlack; //TODO get_color_from_palette_index(bitmap, (row.data[x / 4] >> (x % 4)) & 3);
    case GBitmapFormat4BitPalette :
      return GColorBlack; //TODO
    case GBitmapFormat8BitCircular :
    case GBitmapFormat8Bit :
      return (GColor) {.argb = row.data[x] };
  }
  return GColorClear;
}
