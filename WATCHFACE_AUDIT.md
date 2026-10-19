# Watchface audit — 19 October 2026

The clearest opportunity is to explain the existing benefits better. The live
listing undersells the watchface; the available evidence does not establish that
the name or a lack of features causes low adoption. Technical improvements are
real, but visitors need to see their everyday benefit before installing.

This document preserves the initial audit below. The subsequent implementation
is prepared as **1.6.0**, under **Wordish**. See the
[release plan](RELEASE_1.6.0.md) for the current copy, screenshots, validation and
publication boundary. The original findings and initial validation figures below
describe the earlier audit snapshot, not the final release candidate.

## Store evidence

These are displayed cumulative hearts, not downloads, active users or retention.
The following pages were inspected live with Pebble Time 2 selected. Counts are
rounded where the store rounds them. The official [store API documentation](https://appstore-api.repebble.com/)
defines Most Loved using hearts. Older faces have had much longer to accumulate
them, so these totals are not a fair measure of recent conversion.

| Watchface | Hearts | What its listing communicates |
| --- | ---: | --- |
| [Fuzzy Text Two](https://apps.repebble.com/fuzzy-text-two_a77242a455c04f94a667dbdf) | 14 | Four screenshots; a single sentence about newer watches and colours. Version 1.5.0, updated 9 July 2026. |
| [Fuzzy Text International](https://apps.repebble.com/fuzzy-text-international_52ef7ab0c503c88bdf00001a) | 1.4K | Established word-clock lineage; last updated in 2014; scaled Pebble Time 2 support. |
| [Fuzzy Text International 2](https://apps.repebble.com/fuzzy-text-international-2_30bf2c0381b64d3aaaec0291) | 62 | Closest modern competitor: eight screenshots, eleven language/region choices, animation switch, optional gesture date and timeout, Clay settings. Updated 19 July 2026. |
| [TimeStyle](https://apps.repebble.com/timestyle_55a5c024f4510f794c000071) | 16.1K | Readability, custom colours, optional information widgets and low-battery/disconnection notices. Native Pebble Time 2 and Round 2 support. |
| [YWeather](https://apps.repebble.com/yweather_52cc44e045ffdd31dd000180) | 22.5K | A clear daily use: weather and forecast, with gesture access and multilingual display. |
| [Modern](https://apps.repebble.com/modern_52bb213af9846878c200015b) | 14.7K | A strong historical total despite a 2014 release and a warning about settings availability. Hearts alone do not prove current quality. |

Fuzzy Text Two appeared second in the observed search for “fuzzy”. Search order
can change, but this is evidence against assuming that the name alone prevents
discovery. No impression, installation, uninstall or cohort data was available.

## Recommended order of work

1. **Improve the description and screenshots first.** Lead with calm, readable
   time and precision on demand. Explain double tap, date, presets, row colours
   and languages. Use customer-facing watch names rather than SDK codenames.
2. **Give each screenshot a different purpose.** Show large clear words on
   Pebble Time 2; an attractive colour preset on Round 2; the corner date; then
   exact digital time. Capture the final build so the images demonstrate what
   ships. Avoid using every slot for minor colour variations.
3. **Consider an animation switch, gesture switch and exact-time duration.**
   These fit the existing concept and address control offered by the closest
   competitor. Disabling work may save power, but battery improvement needs
   measurement before advertising it.
4. **Consider discreet optional low-battery and disconnection indicators.**
   These add practical value without turning a word clock into a dashboard.
   Weather and health are lower priorities because they change the product's
   focus and add configuration, permissions or network dependencies.
5. **Use a short, distinct name.** “Two” says little about the benefit and
   resembles “Fuzzy Text International 2”. The subsequently approved name is
   **Wordish**, with **Time in words. Exact when you need it.** as the tagline.
   Keep “formerly Fuzzy Text Two” in the description for continuity. The store
   search found no Wordish result with Pebble Time 2 selected; final acceptance
   depends on the developer portal. Improved adoption remains a hypothesis.

Where the developer dashboard provides them, compare listing views, installs
and conversion over comparable periods. Otherwise record new hearts over time
as a limited engagement signal, not a proxy for downloads. Change copy and
images before changing the name so the effects are easier to distinguish.

## Suggested listing copy

> Time in words. Exact when you need it.
>
> Fuzzy Text Two gives the classic word clock crisp text and colourful
> personalisation for today's Pebble watches.
>
> Read relaxed time at a glance. Double tap to briefly reveal the exact time.
> Choose a colour preset or customise individual text rows. Add a discreet
> corner date, and choose your font, alignment and language.
>
> Native layouts for Pebble Time 2 and Pebble Round 2, alongside classic Pebble
> models. Includes Catalan, English, French, German, Norwegian, Spanish and Swedish.
>
> Open the watchface's settings in the Pebble app to make it yours.

This copy is a local draft. Confirm it against the intended release before
publishing. At this initial audit stage, no listing or application name had changed.

## Changes made during this audit

- Correct remaining-capacity accounting in word conversion, including empty
  buffers, to prevent out-of-bounds writes with small buffers.
- Preserve complete phrases when a narrow line limit would otherwise consume
  all four rows before reaching the hour, including Catalan phrases.
- Make animation cancellation safe when the SDK invokes stop callbacks
  synchronously; release only the animation that actually finished.
- Avoid mutating text layers for unchanged time and date content; refresh when
  a font, format, date or Quiet Time change requires it.
- Include digits and the colon in all four bundled large-font character sets
  so exact digital time can render on high-resolution layouts.
- Validate configuration response shapes and values, tolerate damaged URL
  encoding, and preserve existing options when updates are partial.
- Preserve legacy inverted colours both during watch-side migration and when
  reopening and saving the form, without overwriting explicit custom colours.
- Add an illustrative colour preview, low-contrast warning, accurate
  preset/custom state, zoom support and availability rules for date controls.

The configuration remains a self-contained page with no additional framework
or network dependency. Its preview illustrates colours, not device typography,
language or pixel-perfect geometry. AppMessage failures still log an error and
retry only at the next JavaScript launch; bounded retries are a follow-up.

## Validation and release boundary

`make misc_checks` passed: host layout/conversion checks (including 13,824
phrase-preservation cases), actual watchface callbacks against a small SDK model,
and ten Node configuration regressions. `git diff --check` also passed. The
unchanged-minute regression verifies zero layer mutations or new animations;
this does not measure CPU time or battery life. An additional address/undefined
behaviour sanitiser run remained unfinished without output, so it provides no
validation result.

The installed SDK's Python symlink points to a removed Homebrew interpreter,
so plain `make lint` fails before compilation. Running the same SDK's Waf build
with the existing Python 3.13 interpreter and SDK dependencies builds all seven
targets without changing the SDK installation. The final Aplite build reports
16,881 bytes of static RAM footprint and 7,695 bytes left for heap; resources
occupy 70,932 bytes. These are linker figures, not peak runtime heap usage.
The existing Waf configuration
also skips JSHint if it is absent; a build is not proof that JSHint ran.

Browser security policy rejected the local configuration page's `file://` URL.
Preview behaviour has host-level tests, but no visual browser verification.
No watch/emulator session or physical battery benchmark was run in this audit.

Before release:

- Reconcile **store version 1.5.0** with **local manifest/bridge version 1.4.10**.
  The inspected checkout must not be assumed to match the published binary.
- Verify exact-time glyphs, quick repeated gestures, midnight/date changes,
  Quiet Time and long translated phrases on round and rectangular screens.
- Validate the configuration in the phone webview, including save/cancel and
  delivery while the watch is disconnected, and capture fresh screenshots.
- Publish the configuration HTML as well as the watch bundle when authorised.
  The [live configuration URL](https://www.gloriousedge.com/fuzzy-text-two/resources/configure-fuzzy-text-two.html)
  returned HTTP 200 during the audit, but still serves the existing deployed
  page; local fixes do not update it automatically.

Existing screenshot files and pre-existing test hooks were preserved. No branch,
commit, push, pull request, store publication or SDK installation was performed.

## Follow-up implementation

After approval to implement the recommendations, the local 1.6.0 release adds
animation and gesture switches, configurable exact-time duration, optional silent
battery/disconnection indicators, and ordered settings delivery with two retries.
The approved display name is now **Wordish** in the bundle and settings page,
replacing the earlier proposed title. The UUID and saved-settings key are unchanged. The updated release document
contains the store description, release notes, gallery order and adoption baseline.

Investigation also found that the store-labelled 1.5.0 download actually embeds
1.4.9 metadata and JavaScript. The prepared 1.6.0 label/code17 avoids the earlier
version ordering problem; the published download must be checked after upload.

Further runtime fixes own the tick callback's time value instead of retaining a
borrowed pointer, reserve room for date/status content, and fit real font glyphs
instead of relying only on estimated widths. Emulator screenshots exposed three
clipped phrases that host estimates missed; dedicated regression cases now cover
them. Unchanged content still avoids unnecessary layer and animation work.

Normal `make lint` now works with the installed SDK 4.33.1 and builds all seven
targets. The expanded host checks include 20 Node tests and 193,536 decorated
layout combinations. Current Aplite memory figures and final emulator evidence
are recorded in [the release document](RELEASE_1.6.0.md). The initial optional
sanitiser test was stopped with permission and supplies no passing result.

The configuration and store listing remain unpublished. Phone-webview checks,
physical watch testing and battery measurement remain outside the verified
results. The release and store package are prepared for a local commit; no push,
PR or store publication has been performed.
