# Watch ambient gaze experience

2026-09-09. This contract replaces the older 26–48 second random mood shuffle and
60 second interaction postponement in the StopWatch app.

## Automatic sequence

Automatic mode always enters visibly as Idle. Idle uses BotUx's canonical Auto gaze,
which is the UpRight pose. It remains for a randomized 5–15 seconds, then changes to
Looking around for 30–60 seconds. Looking around uses BotUx's autonomous full-range
gaze. It then changes to one of Idle, Listening, Thinking, Happy, Working, Waiting or
Done for 5–15 seconds before returning to Looking around. This alternating sequence
continues while automatic mode remains eligible.

Only one transition can occur in one host update. A late frame schedules the next
complete phase from that frame rather than catching up through invisible states. All
deadline comparisons support unsigned `millis()` rollover.

## Precedence and interaction

Manual mood selection, a non-Auto expression, settings and editor screens, doze, low
battery, transient poke/happy reactions, temporary directed gaze, and an active input
gesture take precedence over the automatic sequence. The sequence cannot advance behind
those presentations. When automatic behavior becomes eligible again, it restarts at Idle
with a new 5–15 second interval. B double performs the same restart and clears the saved
expression/action overrides as before.

A or B may manually select all 14 moods, including Looking around. Manual choices stay
held until B double; the Watch scheduler never replaces them. A face contact temporarily
presents Idle and follows the contact for 2,200 ms. A point inside the actual bot body
requests Front. A point outside supplies the normalized center-to-contact direction.
When the temporary gaze expires, a manual mood is restored; automatic mode begins its
new Idle dwell. Settings gaze remains an independent persistent field and resumes after
the temporary target.

## Host validation

`test_watch_interaction` checks both duration ranges, repeated phase alternation, safe
mood bounds, unsigned rollover, a late update, and pause/resume entry through Idle.
`test_companion_presets` checks the complete 14-mood manual deck, the safe ambient set,
and temporary touch presentation/restoration including manually selected Looking around.
These are deterministic state and presentation checks; they do not claim physical touch
or display acceptance.

The native CJK atlases were regenerated from the pinned Noto Sans SC source with the
repository's existing Python 3.14/Pillow 12.3 environment. A glyph-by-glyph parser check
against the prior Cjk18, Cjk22, Cjk24 and Cjk28 files found zero changes to the metrics or
coverage bytes of all 340 existing glyphs. The only additions are `处` (U+5904) and `看`
(U+770B); `到` was already present. Sorted insertion changes later byte offsets and the
generated source line wrapping, without changing existing rendered glyphs.

## Validation result

The focused Watch interaction, companion preset, string and native typography checks
pass under C++11 with `-Wall -Wextra -Werror`. The final `m5stack-stopwatch` PlatformIO
build against the frozen shared BotUx source uses 48,928 B RAM and 1,061,825 B flash;
the generated firmware image is 1,062,192 B. This validation includes no serial upload
or physical hardware acceptance.
