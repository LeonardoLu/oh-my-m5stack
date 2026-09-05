# BotUx host preview

This small host-only harness compiles the real `BotUx.cpp` against an SVG-emitting
`M5Canvas` stand-in. It exercises `update()` at 60 Hz, calls the actual `draw()`,
and produces mood contact sheets at 40, 72, and 200 px integration sizes.

```sh
./render.sh
```

The generated `tmp/botux-preview/` directory is disposable. The preview proves layout and
primitive-call behavior; it does not claim pixel-perfect M5GFX rasterization or
hardware performance.
