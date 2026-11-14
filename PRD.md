# PRD: OCDelay v0.8 — Repeater with speed curve, free rate, Stop and Boost

## Goal
OCDelay repeats a single-shot sound for an exact number of bars, then stops dead at the end. Drawn curves shape the volume and the repeat speed across that time, so a single hit can turn into a riser. It's built for one-off hits and phrases, not continuous audio (the name is a deliberate OCD joke about precise control).

- v0.1 (shipped) was a classic feedback delay.
- v0.2 replaced it with a repeater and a volume curve.
- v0.3 added 1- and 2-bar lengths, a ping-pong Width control and a pitch curve.
- v0.4 added Free Rate: how often repeats start, set separately from how much audio is captured.
- v0.5 replaced the pitch curve with a speed curve: the time between repeats moves from the start rate towards an end rate, optionally with the pitch following it like tape.
- v0.6 added a Stop button that kills the repeats.
- v0.7 changes the defaults (Sync and Ping-Pong on, 4 bars, 1/8D, Max Speed on, a flat volume curve) and stops the repeats when the host's transport stops.
- v0.8 adds Boost: the fast part of a speed-curve riser can be made up to twice as loud.

## How it works
1. The plugin waits for a hit: the input jumps above the Threshold after silence.
2. It captures a slice of the input, Slice long, starting from the hit.
3. It replays that slice until Length is reached, measured from the hit, then stops exactly there.
   - By default the repeats play back to back: a new one starts every Slice. Example: a 1-bar slice with an 8-bar Length plays the original bar plus 7 repeats.
   - With Free Rate on, a new repeat starts every Rate instead, which can leave gaps (Rate longer than Slice) or overlaps (Rate shorter than Slice).
4. The volume curve shapes the repeats over the whole Length. The speed curve sets how often repeats start, from the start rate (Slice, or Rate with Free Rate on) towards the End Rate.

## Features
| Parameter   | Type   | Range               | Default   | Unit | Notes |
|-------------|--------|---------------------|-----------|------|-------|
| Slice       | float  | 1 – 2000            | 375       | ms   | Shown when Sync is off. Keeps the param ID `delayTime` |
| Division    | choice | 1/64 … 1/1          | 1/8D      | note | Slice length when Sync is on (the same 17 steps as v0.1) |
| Sync        | bool   | off / on            | on        | —    | Off: Slice and Rate in ms. On: both are note divisions at the host tempo |
| Free Rate   | bool   | off / on            | off       | —    | Off: repeats start every Slice. On: repeats start every Rate |
| Rate        | float  | 1 – 2000            | 375       | ms   | Time between repeat starts when Sync is off. Enabled only with Free Rate on |
| Rate Division | choice | 1/64 … 1/1        | 1/8D      | note | Time between repeat starts when Sync is on (the same 17 steps as Division). Enabled only with Free Rate on |
| Overlap     | bool   | off / on            | off       | —    | When Rate is shorter than Slice. Off: each repeat is cut (3 ms fade) when the next starts. On: repeats ring out and stack. Enabled only with Free Rate on |
| Length      | choice | 1, 2, 4, 8, 16, 32  | 4         | bars | Total time from the hit to the end of the last repeat. Always uses the host tempo |
| Threshold   | float  | −60 – 0             | −30       | dB   | Input level that counts as a hit [NEED: confirm default by ear] |
| Mix         | float  | 0 – 100             | 50        | %    | 0 = dry only, 100 = repeats only |
| Ping-Pong   | bool   | off / on            | on        | —    | Repeats alternate L, R, L… (input summed to mono) |
| Width       | float  | 0 – 100             | 100       | %    | How far apart Ping-Pong places the repeats. 100 = hard left/right, 0 = centred. Only has an effect with Ping-Pong on |
| End Rate    | float  | 1 – 2000            | 62.5      | ms   | Repeat interval at the top of the speed curve when Sync is off |
| End Rate Division | choice | 1/64 … 1/1    | 1/32      | note | Repeat interval at the top of the speed curve when Sync is on (the same 17 steps as Division) |
| Max Speed   | bool   | off / on            | on        | —    | On: the top of the speed curve is the fastest rate (Max interval, audio rate) instead of End Rate |
| Pitch Follow| bool   | off / on            | off       | —    | Off: repeats keep the original pitch. On: each repeat plays faster or slower in proportion to its interval, like tape |
| Boost       | float  | 0 – 100             | 0         | %    | Extra loudness that rises with the speed curve (see Boost). 0 = none, 100 = twice as loud (+6 dB) at the top of the curve |
| Stop        | bool   | off / on            | off       | —    | Momentary: on while the Stop button is held. Switching it on stops the repeats (see Triggering). Automatable, so a DAW can stop a tail at a set point |
| Volume curve| state  | —                   | flat at 100 % | — | Volume envelope over Length. Saved with the session; not automatable |
| Speed curve | state  | —                   | flat at bottom | — | Repeat speed over Length. Bottom = start rate, top = End Rate (or Max). Saved with the session; not automatable |

- Feedback is removed. Its parameter ID `feedback` is retired and must never be reused.
- Pitch Range and Bipolar are removed with the pitch curve in v0.5. Their IDs `pitchRange` and `pitchBipolar` are retired and must never be reused. A saved pitch curve is ignored on load.
- Length gained 1 and 2 bars before v0.2 was ever released, so its choice list changed in place. From v0.3 on, its choices only ever get appended.
- VST3 only, macOS and Windows.

## Triggering
- A hit is the input level rising above Threshold.
- A new hit is only accepted after the input has stayed below Threshold for [NEED: confirm — start at 50 ms] (re-arm), so one sound can't trigger itself twice.
- Hits during the slice capture are part of the slice and never retrigger.
- A hit after the capture restarts everything: the current repeats fade out over 3 ms and a new slice is captured.
- Stop kills the current repeats: they fade out over 3 ms, the same as a restart, and nothing new is captured. The next hit starts a new tail as usual.
- Stopping the host's transport (e.g. Space in the DAW) does the same as Stop. Only the moment playback stops counts: hits played live while the transport is stopped still repeat.
- Capture includes 5 ms of audio from before the hit was detected (a rolling pre-roll buffer), so attacks aren't cut off.
- Slice, Rate, Free Rate, Overlap, Length and Sync are fixed for a tail when it is triggered; changes apply from the next hit. End Rate, Max Speed and Pitch Follow are read as each repeat starts. Boost is smoothed and applies immediately.

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

## Free Rate
- With a flat speed curve at the bottom, repeat k (k ≥ 1) starts exactly k × Rate after the capture start. The speed curve changes the gaps (see Speed).
- Each repeat plays the captured slice from its beginning (very short repeats from the hit; see Speed). A repeat that starts while the slice is still being captured only plays audio that has already been captured; a sped-up repeat that would catch up with the capture ends there (with its fade-out).
- Rate longer than Slice: silence between repeats.
- Rate shorter than Slice, Overlap off: each repeat ends where the next begins, its 3 ms fade-out ending exactly on the next start.
- Rate shorter than Slice, Overlap on: repeats ring out fully and are summed. At most 16 repeats sound at once: a repeat is cut, with its fade, when the 16th repeat after it starts. There is no automatic gain compensation; the volume curve and Mix set the level.
- Each repeat keeps its own pitch and ping-pong side; the volume curve is one envelope over time applied to all of them.
- With Free Rate off and a flat speed curve at the bottom, playback is exactly as in v0.2 (back to back, every Slice).

## Speed
- The start rate is Slice (Free Rate off) or Rate (Free Rate on). The end rate is End Rate / End Rate Division (following Sync, like Slice and Rate), or Max when Max Speed is on.
- Max is a 5 ms interval (about 200 repeats per second), fast enough that the repeats blur into a pitched buzz. [NEED: confirm by ear]
- The curve's value v (0 at the bottom, 1 at the top) sets the interval on a square-root scale: interval = (√start + (√end − √start) × v)². A straight line therefore builds steadily and rushes towards the end. Bend a segment upwards for an earlier rush, downwards for a later one.
  - Chosen by ear between a log scale (reached the fast end around halfway) and equal milliseconds (only got fast at the very end). From 1/4 at 120 BPM to Max, a straight line gives about 300 / 150 / 53 / 18 ms at 25 / 50 / 75 / 90 % of Length.
  - An end rate longer than the start rate makes the repeats slow down instead; nothing else changes.
- Each repeat's interval is the curve's value at the moment it starts: repeat k+1 starts one interval after repeat k. The first repeat starts one interval after the capture start.
- A sped-up repeat follows the Free Rate rules: with Overlap off it ends where the next begins (3 ms fade-out, or half the interval when that's shorter); with Overlap on repeats stack, up to 16 at once. When Free Rate is off, the speed curve behaves as Free Rate with Overlap off.
- A repeat that starts while the slice is still being captured only plays audio that has already been captured.
- Short repeats keep less of the 5 ms pre-roll, so the quiet lead-in before the hit doesn't take over a repeat and make it quieter. Measured in slice audio a repeat can play (interval × playback rate, or 16 intervals with Overlap on):
  - a repeat keeps a sixth of its length of pre-roll, and never less than its fade-in, so the attack always lands at full gain;
  - from 30 ms (6 pre-rolls) on it keeps the whole pre-roll and starts at the slice start, as before;
  - the amount grows smoothly with the repeat's length, so a riser never jumps.
- Edge fades are min(3 ms, a quarter of the repeat). Half the repeat (a pure fade in, then out) lost about 5 dB at audio rates; at those rates the edges are part of the buzz's tone, not clicks.

### Pitch Follow
- Off: every repeat plays at the original pitch and speed.
- On: works like varispeed (tape). A repeat's playback rate = start rate interval ÷ its own interval, so half the gap plays twice as fast (+12 st) and the audio fits its shorter slot.
  - Pitch is continuous, never rounded to semitones, so it glides with the speed.
  - Each repeat holds the playback rate it started with. With fast repeats the steps are too short to hear as steps.
  - Slower than the start rate: lower pitch, and the repeat is cut where the next begins.
- The original slice (the dry hit) is never repitched.
- Fractional read positions are interpolated (cubic). Anti-aliasing for large upward shifts is out of scope for now.

### Boost
- Makes the fast part louder, to make up for the loudness short repeats lose to their fades and to the sound's own attack.
- Wet gain = 1 + Boost × the speed curve's height at that moment: none at the bottom (the start rate), the full amount at the top. It glides with the curve, so there are no steps.
- It multiplies with the volume curve. With a flat speed curve at the bottom (the default) it does nothing.
- Up to +6 dB can take the output over 0 dBFS; there is no limiter.

## Curve Editor
- Fills the area above the control strip. Two tabs at the top-left switch between the **Volume** and **Speed** curves; both use the same editing gestures.
- The x-axis spans the full Length in bars, with beat grid lines.
- Volume tab: y-axis is volume, 0–100%. Default curve: flat at 100% from start to end.
- Speed tab: y-axis is the repeat interval, labelled with the start rate at the bottom and the end rate (or "Max") at the top.
  - Default curve: flat at the bottom (no speed change).
  - Changing the start or end rate doesn't move the curve's points; it changes what they mean.
  - The Boost and End Rate controls and the Max Speed and Pitch Follow switches sit in the curve area's top bar, visible only on the Speed tab. End Rate is greyed out while Max Speed is on.
- Drag points to move them. Double-click empty space to add a point. Right-click a point to delete it.
- The first and last points stay at the start and end and move only vertically.
- Drag a segment up or down to bend it (curvature).
- Snap toggle, on by default. It snaps points to the beat grid horizontally only, on both tabs.
- Faint vertical markers show where each repeat starts at the current Slice (or Rate), speed curve and Length. Where repeats come faster than about one per 4 px, the markers merge into a shaded band.
- A playhead line shows the current position while repeats are playing.
- Curve edits apply immediately, even mid-tail (speed edits from the next repeat).

## UI Layout
- Fixed 860 × 460 px window, not resizable. Every component is laid out relative to the editor bounds.
- Bottom strip, about 30% of the height: Slice / Division (one shared knob slot), Rate / Rate Division (one shared slot), Length (six segmented buttons: 1 / 2 / 4 / 8 / 16 / 32, with a Stop button below them), Threshold, Mix, Width, then the Sync, Free Rate, Overlap and Ping-Pong switches.
- The Rate knob and the Overlap switch are greyed out and can't be changed while Free Rate is off.
- All styling goes through the single `OCDelayLookAndFeel` class (still JUCE's default look).

## Out of Scope (for now)
- Bar-grid or MIDI triggering
- A "classic delay" mode (v0.1 lives in git history)
- Stepped per-repeat volume mode; speed or pitch changing within a single repeat
- Time-preserving (granular) pitch shifting
- A separate pitch curve; semitone-quantised pitch
- Curves for parameters other than volume and speed
- Curve presets and undo/redo for curve edits
- Preset system, filters, modulation, saturation
- Custom UI graphics beyond JUCE's default look-and-feel
- Plugin formats other than VST3, Linux builds
- Code signing and notarization (needed before anyone else gets a build)
