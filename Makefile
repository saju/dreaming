TARGET = mandelbrot
CC = clang
LINKER = clang
CFLAGS = -Wall -fno-math-errno -I /opt/homebrew/Cellar/sdl2/2.28.2/include

ifdef IMPRECISE
CFLAGS += -ffast-math
endif

LFLAGS = -Wl,-sectcreate,__TEXT,__info_plist,dreaming.plist -L /opt/homebrew/Cellar/sdl2/2.28.2/lib -lsdl2
SOURCES = mandelbrot.c color.c
OBJECTS=$(SOURCES:.c=.o )

all:$(TARGET)

$(TARGET): $(OBJECTS)
	$(LINKER) $(OBJECTS) $(LFLAGS) -o $@

$(OBJECTS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(TARGET) *.o



