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
