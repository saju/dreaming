A recursive Mandelbrot fractal generator for Macbooks in C using SDL for graphics. 

Currently:
* Basic "zoom in" available by selecting a area using the touchpad ( but no "zoom out" available).
* "Zoom In" preserves aspect ratio.
* Simple 16 color palette. No smoothing, lots of ugly banding.
* Uses naive escape time calculations (will eat CPU)
* Depends on platform resolution of float/double. "Infinite precision" to come later.

Some sample fractal images rendered by this generator:

![plot](./sample/image1.png)
![plot](./sample/image2.png)
![plot](./sample/image3.png)
