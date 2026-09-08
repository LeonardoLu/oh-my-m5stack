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
styles with default controls. The reference catalog and browser metadata show
many small depth-shaded points forming a tilted sphere. They describe Breathe
as a 3.6-second point shell whose inflation travels between latitudes, and
Vortex as a 4.4-second surface drain whose points move along meridians toward
the north pole, fade out, re-enter at the bottom, and counter-rotate. The M5GFX version
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

## StopWatch legibility and timing tune

The initial native implementation used 3.6- and 4.4-second base periods,
48/72/96 points, and `max(0.55 px, 0.0086R)` for the base point radius. The
StopWatch multiplies phase by both animation speed and motion amount. With its
verified saved level-two settings (`speed=0.73`, `amount=0.55`), those original
periods computed to 8.97 and 10.96 seconds. The audited device also had wrist
motion disabled; an app coupling incorrectly enabled reduced motion too, making
the computed periods 44.83 and 54.79 seconds. These values are calculated from
the code and read-only settings audit, not optically measured periods.

The tuned base periods are 1.6 seconds for Thinking and 1.4 seconds for Working.
The same verified level-two settings now compute to 3.99 and 3.49 seconds. The
StopWatch separately removes the wrist/reduced-motion coupling: wrist sensing
only gates IMU input, while the shared explicit reduced-motion API remains
available. Explicit reduced motion still applies its 0.20 phase multiplier.

Point radius is now `max(0.70 px, 0.014R)`, while counts fall to 32/48/72. The
larger material roughly doubles average cluster ink at the 286 px Watch profile;
the lower count preserves sparse negative space and reduces projection count
and point-draw calls.

## Native rendering contract

- Particle count is fixed for a given size: 32 below a 34 px body radius, 48
  below 60 px, and 72 at larger sizes.
- Particle state uses a fixed 72-element stack array (about 1,440 B with the
  current compiler layout, down from 1,920 B). Drawing performs no heap allocation and creates no
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
motion behavior, exact 1.6/1.4-second base periodicity, adjacent phase-wrap
continuity, and Vortex pole re-entry. It also checks the level-two StopWatch
profile, that explicit expressions cannot restore eyes, and that explicit
animation translation remains visible. Sixteen native 120 px frames cover one
computed StopWatch-profile cycle for each mood.

## Validation result

The focused orb runner passed on the frozen shared renderer. At the 286 px
StopWatch profile, Thinking changed from 796 pixels in 90 clusters (8.84 pixels
per cluster) to 1,231 pixels in 70 clusters (17.59 per cluster). Working changed
from 708 pixels in 90 clusters (7.87 per cluster) to 1,033 pixels in 65 clusters
(15.89 per cluster); its bounding-box occupancy remains a sparse 3.24%. Point
count is 75% of the earlier large-shell count, while AA ink rises by 55% and 46%.
Both exact period wraps, native target sizes, speed equivalence, eye suppression,
explicit-expression suppression and explicit-animation composition passed.

The complete `tools/check_host.sh` gate passed, including the Watch and Core2
host contracts, shared BotUx preview, and focused orb suite. The final Core2
PlatformIO build used 60,464 B RAM and 1,510,801 B flash. The coordinated Watch
build used 48,928 B RAM and 1,061,789 B flash; its firmware image was 1,062,160 B
with SHA-256
`8af0d34570b6578bbe4bcea7a760e06598d24d71aac984bca578ff28914ab930`.
On the deployed Watch, native 466 px captures visually confirmed larger points
and an open Working band. Thinking sustained 40.3–40.4 FPS and Working 39.1 FPS,
compared with 41.4 and 38.7 FPS before the tune. Firmware hash writes were
verified and the device was restored to automatic mode after capture.

The native catalog was regenerated from the final C++ renderer with 32 entries:
14 moods, 10 expressions and 8 animations. The checked-in Thinking and Working
GIFs contain 72 RGB frames at 120 px. Six-frame full-cycle comparisons showed
Thinking's evenly populated breathing shell and Working's moving negative band
and diagonal/ring streams; 286 px host captures retained visible front points
and depth fade. Tuned full-cycle Watch-profile evidence is in
`tmp/botux-orb-tune-after/watch-thinking-120.gif` and
`watch-working-120.gif`, with six-frame sheets and a 286 px before/after
comparison in the same directory. These captures were visually accepted. They
remain host raster evidence, not device frame-time, optical, touch, Bluetooth,
or serial acceptance.
