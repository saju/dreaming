#ifndef _DREAMING_H
#define _DREAMING_H

#include <stdint.h>
#include <stdlib.h>

#define ITERATION_THRESHOLD 8000

struct _rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct _rgb *naive_rgb_palette();
struct _rgb *build_RGB_colormap();
struct _rgb *build_RGB_colormap_reverse();
struct _rgb *build_RGB_grayscale();
#endif
