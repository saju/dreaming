#include <stdbool.h>
#include <SDL2/SDL.h>
#include "dreaming.h"

#define WIN_WIDTH  1000
#define WIN_HEIGHT 700

#define ZOOM_REDRAW_DELAY 100
#define ZOOM_SAVE_SIZE 100

#define GRAPHICS_LOOP_SLEEP 10

#define SCREEN_ASPECT_RATIO(g) (g->screen_h/g->screen_w)

/* go from 2D screen/renderer coordinates to 1D array of pixels */
#define SCREEN_TO_PIXEL(g, P) (P.x + (P.y * g->screen_w))

#define WINDOW_X_TO_SCREEN_X(g, winx) ((winx) * g->screen_w/g->window_w)
#define WINDOW_Y_TO_SCREEN_Y(g, winy) ((winy) * g->screen_h/g->window_h)
#define SCREEN_X_TO_WINDOW_X(g, sx) ((sx) * g->window_w/g->screen_w)
#define SCREEN_Y_TO_WINDOW_Y(g, sy) ((sy) * g->window_h/g->screen_h)

struct _rgb *RGB_colormap, *RGB_grayscale;

typedef struct {
  double x;
  double y;
} point_t;

typedef struct {
  double r;
  double i;
} complex_t;

typedef struct {
  bool busy;
  point_t screen;
  point_t cursor;
  unsigned long last_moved;

  struct _state {
    int current;
    int max_levels;

    struct _settings {
      complex_t top_left;
      double X_scale;
      double Y_scale;
    } *buffer;
  } _st;
} zoom_t;

typedef struct {
  bool cmd_pressed;
} keyboard_t;

typedef struct {
  /* the width & height of the screen. We use float to retain precision in various aspect ratio
   * and scaling calculations
   */
  float screen_w; 
  float screen_h;

  /* Window height & width. Screen dimensions are not same as window dimensions, especially on
   * High DPI displays like Retina. Several graphics functions use Window coordinates. We will
   * frequently have to translate between screen & window coordinates.
   */
  float window_w;
  float window_h;

  /*
   * Introducing the "complex plain". 
   *
   * Our complex number starts with (0,0) at the center of the physical screen. We assign a complex 
   * number of our choice to be the top left of the physical screen. Since all the points that converge
   * for a mandelbrot set lie on a circle of radius 2 on the complex plain, we can choose any point that
   * lies just outside the circle.
   *
   * We (arbitrarily) chose the real part of the top left screen to be the 3. The imaginary part (i.e y axis)
   * is calculated to fit the aspect ratio of the screen. On my M1 macbook with Retina display,
   * on a 1000x700 window, the imaginary part comes to -2.1i - i.e. the top left of the screen is the number 3 - 2.1i.
   * 
   */
  complex_t top_left;


  /* this is the number of complex points per screen unit along the X-axis and Y-axis */
  double X_scale;
  double Y_scale;

  /* state mgmt for zooming and zooming out */
  zoom_t zoom;

  keyboard_t keyboard;
  
  /* graphics machinery */
  SDL_Window *window;
  SDL_Renderer *render;
  SDL_Surface *surface;
} graphics;


void screen_to_complex_plane(graphics *g, point_t screen, complex_t *z) {
  z->r = g->top_left.r + (screen.x * g->X_scale);
  z->i = g->top_left.i - (screen.y * g->Y_scale);
}
    
void dump_graphics(graphics *g) {
  int i;
  
  printf("screen width X height = %d X %d\n", (int)g->screen_w, (int)g->screen_h);
  printf("window width X height = %d X %d\n", (int)g->window_w, (int)g->window_h);
  printf("cartesian top_left = (%g, %g)\n", g->top_left.r, g->top_left.i);
  printf("X_scale = %g, Y_scale = %g\n", g->X_scale, g->Y_scale);

  for (i = 0; i < g->zoom._st.current; i++) {
    printf("zoom idx=%d top_left=(%g, %g), X_scale=%g, Y_scale=%g\n",
	   i,
	   g->zoom._st.buffer[i].top_left.r,
	   g->zoom._st.buffer[i].top_left.i,
	   g->zoom._st.buffer[i].X_scale,
	   g->zoom._st.buffer[i].Y_scale);
  }
	 
  printf("=========================\n");
}

void destroy_graphics(graphics *g) {
  free(g->zoom._st.buffer);
  
  if (g->surface)
    SDL_FreeSurface(g->surface);

  SDL_DestroyRenderer(g->render);
  SDL_DestroyWindow(g->window);
  SDL_Quit();
}

void initialize_graphics(graphics *g) {
  int w, h;

  memset(g, 0, sizeof(*g));
  
  SDL_CreateWindowAndRenderer(WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI, &g->window, &g->render);

  SDL_GetWindowSize(g->window, &w, &h);
  g->window_w = w;
  g->window_h = h;
  
  SDL_GetRendererOutputSize(g->render, &w, &h);
  g->screen_w = w;
  g->screen_h = h;

  g->top_left.r = -3.0;
  g->top_left.i = fabs(g->top_left.r) * SCREEN_ASPECT_RATIO(g);

  /* because origin is centered, total amount of X, Y available to us is 2 time abs(top left X & Y) */
  g->X_scale = fabs(g->top_left.r * 2) / g->screen_w;
  g->Y_scale = fabs(g->top_left.i * 2) / g->screen_h;

  g->surface = SDL_CreateRGBSurface(0, g->screen_w, g->screen_h, 32, 0, 0, 0, 0);

  memset(&g->zoom, 0, sizeof(zoom_t));
  g->zoom._st.current = -1;
  g->zoom._st.buffer = malloc(sizeof(struct _settings) * ZOOM_SAVE_SIZE);
  g->zoom._st.max_levels = ZOOM_SAVE_SIZE;

  g->keyboard.cmd_pressed = false;

  SDL_SetRenderDrawColor(g->render, 0, 0, 0, 255);

  RGB_colormap = build_RGB_colormap_reverse();
  RGB_grayscale = build_RGB_grayscale();
}


int naive_escape_time(complex_t c) {
  complex_t tmp = {0.0, 0.0};
  int iteration = 0;

  tmp.r = 0;
  tmp.i = 0;
  
  while (((tmp.r * tmp.r + tmp.i * tmp.i) <= 4) && iteration < ITERATION_THRESHOLD) {
    double t = tmp.r * tmp.r - tmp.i * tmp.i + c.r;
    tmp.i = 2 * tmp.r * tmp.i + c.i;
    tmp.r = t;
    iteration++;
  }
  return iteration;
}

float normalized_escape_time(complex_t c) {
  complex_t tmp = {0.0, 0.0};
  int iteration = 0;
  float mu = 0.0;

  tmp.r = 0;
  tmp.i = 0;
  
  while (((tmp.r * tmp.r + tmp.i * tmp.i) <= 4) && iteration < ITERATION_THRESHOLD) {
    double t = tmp.r * tmp.r - tmp.i * tmp.i + c.r;
    tmp.i = 2 * tmp.r * tmp.i + c.i;
    tmp.r = t;
    iteration++;
  }

  if (iteration < ITERATION_THRESHOLD) {
    mu = iteration + 1 - log(log(sqrt(tmp.r*tmp.r + tmp.i*tmp.i))/log(2.0)) / log(2.0);
    return mu/ITERATION_THRESHOLD;
  }
  return 1.0;
}


Uint32 monochrome_select_color(graphics *g, int iteration) {
  if (iteration >= ITERATION_THRESHOLD)
    return SDL_MapRGB(g->surface->format, 0xff, 0xff, 0xff);
  else
    return SDL_MapRGB(g->surface->format, 0x00, 0x00, 0x00);
}

Uint32 grayscale_select_color(graphics *g, int iteration) {
  struct _rgb color = RGB_grayscale[iteration];
  return SDL_MapRGB(g->surface->format, color.r, color.g, color.b);
}

Uint32 naive_select_color(graphics *g, int iteration) {
  struct _rgb *palette = naive_rgb_palette();
  struct _rgb color = palette[iteration];
  return SDL_MapRGB(g->surface->format, color.r, color.g, color.b);
}

Uint32 rgb_colormap_select_color(graphics *g, int iteration) {
  int scale_iter = iteration;
  struct _rgb color = RGB_colormap[scale_iter];
  return SDL_MapRGB(g->surface->format, color.r, color.g, color.b);
}


float hue[2000][1400] = {0};
int pixel_iterations[2000][1400] = {0};
int iterations_histogram[ITERATION_THRESHOLD] = {0};

void draw_mandelbrot(graphics *g) {
  Uint32 *pixels;
  int Px, Py, pos, iterations;
  complex_t z;
  point_t P;
  SDL_Texture *texture;

  
  pixels = g->surface->pixels;

  for (Py = 0; Py < g->screen_h; Py++) {
    for (Px = 0; Px < g->screen_w; Px++) {
      P.x = Px;
      P.y = Py;

      pos = SCREEN_TO_PIXEL(g, P);
      screen_to_complex_plane(g, P, &z);
      iterations = naive_escape_time(z);
      pixels[pos] = naive_select_color(g, iterations);
    }
  }

  texture = SDL_CreateTextureFromSurface(g->render, g->surface);
  SDL_RenderClear(g->render);
  SDL_RenderCopy(g->render, texture, NULL, NULL);
  SDL_DestroyTexture(texture);
  SDL_RenderPresent(g->render);
}

void draw_mandelbrot_with_histogram(graphics *g) {
  Uint32 *pixels;
  int Px, Py, pos, iterations;
  complex_t z;
  point_t P;
  SDL_Texture *texture;
  long total = 0;

  memset(hue, 0, 2000*1400*sizeof(float));
  memset(pixel_iterations, 0, 2000*1400*sizeof(int));
  memset(iterations_histogram, 0, ITERATION_THRESHOLD*sizeof(int));
  
  pixels = g->surface->pixels;

  for (Py = 0; Py < g->screen_h; Py++) {
    for (Px = 0; Px < g->screen_w; Px++) {
      P.x = Px;
      P.y = Py;

      pos = SCREEN_TO_PIXEL(g, P);
      screen_to_complex_plane(g, P, &z);
      iterations = naive_escape_time(z);
      pixel_iterations[Px][Py] = iterations;
      iterations_histogram[iterations]++;
    }
  }

  for (int i = 0; i < ITERATION_THRESHOLD; i++) {
    total += iterations_histogram[i];
  }

  for (Py = 0; Py < g->screen_h; Py++) {
    for (Px = 0; Px < g->screen_w; Px++) {
      iterations = pixel_iterations[Px][Py];
      for (int i = 0; i <= iterations; i++) {
	hue[Px][Py] += (float)iterations_histogram[i] / total;
      }
      P.x = Px;
      P.y = Py;
      pos = SCREEN_TO_PIXEL(g, P);
      pixels[pos] = naive_select_color(g, hue[Px][Py]*1000000);
    }
  }

  texture = SDL_CreateTextureFromSurface(g->render, g->surface);
  SDL_RenderClear(g->render);
  SDL_RenderCopy(g->render, texture, NULL, NULL);
  SDL_DestroyTexture(texture);
  SDL_RenderPresent(g->render);
}

void draw_mandelbrot_with_normalized_iteration(graphics *g) {
  Uint32 *pixels;
  int Px, Py, pos;
  point_t P;
  complex_t z;
  float  mu;
  SDL_Texture *texture;

  pixels = g->surface->pixels;

  for (Py = 0; Py < g->screen_h; Py++) {
    for (Px = 0; Px < g->screen_w; Px++) {
      P.x = Px;
      P.y = Py;
      
      screen_to_complex_plane(g, P, &z);
      mu = normalized_escape_time(z);

      pos = SCREEN_TO_PIXEL(g, P);
      //pixels[pos] = rgb_colormap_select_color(g, mu * 6 * 256);
      pixels[pos] = naive_select_color(g, mu * 17);
      //pixels[pos] = grayscale_select_color(g, mu * 256);
      //printf("%g\n", mu);
    }
  }

  texture = SDL_CreateTextureFromSurface(g->render, g->surface);
  SDL_RenderClear(g->render);
  SDL_RenderCopy(g->render, texture, NULL, NULL);
  SDL_DestroyTexture(texture);
  SDL_RenderPresent(g->render);
  
 }

void zoom_selection_begin(graphics *g, int window_x, int window_y) {
  g->zoom.busy = true;
  g->zoom.screen.x = WINDOW_X_TO_SCREEN_X(g, window_x);
  g->zoom.screen.y = WINDOW_Y_TO_SCREEN_Y(g, window_y);
  g->zoom.cursor.x = g->zoom.screen.x;
  g->zoom.cursor.y = g->zoom.screen.y;
}

void zoom_cursor_move(graphics *g, int window_x, int window_y) {
  if (!g->zoom.busy)
    return;
  g->zoom.last_moved = SDL_GetTicks64();
  g->zoom.cursor.x = WINDOW_X_TO_SCREEN_X(g, window_x);
  g->zoom.cursor.y = WINDOW_Y_TO_SCREEN_Y(g, window_y);
}

void zoom_draw_selection(graphics *g) {
  SDL_Rect selector;
  SDL_Texture *texture;
  
  if (!g->zoom.busy)
    return;

  /* debounce a bit */
  if ((SDL_GetTicks64() - g->zoom.last_moved) < ZOOM_REDRAW_DELAY)
    return;

  /* force aspect ratio on the user's selection. We fix height of selection rectangle to fit the
   * screen aspect ratio
   */
  selector.x = g->zoom.screen.x;
  selector.y = g->zoom.screen.y;
  selector.w = g->zoom.cursor.x - g->zoom.screen.x;
  selector.h = ceil(selector.w * SCREEN_ASPECT_RATIO(g));
  g->zoom.cursor.y = g->zoom.screen.y + selector.h;


  SDL_RenderClear(g->render);


  /* this is key, we copy over the base surface (backed by g->surface->pixels) without any
   * zoom rectangles from previous zoom_draw_selection() invocations into GPU memory and directly
   * write our new rectangle there and render everything to screen.
   */
  SDL_SetRenderDrawColor(g->render, 0xFF, 0x00, 0x00, 0xFF);
  texture = SDL_CreateTextureFromSurface(g->render, g->surface);
  SDL_RenderCopy(g->render, texture, NULL, NULL);
  SDL_DestroyTexture(texture);

  SDL_RenderDrawRect(g->render, &selector);
  SDL_RenderPresent(g->render);

  /* finally jump to the bottom right of the just rendered zoom selection rectangle. This will be
   * the X value of current cursor, but the aspect ratio corrected value on the Y axis
   */
  SDL_WarpMouseInWindow(g->window, SCREEN_X_TO_WINDOW_X(g, g->zoom.cursor.x),
			SCREEN_Y_TO_WINDOW_Y(g, g->zoom.cursor.y));
}

void zoom_in(graphics *g) {
  complex_t top_left, cursor;

  if (g->zoom._st.current + 1 == g->zoom._st.max_levels) {
    g->zoom._st.max_levels += ZOOM_SAVE_SIZE;
    g->zoom._st.buffer = realloc(g->zoom._st.buffer, sizeof(struct _settings) * (g->zoom._st.max_levels));
  }

  g->zoom._st.current++;

  g->zoom._st.buffer[g->zoom._st.current].top_left = g->top_left;
  g->zoom._st.buffer[g->zoom._st.current].X_scale = g->X_scale;
  g->zoom._st.buffer[g->zoom._st.current].Y_scale = g->Y_scale;
  
  screen_to_complex_plane(g, g->zoom.screen, &top_left);
  screen_to_complex_plane(g, g->zoom.cursor, &cursor);
  g->top_left.r = top_left.r;
  g->top_left.i = top_left.i;
  
  g->X_scale = fabs(cursor.r - top_left.r) / g->screen_w;
  g->Y_scale = fabs(cursor.i - top_left.i) / g->screen_h;

  g->zoom.busy = false;
}

void zoom_out(graphics *g) {
  if (g->zoom._st.current == -1)
    return;

  g->top_left = g->zoom._st.buffer[g->zoom._st.current].top_left;
  g->X_scale = g->zoom._st.buffer[g->zoom._st.current].X_scale;
  g->Y_scale = g->zoom._st.buffer[g->zoom._st.current].Y_scale;
  
  g->zoom._st.current--;
}


bool keyboard_process(graphics *g, Uint32 event_type, Uint8 scancode) {
  int undo_pressed = false;

  if (event_type == SDL_KEYDOWN) {
    if ((scancode == SDL_SCANCODE_LGUI) || (scancode == SDL_SCANCODE_RGUI)) 
      g->keyboard.cmd_pressed = true;
    else if ((scancode == SDL_SCANCODE_Z) && g->keyboard.cmd_pressed)
      undo_pressed = true;
    else
      undo_pressed = false;
  }

  if (event_type == SDL_KEYUP) {
    if ((scancode == SDL_SCANCODE_LGUI) || (scancode == SDL_SCANCODE_RGUI))
      g->keyboard.cmd_pressed = false;
  }

  return undo_pressed;
}

int main(void) {
  bool quit = false, must_redraw = false;
  graphics g;
  SDL_Event e;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("Failed to init video: %s\n", SDL_GetError());
  }

  initialize_graphics(&g);
  dump_graphics(&g);

  draw_mandelbrot_with_normalized_iteration(&g);

  while (!quit) {
    while(SDL_PollEvent(&e) != 0) {
      switch (e.type) {

      case SDL_QUIT:
	quit = true;
	break;

      case SDL_MOUSEBUTTONDOWN:
	zoom_selection_begin(&g, e.button.x, e.button.y);
	break;

      case SDL_MOUSEMOTION:
	zoom_cursor_move(&g, e.button.x, e.button.y);
	break;

      case SDL_MOUSEBUTTONUP:
	zoom_in(&g);
	must_redraw = true;
	break;

      case SDL_KEYDOWN:
	if (keyboard_process(&g, SDL_KEYDOWN, e.key.keysym.scancode) == true) {
	  zoom_out(&g);
	  must_redraw = true;
	}
	break;

      case SDL_KEYUP:
	keyboard_process(&g, SDL_KEYUP, e.key.keysym.scancode);
	break;
	
      default:
	break;
      }
    }

    zoom_draw_selection(&g);

    if (must_redraw) {
      must_redraw = false;
      draw_mandelbrot_with_normalized_iteration(&g);
      dump_graphics(&g);
    }
      
    SDL_Delay(10);
  }
  
  destroy_graphics(&g);
  return 0;
}
 

