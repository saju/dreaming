#ifndef _DREAMING_H
#define _DREAMING_H

#include <stdint.h>
#include <stdlib.h>

#define ITERATION_THRESHOLD 1000

struct _rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

typedef struct {
  /* number of colors in this palette */
  int size;
  struct _rgb *colors;
} palette_t;


/*
 * Default palette found in Ultra Fractal.
 * From: https://stackoverflow.com/a/16505538
 */
palette_t *uf_rgb_palette();

/*
 * Red -> Yellow -> Green -> Cyan -> Blue -> Magenta -> Red
 */
struct _rgb *build_RGB_colormap();

/*
 * like build_RGB_colormap but starts from blue
 */
struct _rgb *build_RGB_colormap_reverse();

/*
 * RGB grayscale
 */
palette_t *grayscale_RGB_palette();
#endif
