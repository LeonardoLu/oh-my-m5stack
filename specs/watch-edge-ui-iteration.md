# StopWatch round-edge controls

Status: source implementation and focused host validation complete; final firmware
build, device capture and finger-on-glass acceptance belong to the Watch integration
owner.

## Completion footer

The visible Done/Back footer is the same geometry used for hit testing. It occupies
the bottom circular segment `y=406..465` of the 466 px canvas, clipped to the
mathematical display circle centered at `(233,233)` with radius 233. Its label stays
centered at `(233,432)`. The 60 px vertical extent is about 5.72 mm on the documented
1.75 inch panel. It remains attached to the physical bottom edge rather than becoming
a detached pill or an invisible enlarged target.

`WatchControls.h` exposes `doneBounds()`, `doneRowSpan(y)` and
`doneContains(x,y)`. Drawing, pressed feedback and release acceptance must consume
these helpers; no second footer rectangle is authoritative. Existing list, keyboard,
color and editor layouts do not move for this smaller footer.

## Battery tooth

The top swipe panel is a front-tooth silhouette within the existing 466x90 HUD. At
full reveal it follows the display's top arc through `y=21`, continues as a
193 px body from `x=137..329`, and rounds both lower corners to a flat final row
`x=157..309` at `y=83`. A cubic slide moves the complete shape from offset -84 to
zero. Every scanline intersects the local tooth with the physical display circle.

The container itself communicates charge level with fixed colors independent of
the selected theme:

- Above 30 percent: green `#42D978`.
- 11 through 30 percent: yellow `#F2C94C`.
- 0 through 10 percent: red `#FF5B57`.

Percentage text, gauge and outline use fixed dark `#101820`, so they remain legible
on each level color. Charging retains the measured level fill and adds a separate
dark breathing bolt. Percentage and gauge remain in the straight body at `(159,25)`
and `(245,28)`. After text and antialiased primitives render, a final scanline mask
restores the face background outside the current animated tooth; intermediate slide
frames therefore cannot leak glyph or bolt pixels into the uncovered HUD.

`WatchEdgeGeometry.h` is the single geometry contract. A tap dismissal uses
`batteryToothContains(x,y,batteryToothOffset(progress))` against the same current
shape used by rendering, for both press and release coordinates while the panel is
active.

## Focused evidence

`test_ui_controls.cpp` exhaustively compares every footer pixel with
`doneRowSpan`, `doneContains` and `WatchControls::at`. It also checks the new bounds
and label. `test_watch_edge_geometry.cpp` exhaustively compares all display and
battery scanlines with their independent circle/intersection equations over 101
animation progress samples. It covers symmetry, the complete rounded bottom row,
content bounds, battery thresholds and monotonic gauge fill.

Both focused C++11 builds pass with `-Wall -Wextra -Werror`. The host RGB565 sample
at `tmp/watch-edge-ui/battery-tooth-native.png` renders 72 percent, charging 24
percent and 8 percent with the native Latin24 font and shared antialiased shape
primitives. It is a host raster for visual review, not a panel photograph or proof
of touch behavior. Final firmware and hardware evidence must be added after Watch
integration and upload.
