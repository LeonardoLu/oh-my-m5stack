# Watch settings and companion controls

2026-09-06. This iteration refines the StopWatch UI without changing the shared
BotUx input contract.

- Every non-face screen uses one centered `Done` pill. Its drawn rounded rectangle,
  pressed surface and touch target use the same bounds and corner radius. In an
  editor it saves and returns to the parent; in Bot Personality it returns to
  the screen that opened it; in Settings it saves and returns to the face. Power Home and hardware
  Long A retain the cancellation paths.
- Settings and Bot Personality use a 288 px viewport with four 72 px row slots.
  Each row draws its label at the left and current value at the right. Preview
  editors use a 120 px lower viewport with two 60 px slots; Motion and Combinations
  scroll their additional rows. Multi-column steppers, the keyboard and HSV surface
  remain direct controls.
- A touch press immediately clears persistent navigation selection. Its pressed
  fill exists only while captured or during the short accepted-click flash.
  Persistent row selection appears only after A/B navigation.
- Touch-up-inside checks the original rounded target and its viewport. Leaving and
  re-entering remains valid, while a captured scroll never clicks and a clipped row
  cannot release through fixed chrome.
- Face touch uses the current BotUx `cx`, `cy` and `bodyR`. A point inside the body
  requests Front; a point outside supplies its normalized direction. Tracking can
  begin anywhere on the face. A stationary bot hold opens Bot Personality after
  three seconds; the generic button and non-face hold threshold stays two seconds.
- A chooses a non-repeating random result across every shared mood and B advances
  through every shared mood, including Working and appended moods such as Asleep.
  B double returns to automatic Idle and resumes the safe ambient rotation.
  Explicit selection temporarily overrides the BotUx low-battery expression while
  the Watch HUD retains the real percentage. Autonomous
  idle rotation is limited to Idle, Listening, Thinking, Happy, Working, Waiting
  and Done.

Validation requires focused rounded-boundary, clipped-row, adaptive-list, three-
second hold and full mood-set host checks, the full host runner, a clean Watch build,
and native captures of Settings, Bot Personality, pressed Done, Motion before/after
scroll, and the final Combinations row. Upload and injected-contact evidence must
identify the explicit Watch serial port.

## Validation result

The complete host runner passed with 21 reported products, including the new
companion mood/presentation and Watch control geometry checks. The final Watch
build used 37,736 B RAM and 726,149 B flash. Its 726,512 B image was uploaded and
hash-verified on `/dev/cu.usbmodem214201` from commit `a926d87`.

Native RGB565 captures under `tmp/watch-settings-native/` show four unselected
left/right rows in Settings and Bot Personality, the exact pressed Done pill,
Motion at offsets 0 and 120, and Combinations at its maximum offset 60. Injected
contact stayed on Face at 2.15 seconds and entered Bot Personality after 3.15
seconds. Working and Speaking each changed to the temporary Idle/Auto/Auto gaze
presentation, then restored their original mood after 2.2 seconds; Asleep rendered
as mood 12. The session finished by hardware-resetting volatile diagnostic state;
the final readback was Face, automatic mode, Idle, with the original persisted
Chinese language and other settings intact. These captures and injected contacts
exercise firmware paths but do not claim physical touch-panel acceptance.

## Physical Done investigation (in progress)

The 2026-09-06 physical trial uses a command-gated, fixed-capacity trace in the
Watch firmware. It records sampled contact changes, bounded held-contact and long-gap
checkpoints, pointer capture/scroll/end reasons, screen transitions, CST820 interrupt
timestamps, acquisition count, and maximum sample/read time. Trace collection performs
no Serial output or allocation in the input/render loop. `trace dump` first disarms the
GPIO interrupt and sampling trace, then emits the bounded buffers.

The first trial is preserved in `tmp/done-debug/session.log`. The user attempted the
Settings Done control more than five times and reported one success. All contacts were
acquired by the controller and application. Seven rejected presses began and ended at
stable converted coordinates `(227,433)`, `(231,440)`, `(223,442)`, `(220,431)`,
`(227,437)`, `(220,458)`, and `(223,449)`; every one captured `None` and ended with
`reason=no_target`. The accepted press began and ended at `(276,416)`, captured and
released target `Done`, ended after 67 ms with `reason=accepted`, and immediately logged
the Settings-to-Face transition. The trace contained 25,912 acquisitions, no ring drops,
a maximum observed acquisition gap of 79 ms, a maximum read of 329 us, and 24 retained
CST820 interrupt edges.

The corresponding native framebuffer is `tmp/done-debug/settings.png`. Its visible blue
Done pill occupies exactly `x=154..311, y=376..421`; the half-open rounded hit target is
`x=154..312, y=376..422`. The 466 px application canvas is pushed at `(0,0)`, so rendering
and hit testing agree. The rejected samples are 9–37 px below the visible pill.

A second trial asked for presses centered on the rendered Done text `(233,399)`. Ten
successive contacts were rejected at y=423..444, 24–45 px below that center; the eleventh
was accepted at `(206,413)`. After reopening Settings, three more were rejected at
`(256,427)`, `(256,451)`, and `(254,440)`, followed by an accepted contact at `(235,400)`.
Every contact was fully acquired and ended without drag or timeout. These trials rule
out missed acquisition, release timeout, drag promotion, and draw/hit disagreement as
the cause of the observed failures, but a single control could not distinguish mapping
error from contact placement.

The command-gated five-point probe then displayed crosshair targets at center, top,
left, right, and the Done center. It consumed ordinary navigation and recorded raw and
converted coordinates from the same CST820 read. Stable `(target, raw, converted)`
results were `(233,233; 249,251; 255,257)`, `(233,100; 240,104; 246,106)`,
`(100,233; 95,253; 97,259)`, `(366,233; 380,248; 389,254)`, and
`(233,399; 228,427; 233,437)`. The error grows toward the lower edge and is present in
the raw contact and the library conversion rather than in the UI target. At the bottom
probe, the stale transform adds 10 px to raw y=427; it does not by itself explain the
full 38 px difference between the requested target and converted contact. Whether the
remaining raw-coordinate difference was affected by the CST816-specific initialization
or contact centroid is not established by this probe.

The static StopWatch board configuration declares touch maxima 233 and a 468 px panel;
that describes an intended transform, not the transform running on this firmware. The
linked dependency is `.pio/libdeps/m5stack-stopwatch/M5GFX@src-6e4f1aa2c3141670f2da75faf884f9ac/`.
Its StopWatch path installs the CST touch object while `Panel_AMOLED_Framebuffer` still
has its default 240 px configuration, computing an approximately `239/233` affine
scale. It later copies the 468 px panel configuration without recomputing that touch
transform. The measured mappings, such as raw `(249,251)` becoming `(255,257)` and raw
`(228,427)` becoming `(233,437)`, match the stale 240 px transform. Sensor raw values
already use the physical display's pixel domain. Changing only the 233 range while
leaving framebuffer initialization unchanged would instead apply approximately
`239/467` and shrink coordinates to about half size;
the range and framebuffer calibration order must be corrected together.

An isolated per-device independent X/Y linear-profile prototype was used to check the
measurement math, but it is not the production remedy. The first five-point
least-squares candidate for this physical unit is
`x=0.92819782*raw+11.71763980`, `y=0.92641282*raw+1.88247027`. Production work first
corrects and verifies the linked library's deterministic framebuffer/touch setup and
the CST820 coordinate contract; the measured fit remains diagnostic evidence rather
than a result-specific default.

The production remedy is a repository-tracked patch against pinned M5GFX commit
`d91077b`. A post-dependency script accepts only that commit and the expected SHA-256
set, applies the patch once, verifies the patched hashes, and rejects drift. It gives
StopWatch a distinct CST820 path matching the factory firmware: 100 kHz I2C, A7/A9
identification, one seven-byte read from register zero, direct 12-bit coordinates, and
no CST816-specific FA/ED writes or wait heuristic. The StopWatch touch range is set to
`0..467` to express pixel identity against M5GFX's current 468 px framebuffer; this is
not a claim that 467 is the measured sensor maximum. The framebuffer copies its final
configuration before installing the touch device and computing the affine transform.

A cold dependency install and full rebuild applied the patch before M5GFX compilation.
The resulting ELF contains `Touch_CST820`. Its runtime `cal map` report showed separate
axes and pixel identity: `(0,0)->(0,0)`, `(0,467)->(0,467)`, `(467,0)->(467,0)`, and the
previous lower probe `(228,427)->(228,427)`. M5GFX's floating affine calculation truncates
some Y results down by one pixel (`(233,233)->(233,232)`, `(467,467)->(467,466)`), which
is the bounded numeric effect of the identity transform rather than measured fitting.

The first physical Done trial after this library fix remained intermittent. The user
reported that the seventh press succeeded, then reopened Settings, pressed slightly
higher, and succeeded after another two or three attempts. The final trace dump is
preserved in `tmp/done-debug/library-fix-final-session.log` and its trial slice in
`tmp/done-debug/library-fix-trial-session.log`. Its 160-entry ring had overwritten
1,209 entries during the wait, so it does not preserve the first seven coordinates.
The retained tail has four Settings contacts: `(275,424)` ended `no_target`;
`(234,392)` began on Done but moved to `(263,441)` and ended `target_changed`;
`(232,440)` ended `no_target`; and `(230,416)` was accepted and immediately changed
Settings to Face. These are post-conversion coordinates; that trace version did not
retain the same-read sensor coordinates, so none are inferred after the fact. The
runtime `cal map` confirms that the deterministic stale-scale bug was fixed; this
physical trial shows that Done acceptance remains intermittent and needs
device-specific mapping evidence.

The factory CST820 driver provides no additional offset, scale or rotation constants:
it decodes the 12-bit register coordinates and passes them directly to LVGL. The
factory display is configured as 468 by 466 with panel-memory x offset 6, while this
application uses the linked M5GFX framebuffer and a 466 by 466 canvas at `(0,0)`; no
second hidden application translation exists. The linked high-10 ms, low-8 ms,
high-2 ms reset sequence satisfies the CST820B table's
0.1 ms low-pulse and 5 ms pre-release requirements, and the subsequent 150 ms OLED
startup delay exceeds the 100 ms reset-to-operation requirement.
These checks do not yield a further physical touch affine, so a remaining per-device
transform cannot be justified as a universal constant.
The source references are the factory
[`hal_display.cpp`](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/main/hal/hal_display.cpp),
factory
[`cst820.cpp`](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/main/hal/drivers/cst820/cst820.cpp),
and the official
[`CST820B` datasheet](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1242/CST820B_datasheet.pdf).

The trace recorder now keeps contact samples separately from a 128-entry critical
Down/End/Screen ring. Idle acquisitions update counters without consuming either
ring; a bounded sequence of at least twenty trials therefore survives an arbitrarily
long idle wait. When tracing is enabled, one `getTouchRaw` result supplies the sensor
coordinate and the same point is passed once through `convertRawXY`; zero-result reads
report no sensor coordinate, and End explicitly identifies its coordinate as the last
valid contact sample. Acquisition and IRQ totals are snapshotted with key events.

The command-gated standard calibration uses new measurements after the CST820 library
fix. `cal start` collects five fixed training targets, rejects a sample whose raw span
exceeds 16 px, solves M5GFX's six-coefficient affine, and requires training maximum
error at most 10 px. A RAM candidate is installed once through
`Panel_Device::setCalibrateAffine`. Five distinct holdout targets are then measured
without refitting: `(145,145)`, `(321,145)`, `(145,321)`, `(321,321)`, and `(233,410)`.
Acceptance uses the actual integer `convertRawXY` results, with every point at most
10 px away and RMS at most 7 px; float predictions are diagnostic only. Failure or
abort restores the loaded baseline. A passing candidate remains RAM-only for a normal
Done trial. Only a later explicit `cal save`, after that independent trial succeeds,
writes one versioned coefficient blob in the dedicated `watch-touch` namespace;
`cal reset` clears it and restores identity. No coefficient from the earlier probe is
reused or compiled as a default.

The calibration firmware at `bc06505` passed 24 host-runner products and built with
48,448 B RAM and 1,016,617 B flash. Its 1,016,976 B image has SHA-256
`f1ca595aedfdef526beebfb1c54afc28ac4c7bb6603e90e619337316857da36f`; upload to
`/dev/cu.usbmodem214201` completed with hash verification while preserving NVS.
Runtime readback before training reported `stored=0`, no RAM candidate, and the exact
identity matrix. All six `cal map` anchors, including separate x/y axes and the former
bottom probe, converted identically. The native 466 by 466 TRAIN 1/5 framebuffer in
`tmp/done-debug/affine-train.png` passed layout review. At that firmware-readiness point,
physical training and holdout validation had not yet run and no profile had been saved.

The first complete calibration attempt on that firmware failed its fixed validation
limits and rolled back to identity without writing NVS. The five training taps produced
an affine candidate with 8.126 px RMS and 9.729 px maximum training error, so validation
started. Its unseen-point errors were 8.062, 8.062, 3.000, 21.954, and 14.866 px: RMS
12.977 and maximum 21.954 exceeded the predeclared 7 px RMS and 10 px per-point limits.
The final profile report confirmed `stored=0`, no candidate applied, and identity active.
Each held contact itself was stable to 0–1 raw pixel, but there was only one independent
tap at each target. That run therefore does not establish whether the remaining pattern
is a repeatable spatial nonlinearity or variation in the contact centroid of ordinary
flat-finger touches.

The follow-up `cal repeat` diagnostic measures that distinction without fitting or
applying another transform. It presents cyan targets in the fixed order center,
lower-right, bottom, lower-right, bottom, center, bottom, center, lower-right, using
coordinates `(233,233)`, `(321,321)`, and `(233,399)`. The existing stable-sample,
16 px span, and confirmed-release gates make all nine taps independent. Its dedicated
results preserve target, raw mean and range, actual identity-converted mean, sample
count, and hold duration for `cal dump`. Starting the run restores the loaded baseline;
completion only displays `9/9 COMPLETE` and never solves, applies, or saves a profile.

The physical repeat run completed all nine points under the identity baseline. Center
reported `(254,256)`, `(258,243)`, and `(245,256)`; lower-right reported `(340,339)`,
`(339,340)`, and `(346,339)`; bottom reported `(225,426)`, `(237,437)`, and
`(240,431)`. Raw and converted means were identical, and every held contact had a
zero-pixel raw range over 6–18 acquired samples. Variation occurred between independent
taps: maximum same-target distances were 18.38 px at center, 7.07 px at lower-right,
and 16.28 px at bottom. Mean offsets from the displayed targets were respectively
`(+19.33,+18.67)`, `(+20.67,+18.33)`, and `(+1.00,+32.33)` px. This confirms that
ordinary flat-finger target acquisition is not represented by a single noiseless point;
it does not by itself distinguish position-dependent controller mapping from contact
centroid or target-occlusion effects, and it does not establish a non-affine hardware
mapping. The run ended with `stored=0`, no candidate applied, identity active, and no
NVS write or reboot.
