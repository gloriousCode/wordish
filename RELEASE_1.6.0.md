# Wordish 1.6.0

Prepared locally, not published. This release retains UUID
`bf4b4493-24d6-49e1-9388-4100aa55d9ba`, the existing configuration URL and the
`fuzzy-text-two-options-v2` storage key. `appinfo.json` uses version label 1.6.0
and version code 17; the phone bridge reports the same version label.

The upload-ready [store package](store/1.6.0/README.md) contains plain-text listing
copy and release notes, five screenshots each for Time 2 and Round 2, and a native
720×320 banner. Its manifest records image and bundle checksums.

## Release identity

The store's latest release is labelled 1.5.0, but its downloadable PBW contains
an `appinfo.json` labelled 1.4.9. The store history lists a 1.4.10 double-tap fix
uploaded after 1.5.0, while the latest release still selects 1.5.0. Origin's
current source also says 1.4.10. This explains why a visitor might not receive
the features present in this checkout. Version 1.6.0 moves beyond both labels;
verify the uploaded bundle's embedded metadata after publishing.

Evidence: [live release metadata](https://appstore-api.repebble.com/api/v1/apps/id/a77242a455c04f94a667dbdf?hardware=emery)
and the [bundle served as 1.5.0](https://appstore-api.repebble.com/api/assets/pbw/a77242a455c04f94a667dbdf/1.5.0/55cc1c93-3951-4eaa-ac09-72f2928d0830.pbw),
inspected on 19 October 2026. That bundle's phone JavaScript also says 1.4.9.

The original store entry is
[Fuzzy Text Two](https://apps.repebble.com/fuzzy-text-two_a77242a455c04f94a667dbdf).
Update that entry rather than creating a new app, so existing users and hearts
stay attached. The new display name is 7 UTF-8 bytes, below the SDK's 32-byte
process-name field including its terminator.
An [exact-name store search](https://apps.repebble.com/search?query=Wordish&type=faces&page=1)
returned no results with Pebble Time 2 selected during preparation; final acceptance
still depends on the developer portal.

## Store description

Time in words. Exact when you need it.

Wordish gives the classic Pebble word clock crisp text and
colourful personalisation. Previously called Fuzzy Text Two.

- Read relaxed time at a glance, with native layouts for Pebble Time 2 and
  Pebble Round 2 alongside classic Pebble models.
- Double tap to reveal the exact time for 3, 5 or 10 seconds, following your
  watch's 12-hour or 24-hour clock setting.
- Pick a colour preset or customise individual text rows, fonts and alignment.
- Add a discreet corner date with a choice of formats and colours.
- Switch animations or the double-tap gesture off for a still, simple display.
- Optionally show low battery or phone disconnection, without vibrations.
- Choose Catalan, English, French, German, Norwegian, Spanish or Swedish.

The low-battery indicator appears at 20% or less and stays hidden while charging.
Indicators are off by default. Colour customisation requires a colour screen;
monochrome watches use high-contrast black and white.

Open settings in the Pebble app to make it yours. A colour preview helps you
choose a readable combination before saving.

## Release notes for the store

Fuzzy Text Two is now Wordish. This update adds animation and
double-tap switches, adjustable exact-time duration, optional silent battery and
connection indicators, and a clearer settings page with colour preview. It also
fixes missing exact-time characters, truncated translated phrases, interrupted
animations and legacy colour migration. Settings delivery now retries transient
failures while preserving your latest choices.

## Screenshot sequence

Use fresh, unaltered emulator captures from the 1.6.0 bundle. Lead with benefits
rather than a grid of nearly identical colour choices:

1. **Time in words** — large, clear words on Pebble Time 2.
2. **Make it yours** — an attractive colour preset on Pebble Round 2.
3. **A discreet date** — the corner date alongside the word clock.
4. **Exact when needed** — digital time on demand.
5. **Useful details, quietly** — optional status indicators alongside the date.

Additional verification captures should show long Catalan phrases and simultaneous
date/status indicators on round and rectangular screens. Emulator captures do
not establish physical watch contrast or battery life.

The selected upload files are in `store/1.6.0/emery` and `store/1.6.0/gabbro`,
numbered in this order for each platform. The full verification captures are in
`screenshots/release-1.6.0`.
`scripts/capture_screenshots.py` records bundle/source hashes, settings and capture
times in a manifest for each platform, and checks real emulated double taps,
the exact-time timeout and the disabled-gesture setting.

## Local validation

The final SDK 4.33.1 build passes `make lint` for all seven targets. The
`make misc_checks` run passes both host C suites and 20 Node configuration tests.
The C suite includes 193,536 decorated layout combinations; its SDK model is
complemented by actual Emery and Gabbro captures. Real-font regression cases
cover Catalan “dos quarts” and round-screen “twelve”/“eleven”, which the previous
width estimates allowed to truncate. A bounded 16-entry cache reuses recent font
measurements.

Aplite reports 19,865 bytes of static RAM footprint, leaving 4,711 bytes for heap,
with 70,932 bytes of resources. These linker figures are not peak runtime heap or
battery measurements. The build checks bridge syntax with Node; optional JSHint
is unavailable. No physical phone/webview or watch test has been completed.

All 26 final Emery/Gabbro images have verified image, bundle and source hashes.
The real emulated double-tap, timeout and gesture-off comparisons pass on both
platforms. Visual inspection confirms complete long phrases and simultaneous
date, Quiet Time, low-battery and phone-disconnection icons. The capture helper
waits 27 seconds for a disconnection, beyond PebbleOS's 25-second debounce;
an earlier 1.5-second capture was too soon to show the icon.

## Publishing order and verification

1. Run `make lint` and `make misc_checks`, then inspect the final bundle and
   captured screenshots. Keep the UUID unchanged.
2. Publish `resources/configure-fuzzy-text-two.html` at its existing URL. The new
   page tolerates missing new preferences; existing versions ignore the extra
   preferences. Confirm the hosted page displays the new name and controls.
3. Upload `build/fuzzy-text-two.pbw` to the existing store entry as **1.6.0**.
   Apply the approved Wordish name, description, release notes and screenshot
   sequence above. Keep the existing listing and app UUID for continuity.
4. Download the published bundle and inspect its embedded version/UUID. Confirm
   the store selects 1.6.0 as latest and uses the intended screenshots.
5. Check settings save/cancel on a real phone, including disconnected delivery,
   and verify the watch retains settings after reopening the face.

GitHub publishing is gated by approval of the exact `git push origin ...`
command. No PR may be created under this repository's instructions. A physical
phone/watch check requires access to the device; emulator and host results must
not be presented as that check.

The repository's Pages configuration was read on 19 October 2026: it publishes
the root of `master` at `www.gloriousedge.com/fuzzy-text-two/`. The current local
`master` tracks `origin/master`. After committing and reviewing the release,
the publishing command is `git push origin master`, subject to explicit approval.
That push publishes the configuration/site; the PBW still needs a separate
upload through the store developer portal.

## Adoption baseline

On 19 October 2026, before these changes were published, the listing displayed
14 cumulative hearts and appeared second in the observed “fuzzy” search with
Pebble Time 2 selected. These are not installation or retention figures.

Record publication time and compare equal periods before/after each listing
change. If the developer dashboard exposes listing views and installs, calculate
installs divided by views for each period. Otherwise record the change in hearts
as a limited engagement signal. Do not infer improved conversion from a new name
alone or compare the raw total directly with a face released in 2014. No monitoring
automation or telemetry has been added.
