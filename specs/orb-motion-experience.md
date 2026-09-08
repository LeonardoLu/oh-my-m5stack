# Thinking and Working orb motion — 2026-09-09

This iteration replaces the earlier three-dot Thinking glyph and the
anthropomorphic Working face with two related volumetric point-shell states.
Both states replace the avatar silhouette and eye overlay. The Watch and Core2
continue to supply the existing BotUx style; `bodyColor` colors the points and
`bgColor` remains the field behind them. The reference pill and label belong to
the surrounding product UI and are not drawn by BotUx.

## Motion language

The visual references are the user-selected MetalForge Thinking Orbs
[Breathe](https://metalforge.xyz/editor#effect=thinking-orbs&style=breathe&speed=1&reverse=0&phase=0&spin=0&yaw=0&pitch=0&dots=1&spread=1&dotScale=1&perspective=1&depthSize=1&depthFade=1&opacity=1&showsPill=1&showsLabel=1&labelScale=1&pillPad=1&pillGap=1&accent=%23E8853C&dotColor=%23F4F1EA&pill=%231B1B1D&labelColor=%23F4F1EA&dotColorLight=%2325242A&pillLight=%23ECECEF&labelColorLight=%2325242A)
and
[Vortex](https://metalforge.xyz/editor#effect=thinking-orbs&style=vortex&speed=1&reverse=0&phase=0&spin=0&yaw=0&pitch=0&dots=1&spread=1&dotScale=1&perspective=1&depthSize=1&depthFade=1&opacity=1&showsPill=1&showsLabel=1&labelScale=1&pillPad=1&pillGap=1&accent=%23E8853C&dotColor=%23F4F1EA&pill=%231B1B1D&labelColor=%23F4F1EA&dotColorLight=%2325242A&pillLight=%23ECECEF&labelColorLight=%2325242A)
styles with default controls. The
public catalog and shipped browser metadata describe Breathe as a 3.6-second
point shell whose inflation travels between latitudes, and Vortex as a
4.4-second surface drain whose points move along meridians toward the north
pole, fade out, re-enter at the bottom, and counter-rotate. The public preview
shows many small depth-shaded points forming a tilted sphere. The M5GFX version
adapts that behavior to native primitives; it does not copy a shader or draw
the reference status pill.

Thinking distributes points over a sphere and applies a subtle delayed radial
swell at each latitude while the shell turns. It should read as coordinated,
deliberate breathing rather than three independent loading dots.

Working continuously advances points from the bottom to the top along sparse
diagonal meridian streams. Points fade near both poles so the wrap has no flash,
and the shell turns in the opposing direction. Open lanes and a ring-like
negative space distinguish it from Thinking even though both share the same
dot material and depth model.

## Native rendering contract

- Particle count is fixed for a given size: 48 below a 34 px body radius, 72
  below 60 px, and 96 at larger sizes.
- Particle state uses a fixed 96-element stack array (about 2 KiB with the
  current compiler layout). Drawing performs no heap allocation and creates no
  secondary framebuffer.
- Each point is an existing zero-length capsule. Its distance-field edge blends
  against the actual RGB565 pixel already under it, so overlapping points do
  not cut background-colored fringes into one another.
- A fixed tilt, perspective projection, depth-scaled radius, and depth fade
  establish volume. Back points draw before front points.
- `Animation::Auto` uses the shell's own motion without the legacy Orbit translation.
  An explicitly selected Animation still translates the complete shell through
  the existing choreography layer.
- `setAnimationSpeed()` scales phase. Motion amount progressively scales shell
  travel and the Thinking deformation; zero freezes every particle. Reduced
  motion applies a 0.20 multiplier to both phase travel and deformation.

## Host validation

`lib/bot-ux/tools/host-preview/test-orb-motion.sh` compiles a separate C++11
focused test against the RGB565 host canvas. It checks both moods at 40, 72,
178 and 286 px, including in-bounds drawing, absence of eye ink, populated
point clusters, speed equivalence, distinct silhouettes, low/reduced/zero
motion behavior, exact 3.6/4.4-second periodicity, adjacent phase-wrap
continuity, and Vortex pole re-entry. It also checks that explicit expressions
cannot restore eyes and that explicit animation translation remains visible.

## Validation result

The focused orb runner passed on the frozen shared renderer. At 200 px,
Thinking contained 562 non-background pixels in 89 connected clusters and
Working contained 481 pixels in 85 clusters. Its 180 ms full/reduced/0.1-motion
frame deltas were 1,082/981/811 and 942/865/691 pixels respectively; zero amount
was identical. Both exact period wraps, native target sizes, speed equivalence,
eye suppression, explicit-expression suppression and explicit-animation
composition passed.

The complete `tools/check_host.sh` gate passed, including the shared BotUx
preview and Watch/Core2 host contracts. The final Core2 PlatformIO build used
60,464 B RAM and 1,510,809 B flash. The coordinated Watch build used 48,928 B
RAM and 1,061,825 B flash; its firmware image was 1,062,192 B. No serial upload
or physical-device acceptance was attempted.

The native catalog was regenerated from the final C++ renderer with 32 entries:
14 moods, 10 expressions and 8 animations. The checked-in Thinking and Working
GIFs contain 72 RGB frames at 120 px. Six-frame full-cycle comparisons showed
Thinking's evenly populated breathing shell and Working's moving negative band
and diagonal/ring streams; 286 px host captures retained visible front points
and depth fade. These captures were visually accepted against the authenticated
reference. They remain host raster evidence, not device frame-time, optical,
touch, Bluetooth, or serial acceptance.
