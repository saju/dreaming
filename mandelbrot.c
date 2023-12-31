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

palette_t *uf_palette, *RGB_grayscale_palette;

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

  int worker_count;
  
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

  /* XXX: free these */
  RGB_grayscale_palette = grayscale_RGB_palette();
  uf_palette = uf_rgb_palette();

  g->worker_count = init_workers();
}


Uint32 monochrome_select_color(graphics *g, int iteration) {
  if (iteration >= ITERATION_THRESHOLD)
    return SDL_MapRGB(g->surface->format, 0xff, 0xff, 0xff);
  else
    return SDL_MapRGB(g->surface->format, 0x00, 0x00, 0x00);
}

Uint32 uf_select_color(graphics *g, float iteration){
  struct _rgb color;

  if (iteration == 1.0) {
    return SDL_MapRGB(g->surface->format, 0x00, 0x00, 0x00); 
  } else {
    color = uf_palette->colors[(int)(iteration * uf_palette->size)];
    return SDL_MapRGB(g->surface->format, color.r, color.g, color.b);
  }
}

Uint32 grayscale_select_color(graphics *g, float iteration) {
  struct _rgb color = RGB_grayscale_palette->colors[(int)(iteration * RGB_grayscale_palette->size)];
  return SDL_MapRGB(g->surface->format, color.r, color.g, color.b);
}

/*
 * For f(z) = z^2 + c with z & c defined in the complex plain, compute if the sequence
 * f(f(f....(f(0))...)) diverges to infinity. If |z| > 2, the series will definitely
 * escape to infinity. A `c` for which the sequence does not diverge after ITERATION_THRESHOLD
 * applications is considered as "not diverging" and is part of the mandebrot set.
 *
 * Iteration count to escape (or threshold) for `c` determines the color of the corresponding
 * pixel on screen.
 *
 * We normalize the iteration count for obtaining smooth color transition.
 * Theory and details from: http://linas.org/art-gallery/escape/smooth.html
 */
float normalized_escape_time(complex_t c) {
  int iteration = 0;
  float mu = 0.0;
  double r0 = c.r, i0 = c.i;
  double r = 0, i = 0, r2 = 0, i2 = 0;

#pragma clang loop vectorize(enable)
  while (r2 + i2 <= 4 && iteration < ITERATION_THRESHOLD) {
    i = (r + r) * i + i0;
    r = r2 - i2 + r0;
    r2 = r * r;
    i2 = i * i;
    iteration++;
  }
  
  if (iteration < ITERATION_THRESHOLD) {
    mu = iteration + 1 - log(log(sqrt(r2 + i2))/log(2.0)) / log(2.0);
    return mu/ITERATION_THRESHOLD;
  } else 
    return 1.0;
}


void _compute(graphics *g, int y_start, int y_end) {
  int x, y, pos;
  point_t p;
  complex_t z;
  float mu;
  Uint32 *pixels = g->surface->pixels;

#pragma clang loop vectorize(enable) 
  for (y = y_start; y < y_end; y++) {
    for (x = 0; x < g->screen_w; x++) {
      p.x = x;
      p.y = y;

      screen_to_complex_plane(g, p, &z);
      mu = normalized_escape_time(z);

      pos = SCREEN_TO_PIXEL(g, p);
      pixels[pos] = uf_select_color(g, mu);
    }
  }
}


void *chunk_escape_time(void *arg) {
  worker_ctx_t *ctx = (worker_ctx_t *)arg;
  graphics *g = (graphics *)ctx->args;
  int chunk = g->screen_h/g->worker_count;

  _compute(g, ctx->worker * chunk, (ctx->worker + 1) * chunk);
  return NULL;
}

/*
 * break the screen up along the height (think rasters) and have each worker do one "raster".
 */
void draw_mandelbrot(graphics *g) {
  uint64_t then;
  SDL_Texture *texture;
  int leftovers;

  then = SDL_GetTicks64();

  worker_run(chunk_escape_time, g);

  leftovers = (int)g->screen_h % g->worker_count;
  if (leftovers) {
    _compute(g, g->screen_h - leftovers, g->screen_h);
  }

  printf("computations took %" PRIu64 "ms\n", SDL_GetTicks64() - then);

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


  /* this is key; we copy over the base surface (backed by g->surface->pixels) without any
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

  draw_mandelbrot(&g);

  while (!quit) {
    while(SDL_PollEvent(&e) != 0) {
      switch (e.type) {

      case SDL_QUIT:
	goto byebye;
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
      draw_mandelbrot(&g);
      dump_graphics(&g);
    }
      
    SDL_Delay(10);
  }

 byebye:
  destroy_graphics(&g);
  return 0;
}
 

