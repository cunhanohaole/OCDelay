# PRD: OCDelay v0.3 — Repeater with pitch curve

## Goal
OCDelay repeats a single-shot sound for an exact number of bars, then stops dead at the end. Drawn curves shape the volume and the pitch across that time. It's built for one-off hits and phrases, not continuous audio (the name is a deliberate OCD joke about precise control).

- v0.1 (shipped) was a classic feedback delay.
- v0.2 replaced it with a repeater and a volume curve.
- v0.3 adds 1- and 2-bar lengths, a ping-pong Width control and a pitch curve.

## How it works
1. The plugin waits for a hit: the input jumps above the Threshold after silence.
2. It captures a slice of the input, Slice long, starting from the hit.
3. It replays that slice back to back until Length is reached, measured from the hit, then stops exactly there.
   - Example: a 1-bar slice with an 8-bar Length plays the original bar plus 7 repeats.
4. The volume curve shapes the repeats over the whole Length. The pitch curve sets the pitch of each repeat.

## Features
| Parameter   | Type   | Range               | Default   | Unit | Notes |
|-------------|--------|---------------------|-----------|------|-------|
| Slice       | float  | 1 – 2000            | 500       | ms   | Shown when Sync is off. Keeps the param ID `delayTime` |
| Division    | choice | 1/64 … 1/1          | 1/4       | note | Slice length when Sync is on (the same 17 steps as v0.1) |
| Sync        | bool   | off / on            | off       | —    | Off: Slice in ms. On: Slice is a note division at the host tempo |
| Length      | choice | 1, 2, 4, 8, 16, 32  | 8         | bars | Total time from the hit to the end of the last repeat. Always uses the host tempo |
| Threshold   | float  | −60 – 0             | −30       | dB   | Input level that counts as a hit [NEED: confirm default by ear] |
| Mix         | float  | 0 – 100             | 50        | %    | 0 = dry only, 100 = repeats only |
| Ping-Pong   | bool   | off / on            | off       | —    | Repeats alternate L, R, L… (input summed to mono) |
| Width       | float  | 0 – 100             | 100       | %    | How far apart Ping-Pong places the repeats. 100 = hard left/right, 0 = centred. Only has an effect with Ping-Pong on |
| Pitch Range | float  | 1 – 48              | 12        | st   | Semitones covered by the pitch curve |
| Bipolar     | bool   | off / on            | off       | —    | Off: curve bottom = no change, top = +Range. On: middle = no change, top = +Range, bottom = −Range |
| Volume curve| state  | —                   | ramp down | —    | Volume envelope over Length. Saved with the session; not automatable |
| Pitch curve | state  | —                   | flat at bottom | — | Pitch over Length. Saved with the session; not automatable |

- Feedback is removed. Its parameter ID `feedback` is retired and must never be reused.
- Length gained 1 and 2 bars before v0.2 was ever released, so its choice list changed in place. From v0.3 on, its choices only ever get appended.
- VST3 only, macOS and Windows.

## Triggering
- A hit is the input level rising above Threshold.
- A new hit is only accepted after the input has stayed below Threshold for [NEED: confirm — start at 50 ms] (re-arm), so one sound can't trigger itself twice.
- Hits during the slice capture are part of the slice and never retrigger.
- A hit after the capture restarts everything: the current repeats fade out over 3 ms and a new slice is captured.
- Capture includes 5 ms of audio from before the hit was detected (a rolling pre-roll buffer), so attacks aren't cut off.
- Slice, Length and Sync are fixed for a tail when it is triggered; changes apply from the next hit.

## Playback
- Each repeat gets a 3 ms fade-in and fade-out at its edges so slice boundaries never click.
- The last repeat is cut at the end of Length, with its 3 ms fade-out ending exactly on the end.
- A repeat that would start exactly at the end of Length is not played.
- Dry input always passes through at the Mix level. The curves affect only the repeats.
- The volume curve is a continuous envelope over time: what is drawn is what is heard, including within a repeat.
- If the host reports no tempo, the last known tempo is used (120 BPM until one arrives).
- The capture buffer holds 8 s (a 1/1 slice down to 30 BPM). Longer slices are clamped to 8 s.
- Bus layouts as in v0.1: mono → mono, mono → stereo, stereo → stereo.
  - Ping-Pong and Width do nothing on mono → mono.
- Tail length reported to the host is Length in seconds.
- Mix and Width changes are smoothed. Toggling Ping-Pong never clicks. Zero added latency. Works at 44.1–192 kHz.

## Pitch
- Pitch works like varispeed (tape): each repeat plays the captured slice faster or slower.
  - Higher pitch: the repeat's audio ends early, followed by silence until the next repeat starts.
  - Lower pitch: the repeat is cut off when the next repeat starts.
  - Repeats still start on the same grid (every Slice), whatever the pitch.
- Each repeat holds one pitch: the pitch curve's value at the moment that repeat starts. The original slice (the dry hit) is never repitched.
- Semitones = curve value mapped through Range and Bipolar; playback rate = 2^(semitones / 12).
- Range and Bipolar are read when each repeat starts, so changing them affects the next repeat.
- The 3 ms edge fades apply to the audio that is actually played, so a sped-up repeat fades out where its audio ends.
- Fractional read positions are interpolated (cubic). Anti-aliasing for large upward shifts is out of scope for now.

## Curve Editor
- Fills the area above the control strip. Two tabs at the top-left switch between the **Volume** and **Pitch** curves; both use the same editing gestures.
- The x-axis spans the full Length in bars, with beat grid lines.
- Volume tab: y-axis is volume, 0–100%. Default curve: a straight ramp from 100% at the start to 0% at the end.
- Pitch tab: y-axis is semitones, labelled with the top, bottom and no-change values for the current Range and Bipolar.
  - A line marks "no change" (the bottom in unipolar, the middle in bipolar).
  - Default curve: flat at the bottom (no pitch change in unipolar mode).
  - Switching Bipolar doesn't move the curve's points; it changes what they mean.
  - The Pitch Range control and the Bipolar switch sit in the curve area's top bar, visible only on the Pitch tab.
- Drag points to move them. Double-click empty space to add a point. Right-click a point to delete it.
- The first and last points stay at the start and end and move only vertically.
- Drag a segment up or down to bend it (curvature).
- Snap toggle, on by default. It snaps points to the beat grid horizontally; on the Pitch tab it also snaps them to whole semitones vertically.
- Faint vertical markers show where each repeat starts at the current Slice and Length.
- A playhead line shows the current position while repeats are playing.
- Curve edits apply immediately, even mid-tail (pitch edits from the next repeat).

## UI Layout
- Fixed 760 × 460 px window, not resizable. Every component is laid out relative to the editor bounds.
- Bottom strip, about 30% of the height: Slice / Division (one shared knob slot), Length (six segmented buttons: 1 / 2 / 4 / 8 / 16 / 32), Threshold, Mix, Width, then the Sync and Ping-Pong switches.
- All styling goes through the single `OCDelayLookAndFeel` class (still JUCE's default look).

## Out of Scope (for now)
- Separate repeat interval (gaps or overlaps between repeats)
- Bar-grid or MIDI triggering
- A "classic delay" mode (v0.1 lives in git history)
- Stepped per-repeat volume mode; continuous pitch glide within a repeat
- Time-preserving (granular) pitch shifting
- Curves for parameters other than volume and pitch
- Curve presets and undo/redo for curve edits
- Preset system, filters, modulation, saturation
- Custom UI graphics beyond JUCE's default look-and-feel
- Plugin formats other than VST3, Linux builds
- Code signing and notarization (needed before anyone else gets a build)
