#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define WIN_WIDTH  1000
#define WIN_HEIGHT 700

#define ITERATION_THRESHOLD 800
#define ZOOM_REDRAW_DELAY 100

#define GRAPHICS_LOOP_SLEEP 10

#define SCREEN_ASPECT_RATIO(g) (g->screen_h/g->screen_w)

/* go from 2D screen/renderer coordinates to 1D array of pixels */
#define SCREEN_TO_PIXEL(g, P) (P.x + (P.y * g->screen_w))

#define WINDOW_X_TO_SCREEN_X(g, winx) ((winx) * g->screen_w/g->window_w)
#define WINDOW_Y_TO_SCREEN_Y(g, winy) ((winy) * g->screen_h/g->window_h)
#define SCREEN_X_TO_WINDOW_X(g, sx) ((sx) * g->window_w/g->screen_w)
#define SCREEN_Y_TO_WINDOW_Y(g, sy) ((sy) * g->window_h/g->screen_h)

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
} zoom_t;

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


  
  /* machinery */
  SDL_Window *window;
  SDL_Renderer *render;
  SDL_Surface *surface;
} graphics;


void screen_to_complex_plain(graphics *g, point_t screen, complex_t *z) {
  z->r = g->top_left.r + (screen.x * g->X_scale);
  z->i = g->top_left.i - (screen.y * g->Y_scale);
}
    
void dump_graphics(graphics *g) {
  printf("screen width X height = %d X %d\n", (int)g->screen_w, (int)g->screen_h);
  printf("window width X height = %d X %d\n", (int)g->window_w, (int)g->window_h);
  printf("cartesian top_left = (%g, %g)\n", g->top_left.r, g->top_left.i);
  printf("X_scale = %g, Y_scale = %g\n", g->X_scale, g->Y_scale);
  printf("=========================\n");
  
}

void destroy_graphics(graphics *g) {
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

  SDL_SetRenderDrawColor(g->render, 0, 0, 0, 255);
}


int naive_escape_time(complex_t c) {
  complex_t tmp;
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
      screen_to_complex_plain(g, P, &z);

      iterations = naive_escape_time(z);

      if (iterations >= ITERATION_THRESHOLD)
	pixels[pos] = SDL_MapRGB(g->surface->format, 255, 255, 255);
      else
	pixels[pos] = SDL_MapRGB(g->surface->format, 0, 0, 0);
    }
  }

  texture = SDL_CreateTextureFromSurface(g->render, g->surface);
  SDL_RenderClear(g->render);
  SDL_RenderCopy(g->render, texture, NULL, NULL);
  SDL_DestroyTexture(texture);
  SDL_RenderPresent(g->render);
}

void zoom_selection_begin(graphics *g, zoom_t *zoom, int window_x, int window_y) {
  zoom->busy = true;
  zoom->screen.x = WINDOW_X_TO_SCREEN_X(g, window_x);
  zoom->screen.y = WINDOW_Y_TO_SCREEN_Y(g, window_y);
  zoom->cursor.x = zoom->screen.x;
  zoom->cursor.y = zoom->screen.y;
}

void zoom_cursor_move(graphics *g, zoom_t *zoom, int window_x, int window_y) {
  zoom->last_moved = SDL_GetTicks64();
  zoom->cursor.x = WINDOW_X_TO_SCREEN_X(g, window_x);
  zoom->cursor.y = WINDOW_Y_TO_SCREEN_Y(g, window_y);
}

void zoom_draw_selection(graphics *g, zoom_t *zoom) {
  SDL_Rect selector;
  SDL_Texture *texture;
  
  if (!zoom->busy)
    return;

  /* debounce a bit */
  if ((SDL_GetTicks64() - zoom->last_moved) < ZOOM_REDRAW_DELAY)
    return;

  /* force aspect ratio on the user's selection. We fix height of selection rectangle to fit the
   * screen aspect ratio
   */
  selector.x = zoom->screen.x;
  selector.y = zoom->screen.y;
  selector.w = zoom->cursor.x - zoom->screen.x;
  selector.h = ceil(selector.w * SCREEN_ASPECT_RATIO(g));
  zoom->cursor.y = zoom->screen.y + selector.h;

  // printf("screen=(%f, %f), cursor=(%f, %f), w=%d, h=%d\n", zoom->screen.x, zoom->screen.y, zoom->cursor.x, zoom->cursor.y, selector.w, selector.h);

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
  SDL_WarpMouseInWindow(g->window, SCREEN_X_TO_WINDOW_X(g, zoom->cursor.x),
			SCREEN_Y_TO_WINDOW_Y(g, zoom->cursor.y));
}

void rescale_screen(graphics *g, zoom_t *zoom) {
  complex_t top_left, cursor;

  screen_to_complex_plain(g, zoom->screen, &top_left);
  screen_to_complex_plain(g, zoom->cursor, &cursor);
  g->top_left.r = top_left.r;
  g->top_left.i = top_left.i;

  g->X_scale = fabs(cursor.r - top_left.r) / g->screen_w;
  g->Y_scale = fabs(cursor.i - top_left.i) / g->screen_h;
}

int main(void) {
  bool quit = false, must_redraw = false;
  graphics g;
  SDL_Event e;
  zoom_t zoom;
  
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
	quit = true;
	break;

      case SDL_MOUSEBUTTONDOWN:
	zoom_selection_begin(&g, &zoom, e.button.x, e.button.y);
	break;

      case SDL_MOUSEMOTION:
	if (zoom.busy)
	  zoom_cursor_move(&g, &zoom, e.button.x, e.button.y);
	break;

      case SDL_MOUSEBUTTONUP:
	zoom.busy = false;
	rescale_screen(&g, &zoom);
	must_redraw = true;

	break;
	
      default:
	break;
      }
    }

    zoom_draw_selection(&g, &zoom);

    if (must_redraw) {
      must_redraw = false;
      draw_mandelbrot(&g);
      dump_graphics(&g);
    }
      
    SDL_Delay(10);
  }
  
  destroy_graphics(&g);
  return 0;
}
 

