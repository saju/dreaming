#include "dreaming.h"


struct _rgb naive_colors[17] = {{00, 00, 00},
				{66, 30, 15},
				{25, 7, 26},
				{9, 1, 47},
				{4, 4, 73},
				{0, 7, 100},
				{12, 44, 138},
				{24, 82, 177},
				{57, 125, 209},
				{134, 181, 229},
				{211, 236, 248},
				{241, 233, 191},
				{248, 201, 95},
				{255, 170, 0},
				{204, 128, 0},
				{153, 87, 0},
				{106, 52, 3}};
//{1, 1, 1},
//				{10, 10, 10},
//				{20, 20, 20}};

inline struct _rgb *naive_rgb_palette() {
  return naive_colors;
}



/*
 * build RGB cyclic colormap
 *
 * Red -> Yellow -> Green -> Cyan -> Blue -> Magenta -> Red
 */
struct _rgb *build_RGB_colormap() {
  int i, total = 0;

  struct _rgb *rgb_map = malloc(sizeof(struct _rgb) * 6 * 256);

  /* red -> yellow interpolation */
  for (i = 0; i < 256; i++) {
    rgb_map[i].r = 255;
    rgb_map[i].g = i;
    rgb_map[i].b = 0;
  }

  total += i - 1;

  /* yellow -> green */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = (255 - i);
    rgb_map[total + i].g = 255;
    rgb_map[total + i].b = 0;
  }

  total += i - 1;

  /* green -> cyan */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = 0;
    rgb_map[total + i].g = 255;
    rgb_map[total + i].b = i;
  }

  total += i - 1;

  /* cyan -> blue */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = 0;
    rgb_map[total + i].g = (255 - i);
    rgb_map[total + i].b = 255;
  }

  total += i - 1;

  /* blue -> magenta */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = i;
    rgb_map[total + i].g = 0;
    rgb_map[total + i].b = 255;
  }

  total += i - 1;

  /* magenta -> red */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = 255;
    rgb_map[total + i].g = 0;
    rgb_map[total + i].b = (255 - i);
  }

  return rgb_map;
}

/*
 * build RGB cyclic colormap cycling from blue -> red
 *
 */
struct _rgb *build_RGB_colormap_reverse() {
  int i, total = 0;

  struct _rgb *rgb_map = malloc(sizeof(struct _rgb) * 6 * 256);

  /* blue -> magenta interpolation */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = i;
    rgb_map[total + i].g = 0;
    rgb_map[total + i].b = 255;
  }

  total += i - 1;

  /* magenta -> red */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = 255;
    rgb_map[total + i].g = 0;
    rgb_map[total + i].b = (255 - i);
  }

  total += i - 1;

  /* red -> yellow interpolation */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = 255;
    rgb_map[total + i].g = i;
    rgb_map[total + i].b = 0;
  }

  total += i - 1;

  /* yellow -> green */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = (255 - i);
    rgb_map[total + i].g = 255;
    rgb_map[total + i].b = 0;
  }

  total += i - 1;

  /* green -> cyan */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = 0;
    rgb_map[total + i].g = 255;
    rgb_map[total + i].b = i;
  }

  total += i - 1;

  /* cyan -> blue */
  for (i = 0; i < 256; i++) {
    rgb_map[total + i].r = 0;
    rgb_map[total + i].g = (255 - i);
    rgb_map[total + i].b = 255;
  }

  total += i - 1;

  return rgb_map;
}


/*
 * build RGB grayscale
 *
 * R = G = B is "shades of gray" in RGB colorspace
 */
struct _rgb *build_RGB_grayscale() {
  int i;
  struct _rgb *rgb_map = malloc(sizeof(struct _rgb) * 256);

  for (i = 0; i < 256; i++) {
    rgb_map[i].r = i;
    rgb_map[i].g = i;
    rgb_map[i].b = i;
  }

  return rgb_map;
}
