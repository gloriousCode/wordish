[Go back][back]
[back]: javascript:history.back()

Changes
========

Version 1.6.0 is prepared locally and has not yet been published. It retains
the existing app UUID and saved settings under the name Wordish.

version 1.6.0 (unreleased)
-------------------------

- Smooths the larger clock fonts on Time 2 and Round 2 while measuring their
  full outer edges for screen fit. Older models retain their existing rendering.
- Replaces the old colourful presets with Lagoon, Ember, Orchid, Grove and Sand:
  coordinated palettes with strong contrast and quieter date accents.
- Adds animation and double-tap switches, plus a choice of 3, 5 or 10 seconds
  for exact time.
- Adds optional, silent low-battery and phone-disconnection indicators.
- Adds a settings colour preview, contrast warning and accurate custom-style label.
- Groups everyday settings separately from optional colour editing, keeps Save
  accessible, and lets users undo changes before sending them to the watch.
- Preserves legacy inverted colours and existing settings during partial updates.
- Retries failed settings delivery twice, without letting stale saves override new ones.
- Fixes missing digital-time glyphs, truncated translated phrases, small-buffer
  writes and interrupted animation cleanup.
- Fits text using actual font measurements, with a small cache, to prevent
  ellipsis on long words and circular layouts.
- Avoids unnecessary layer updates while the displayed words/date stay unchanged.
- Renames Fuzzy Text Two to Wordish and clarifies the store copy.

version 1.4.10
--------------

- Adds a double tap gesture that briefly shows the exact digital time before
  returning to fuzzy text.

version 1.4.9
-------------

- Adds a preset selector with quiet, mid-range, and colourful styles that
  demonstrate row colours, date section colours, date placement, and date
  formats.
- Replaces the corner date radio groups with labelled placement and format
  selectors.

version 1.4.8
-------------

- Adds optional separate colours for minute row one, minute row two, hour row,
  and each corner date section.

version 1.4.7
-------------

- Adds an optional small corner date with top or bottom placement and
  compact numeric or abbreviated formats.

version 1.2.1
-------------

- Version 1.2.0 was released with the wrong settings URL; this version
  fixes that.

version 1.2.0
-------------

- Fixes blank settings page on iOS devices.  (Many thanks to Tomi De Lucca!)
- Adds translation, English (Great Britain).
- Fixes for Spanish translation.  (Again, thanks to Tomi De Lucca!)

version 1.1.0
-------------

- Adds Norwegian translation provided by iFlip.

version 1.0.0
-------------

- first release
