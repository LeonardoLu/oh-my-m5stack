# bot-ux — component notes for agents

The shared animation component. Public API is `src/BotUx.h` (source of truth);
implementation is `src/BotUx.cpp`.

## Rules

- The component renders into an `M5Canvas*`; it **never** owns a display, calls
  `M5.update()`, or pushes pixels. Hosts do that.
- All timing is `millis()`-based and framerate-agnostic. `update()` computes the
  per-frame state; `draw()` only renders. Never allocate in `draw()`.
- Keep drawing to M5GFX primitives (`fillCircle`, `fillEllipse`, `fillArc`,
  `fillRoundRect`, `fillTriangle`, `drawLine`, `drawString`, …).
- Arc angle convention (LovyanGFX, verified): **0° = right (3 o'clock), clockwise,
  90° = down, 180° = left, 270° = up**. Happy crescent eyes use the top
  arc (`180..360`).
- `Style` colors are RGB565 (`botux::rgb565`). Passing `uint16_t` to M5GFX's
  `uint32_t` color params is fine.

## When to touch this

- New mood/expression → add a case in `_resolveMood`, then handle silhouette
  replacement in `_drawBody` or eye treatment in `_drawEyes` if needed.
- New theme → add a preset in the device apps, not here (this stays generic).
- API change → update the header, keep it additive so device apps don't break.
