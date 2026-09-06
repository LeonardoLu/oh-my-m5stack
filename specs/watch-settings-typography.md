# Watch settings typography — 2026-09-06

The 466 px Watch settings hierarchy uses native Noto Sans SC coverage at larger
sizes. Regular settings rows, values, pills, Done and editor labels use Latin24
or Cjk24 with a 29 px line box. Titles, selected expression/name text and numeric
stepper values use Latin28 or Cjk28 with a 33 px line box. The name keyboard uses
Latin24 for English and Cjk22 for Chinese; the latter gives two-character action
labels a 44 px width inside the narrowest 47 px key interior. These are distinct
FreeType-derived 4-bit coverage atlases, not enlarged pixels from the old 18 px
face.

The title center at y=48 has about 284 px of round-safe width. At 28 px the widest
title, `BOT PERSONALITY`, is 242 px. Main and preview rows retain their existing
58 px and 44 px heights; their 29 px text line fits vertically. The label/value
anchors span x=84 through x=382. Bounded pairs preserve at least 16 px between
the measured label and value, including `EXPRESSION` / `Skeptical`,
`DIRECTION` / `Down-right`, and their Chinese equivalents. The format hint uses
the purposeful shorter English copy `Show seconds on the clock`, 301 px at 24 px,
within the roughly 424 px round-safe chord at its y position. The corresponding
Chinese hint is 288 px.

Names remain at the full native font size. The list value is ellipsized at 214 px
and the editor field at 310 px, preserving UTF-8 boundaries and reserving the
width of the three visible dots. No fixed settings label or value falls back to
18 px.

`test_watch_typography.cpp` scans every localized source glyph against the new
CJK faces and checks title, row pair, pill, stepper, help and keyboard bounds. It
also writes `tmp/host-checks/watch-typography.ppm`, a direct RGB565 render using
the generated coverage. Run it through `sh tools/check_host.sh`, or compile the
focused test using the font sources listed beside it in that script. Device
captures and runtime timing remain the Watch integration owner's acceptance;
the host artifact proves source coverage and measured layout, not AMOLED optics.

The first compile-only integration build succeeds with 42,464 B static RAM and
1,005,313 B flash. ELF symbol inspection includes Cjk18/22/24/28,
Latin18/24/28 and Clock36, while unused Latin14 remains absent. This confirms the
new faces retain the library's per-object selective linkage. Runtime and capture
numbers are recorded only after the integration owner uploads the same frozen
sources.
