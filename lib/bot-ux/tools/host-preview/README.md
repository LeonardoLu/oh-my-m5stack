# BotUx host preview

This small host-only harness compiles the real `BotUx.cpp` against an SVG-emitting
`M5Canvas` stand-in. It calls the actual `update()` and `draw()` paths and
produces mood sheets at 40, 72, and 200 px plus expression and animation pickers.
It also writes an interactive `animation-player.html`: each mode contains 60
frames sampled at 15 fps from the real C++ component. JavaScript only selects
and plays those frames; it does not substitute a CSS animation.

```sh
./render.sh
```

The generated `tmp/botux-preview/` directory is disposable. Assertions exercise
visibly distinct choices, direct and eased pose changes, normalized motion
input, reduced motion, and updates as sparse as 3 fps. The preview proves layout
and primitive-call behavior; it does not claim pixel-perfect M5GFX font or shape
rasterization, display transfer timing, or hardware performance.

`transitions.svg` shows eight successive 100 ms frames of Neutral → Joy and
Neutral → Wink. Pixel/rectangle drawing also maintains an RGB565 buffer, allowing
coverage-color and face-placement checks at 72, 200, and 310 px. SVG pixel
rectangles use `crispEdges` so a browser does not add its own antialiasing or
seams between adjacent scanlines. Native circle/triangle/round-rectangle calls
still use SVG approximations; this harness is not a replacement for M5GFX raster
capture or device frame-time measurements.
