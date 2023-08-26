all:
	clang -Wl,-sectcreate,__TEXT,__info_plist,dreaming.plist -I /opt/homebrew/Cellar/sdl2/2.28.2/include -L /opt/homebrew/Cellar/sdl2/2.28.2/lib -lsdl2 mandelbrot.c

