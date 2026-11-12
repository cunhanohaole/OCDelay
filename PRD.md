# PRD: OCDelay v0.2 — Repeater

## Goal
OCDelay repeats a single-shot sound for an exact number of bars, then stops dead at the end, with a drawn curve shaping the volume across that time. It's built for one-off hits and phrases, not continuous audio (the name is a deliberate OCD joke about precise control).

v0.1 (shipped) was a classic feedback delay. v0.2 replaces it with a repeater.

## How it works
1. The plugin waits for a hit: the input jumps above the Threshold after silence.
2. It captures a slice of the input, Slice long, starting from the hit.
3. It replays that slice back to back until Length is reached, measured from the hit, then stops exactly there.
   - Example: a 1-bar slice with an 8-bar Length plays the original bar plus 7 repeats.
4. The volume curve shapes the repeats over the whole Length.

## Features
| Parameter | Type   | Range         | Default | Unit | Notes |
|-----------|--------|---------------|---------|------|-------|
| Slice     | float  | 1 – 2000      | 500     | ms   | Shown when Sync is off. Keeps the param ID `delayTime` |
| Division  | choice | 1/64 … 1/1    | 1/4     | note | Slice length when Sync is on (the same 17 steps as v0.1) |
| Sync      | bool   | off / on      | off     | —    | Off: Slice in ms. On: Slice is a note division at the host tempo |
| Length    | choice | 4, 8, 16, 32  | 8       | bars | Total time from the hit to the end of the last repeat. Always uses the host tempo |
| Threshold | float  | −60 – 0       | −30     | dB   | Input level that counts as a hit [NEED: confirm default by ear] |
| Mix       | float  | 0 – 100       | 50      | %    | 0 = dry only, 100 = repeats only |
| Ping-Pong | bool   | off / on      | off     | —    | Repeats alternate L, R, L… (input summed to mono) |
| Curve     | state  | —             | ramp down | —  | Volume envelope over Length. Saved with the session; not automatable |

- Feedback is removed. Its parameter ID `feedback` is retired and must never be reused.
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
- Dry input always passes through at the Mix level. The curve affects only the repeats.
- The curve is a continuous volume envelope over time: what is drawn is what is heard, including within a repeat.
- If the host reports no tempo, the last known tempo is used (120 BPM until one arrives).
- The capture buffer holds 8 s (a 1/1 slice down to 30 BPM). Longer slices are clamped to 8 s.
- Bus layouts as in v0.1: mono → mono, mono → stereo, stereo → stereo.
  - Ping-Pong does nothing on mono → mono.
- Tail length reported to the host is Length in seconds.
- Mix changes are smoothed. Toggling Ping-Pong never clicks. Zero added latency. Works at 44.1–192 kHz.

## Curve Editor
- Fills the area above the control strip.
- The x-axis spans the full Length in bars, with beat grid lines. The y-axis is volume, 0–100%.
- Default curve: a straight ramp from 100% at the start to 0% at the end.
- Drag points to move them. Double-click empty space to add a point. Right-click a point to delete it.
- The first and last points stay at the start and end and move only vertically.
- Drag a segment up or down to bend it (curvature).
- Snap to beat grid toggle, on by default (horizontal snapping only).
- Faint vertical markers show where each repeat starts at the current Slice and Length.
- A playhead line shows the current position while repeats are playing.
- Curve edits apply immediately, even mid-tail.

## UI Layout
- Fixed 640 × 400 px window, not resizable. Every component is laid out relative to the editor bounds.
- Bottom strip, about 30% of the height: Slice / Division (one shared knob slot), Length (four segmented buttons: 4 / 8 / 16 / 32), Threshold, Mix, then the Sync and Ping-Pong switches.
- All styling goes through the single `OCDelayLookAndFeel` class (still JUCE's default look).

## Out of Scope (for now)
- Separate repeat interval (gaps or overlaps between repeats)
- Bar-grid or MIDI triggering
- A "classic delay" mode (v0.1 lives in git history)
- Stepped per-repeat volume mode
- Curves for parameters other than volume
- Curve presets and undo/redo for curve edits
- Preset system, filters, modulation, saturation
- Custom UI graphics beyond JUCE's default look-and-feel
- Plugin formats other than VST3, Linux builds
- Code signing and notarization (needed before anyone else gets a build)
