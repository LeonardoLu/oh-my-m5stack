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
`x=154..312, y=376..422`. StopWatch panel and touch rotation are both zero, M5GFX scales
raw touch `0..233` to panel `0..467`, and the 466 px application canvas is pushed at
`(0,0)`. The two-pixel panel/canvas difference cannot explain the rejected samples,
which are 9–37 px below the visible pill. This trial rules out missed acquisition,
release timeout, drag promotion, and draw/hit disagreement for those failures. It does
not yet establish whether the physical target-to-coordinate mapping is offset or the
finger contact centroid landed below the intended visual point. A second trace asking
for one press centered precisely on the rendered Done text is pending before changing
geometry or calibration.
