# StopWatch round-edge controls

Status: Done and battery implementation, host validation, firmware upload and
device framebuffer review complete. Liquid physical-button feedback is undergoing
its final integration pass. Finger-on-glass behavior remains separate evidence.

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

## Physical-button edge feedback

The enclosure mapping follows the official StopWatch pin map and product layout:
A is the yellow upper-left button on GPIO 2, B is the blue upper-right button on
GPIO 1, and the red power button is on the lower-left side. The centered bottom
opening is USB-C, so it has no button feedback. The red shape uses 135 degrees as
a screen-space UI center based on M5Stack's [official front product
image](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1242/C152_stopwatch_mainpictures_05.webp),
not as a precision mechanical measurement. While a button is physically held,
its matching colored liquid edge shape expands over 160 ms. Its outer edge
follows the physical display circle; its inner edge uses a smooth central bulge
whose depth and tangent both converge at the pointed ends. A brief expansion
overshoot settles to the held shape without becoming an equal-width line. Each
button owns its press timestamp, so a later button in an A+B chord starts its own
animation. Releasing a button removes its shape on the next rendered frame. The
overlay is visual feedback only and does not replace or synthesize the existing
button gesture events. See the [official StopWatch documentation and pin
map](https://docs.m5stack.com/en/core/StopWatch).

A and B use M5Unified's live `Button_Class::isPressed()` state. M5Unified exposes
the power key to the application as a click/hold IRQ event, so the pinned
M5Unified source receives a verified, fail-closed patch that reads PM1 button
status register `0x48` bit 0 through the same initialized PMIC object. The added
read does not replace `getPekPress()` or the normal `M5.update()` event path. A
failed PMIC read makes power feedback unknown and hidden; it cannot leave a red
shape stuck on screen. The patching hook accepts only the pinned revision and exact
original or patched file hashes, and a second build verifies the patched hashes
without changing them.

Face rendering clears and invalidates its partial-update background whenever a
button edge changes. Static settings and editor pages force a full redraw rather
than a list-band update. In each path the liquid shapes render last and the
underlying content returns without residual pixels on release. Serial-only `keys`
and battery percentage
overrides support framebuffer geometry captures. These overrides do not prove the
physical button signals and must be disabled after capture.

`test_watch_button_feedback.cpp` covers held/released transitions, independent
expansion clocks, simultaneous buttons and invalid power samples. The clean
dependency build applied the PMIC patch from its original hashes; an immediate
second build reported both M5GFX and M5Unified patches already verified. The
intermediate source build used 48,888 bytes RAM and 1,023,653 bytes flash.

## Done/battery device evidence and intermediate button render

The Watch source for the Done and battery captures was built and uploaded at
commit `0bb52ed`. The
1,023,936-byte firmware image has SHA-256
`266e1c78862ad5bfb209bd8f91cc7cd7016a243ba4255dd6687ed84fbebcb15c`;
upload verification matched the flashed image.

Firmware framebuffer captures under `tmp/watch-edge-buttons/device/` show the
smaller Done footer; green 72 percent, yellow 24 percent and red 8 percent battery
teeth; and the fully restored settings page after an intermediate button render.
The three battery frames keep
the fixed dark percentage and gauge readable on every container fill. Their exact
files are `battery-green-72.png`, `battery-yellow-24.png`,
`battery-red-8.png` and `settings-small-done.png`. The `key-*.png` and
`settings-power-overlay.png` files show the superseded equal-width arc treatment;
they are retained only as intermediate redraw evidence and do not accept the final
liquid-button design.

The live intermediate state reported the physical PMIC status read available
(`pwr_valid=1`) and no held button. The device returned to Face with manual preview
off, Chinese UI, identity touch profile, diagnostic key rendering disabled and the
serial port released. Battery values and key masks in these captures are diagnostic
render overrides. They exercise production framebuffer geometry and redraw paths,
but do not claim actual battery measurements, optical panel output, physical held
button recognition or a finger tap on the smaller Done control.
