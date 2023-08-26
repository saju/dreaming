#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define ZOOM_FACTOR 2

typedef struct {
  double x;
  double y;
} point;

typedef struct {
  double r;
  double i;
} complex;

typedef struct {
  /* the width of the screen. this does not change after startup. maps to screen X-axis */
  int screen_w; 

  /* the height of the screen. this does not change after startup. maps to screen Y-axis */
  int screen_h;

  /* Window height & width. Screen dimensions are not same as window dimensions, especially on
   * High DPI displays like Retina
   */
  int window_w;
  int window_h;

  /*
   * the left top edge of our cartesian coordinate system. This changes on origin translations.
   * note that "screens/displays" have origin at top left and have positive XY plain going downwards
   * i.e like a regular cartesian XY plain rotated by 180 degrees on the Z axis!
   *
   * The top left of the display is always 0,0 in screen cordinates. Knowing this and the screen width
   * & height and the fact that our screen is a rectangle and that we always place the origin of our
   * cartesian plane at the center of the display screen allows us to translate between cartesian and
   * screen coordinates easily.
   */
  point top_left;

  /* needed to recompute the length of the x & y line segments - which in turn is needed for
     calculating X & Y scale */
  point bottom_right;

  double X_scale;
  double Y_scale;

  unsigned int iteration;
  
  SDL_Window *window;
  SDL_Renderer *render;
  SDL_Surface *surface;
} graphics;


/*
 * we will initialize our cartesian plain with (-3,2) on top left. i.e (-3,2) is (0,0) in screen
 * coordinates. Cartesian origin (0,0) will be at screen coordinates (screen_width/2, screen_height/2) 
 */
void initialize_cartesian(graphics *g) {
  SDL_GetWindowSize(g->window, &g->window_w, &g->window_h);
  SDL_GetRendererOutputSize(g->render, &g->screen_w, &g->screen_h);
  
  g->top_left.x = -3.0;
  g->top_left.y = 2.0;
  g->bottom_right.x = 3.0;
  g->bottom_right.y = -2.0;

  g->X_scale = fabs((g->bottom_right.x - g->top_left.x)) / g->screen_w;
  g->Y_scale = fabs((g->bottom_right.y - g->top_left.y)) / g->screen_h;

  g->iteration = 0;
}

int upscale_Sx_to_Px(graphics *g, int Sx) {
  return Sx * (g->screen_w / g->window_w);
}

int upscale_Sy_to_Py(graphics *g, int Sy) {
  return Sy * (g->screen_h / g->window_h);
}

void zoom(graphics *g, int Sx0, int Sy0, int Sx1, int Sy1) {
  int Px0 = upscale_Sx_to_Px(g, Sx0);
  int Py0 = upscale_Sy_to_Py(g, Sy0);
  int Px1 = upscale_Sx_to_Px(g, Sx1);
  int Py1 = upscale_Sy_to_Py(g, Sy1);
  double x0, y0, x1, y1;

  x0 = g->top_left.x + (Px0 * g->X_scale);
  y0 = g->top_left.y - (Py0 * g->Y_scale);
  x1 = g->top_left.x + (Px1 * g->X_scale);
  y1 = g->top_left.y - (Py1 * g->Y_scale);

  g->top_left.x = x0;
  g->top_left.y = y0;
  g->bottom_right.x = x1;
  g->bottom_right.y = y1;


  printf("zoom: Screen (X0, Y0) => (%d, %d)\n", Sx0, Sy0);
  printf("zoom: Screen (X1, Y1) => (%d, %d)\n", Sx1, Py1);
  printf("zoom: Pixel (X0, Y0) => (%d, %d)\n", Px0, Py0);
  printf("zoom: Pixel (X1, Y1) => (%d, %d)\n", Px1, Py1);
  printf("zoom: Cart (X0, Y0) => (%f, %f)\n", x0, y0);
  printf("zoom: Cart (X1, Y1) => (%f, %f)\n", x1, y1);

  g->X_scale =  fabs((g->bottom_right.x - g->top_left.x)) / g->screen_w;
  g->Y_scale = fabs((g->bottom_right.y - g->top_left.y)) / g->screen_h;
}
  
  
  

void dump_graphics(graphics *g) {
  printf("screen width X height = %dx%d\n", g->screen_w, g->screen_h);
  printf("window width X height = %dx%d\n", g->window_w, g->window_h);
  printf("cartesian top_left = (%f, %f)\n", g->top_left.x, g->top_left.y);
  printf("cartesian bottom_right = (%f, %f)\n", g->bottom_right.x, g->bottom_right.y);
  printf("X_scale = %f, Y_scale = %f\n", g->X_scale, g->Y_scale);
  printf("Iteration %d\n", g->iteration);
  printf("=========================\n");
  
}


int naive_escape_time(complex c) {
  complex tmp;
  int iteration = 0;

  tmp.r = 0;
  tmp.i = 0;
  
  while (((tmp.r * tmp.r + tmp.i * tmp.i) <= 4) && iteration < 800) {
    double t = tmp.r * tmp.r - tmp.i * tmp.i + c.r;
    tmp.i = 2 * tmp.r * tmp.i + c.i;
    tmp.r = t;
    iteration++;
  }

  return iteration;
}



void draw_mandelbrot2(graphics *g) {
  complex *z;
  double X_scale, Y_scale;
  SDL_Texture *texture;
  Uint32 *pixels;

  z = malloc(sizeof(*z) * g->screen_w * g->screen_h); // don't need to store
  pixels = g->surface->pixels;
  
  for (int Py = 0; Py < g->screen_h; Py++) {
    for (int Px = 0; Px < g->screen_w; Px++) {
      int iterations = 0;
      int pos = Px + (Py * g->screen_w);

      z[pos].r = g->top_left.x + (Px * g->X_scale);
      z[pos].i = g->top_left.y - (Py * g->Y_scale);

      iterations = naive_escape_time(z[pos]);
      if (iterations >= 800)
	pixels[pos] = SDL_MapRGB(g->surface->format, 255, 255, 255);
      else
	pixels[pos] = SDL_MapRGB(g->surface->format, 0, 0, 0);
    }
  }
  free(z);
  texture = SDL_CreateTextureFromSurface(g->render, g->surface);
  SDL_RenderClear(g->render);
  SDL_RenderCopy(g->render, texture, NULL, NULL);
  SDL_DestroyTexture(texture);
  SDL_RenderPresent(g->render);
}


int main(void)
{
    SDL_Window      *win = NULL;
    SDL_Renderer    *ren = NULL;
    SDL_Surface     *sfc = NULL;
    SDL_Event e;
    graphics g;
    bool quit = false, must_redraw = false, zoom_session = false;
    int zoom_session_x0, zoom_session_y0, zoom_session_x1, zoom_session_y1;


    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_CreateWindowAndRenderer(1000, 700, SDL_WINDOW_ALLOW_HIGHDPI, &win, &ren);

    g.window = win;
    g.render = ren;
    
    initialize_cartesian(&g);

    sfc = SDL_CreateRGBSurface(0, g.screen_w, g.screen_h, 32, 0, 0, 0, 0);
    if (!sfc) {
      printf("could not create RGB surface: %s\n", SDL_GetError());
      exit(2);
    }

    g.surface = sfc;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);

    dump_graphics(&g);
    draw_mandelbrot2(&g);

    
    while(!quit) {
	while(SDL_PollEvent(&e) != 0) {
	  switch (e.type) {
	  case SDL_QUIT:
	    quit = true;
	    printf("received quit\n");
	    break;
	  case SDL_MOUSEBUTTONDOWN:
	    zoom_session = true;
	    zoom_session_x0 = e.button.x;
	    zoom_session_y0 = e.button.y;
	    break;
	  case SDL_MOUSEBUTTONUP:
	    if (zoom_session) {
	      zoom_session = false;
	      zoom_session_x1 = e.button.x;
	      zoom_session_y1 = e.button.y;
	      printf("zoom area (%d, %d) - (%d, %d)\n", zoom_session_x0, zoom_session_y0,
		     zoom_session_x1, zoom_session_y1);
	      zoom(&g, zoom_session_x0, zoom_session_y0, zoom_session_x1, zoom_session_y1);
	      must_redraw = true;
	    }
	    break;
	  default:
	    break;
	  }
	}

	if (must_redraw) {
	  draw_mandelbrot2(&g);
	  dump_graphics(&g);
	  must_redraw = false;
	}
    }


    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return (0);
}




