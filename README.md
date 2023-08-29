A recursive Mandelbrot fractal generator for Macbooks in C using SDL for graphics. 

Currently:
* Basic "zoom in" available by selecting a area using the touchpad. Zoom out via CMD-z (osx undo)
* "Zoom In" preserves aspect ratio.
* A simple color palette as well as RGB grayscale palette available
* Uses naive escape time calculations with iteration normalization for color smoothing
* Depends on platform resolution of float/double. "Infinite precision" to come later.
* Only tested on a M1 Macbook laptop screen.

INSTALL:
* You need a c99 C complier. You can get clang from Xcode Command Line Tools.
* You need make (or you can copy the build line from the makefile)
* git clone this repo; cd into_repo; make; ./a.out

Some sample fractal images rendered by this generator:

![plot](./sample/image1.png)
![plot](./sample/image2.png)
![plot](./sample/image3.png)
![plot](./sample/image4.png)
