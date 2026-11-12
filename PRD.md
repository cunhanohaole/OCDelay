# PRD: OCDelay v0.1

## Goal
A clean digital delay for people who want precise control over their repeats (the name is a deliberate OCD joke). v0.1 is a simple, solid delay that the later tail-length and curve features will be built on.

## Features
| Parameter  | Type  | Range       | Default | Unit | Notes |
|------------|-------|-------------|---------|------|-------|
| Delay Time | float | 1 – 2000    | 500     | ms   | Skewed so short times are easy to dial in |
| Feedback   | float | 0 – 95      | 35      | %    | Hard cap enforced in the DSP, not only the UI |
| Mix        | float | 0 – 100     | 50      | %    | 0 = dry only, 100 = wet only |
| Ping-Pong  | bool  | off / on    | off     | —    | Repeats alternate between left and right |

- VST3 only, macOS and Windows.
- Native JUCE UI: one rotary knob per float parameter, a toggle for Ping-Pong, value readouts with units.

## UI Layout
- Fixed window size, not resizable in v0.1: [NEED: confirm size — start at 640 × 400 px].
- Lay out every component relative to the editor bounds (no hard-coded pixel positions), so resizing can be added later without rewriting the layout.
- Controls sit in one strip along the bottom of the window: Delay Time, Feedback, Mix, Ping-Pong.
- The area above the strip is reserved for the future curve editor and stays empty in v0.1.
- All styling goes through a single custom `juce::LookAndFeel_V4` subclass, visually identical to JUCE's default for now.
- A full visual design pass happens after v0.1 ships and before the curve-editor work starts.

## Behaviour
- Supported bus layouts: mono → mono, mono → stereo, stereo → stereo.
- Stereo mode (Ping-Pong off): L and R are delayed independently with the same settings.
- Ping-Pong on: the input is summed to mono and the repeats alternate L → R → L.
- Ping-Pong also applies to a mono input on a stereo output.
- Ping-Pong has no effect on a mono → mono layout (there is nowhere to bounce to).
- Changing Delay Time while audio plays glides the pitch, tape-style (smoothed read position with interpolation, never a hard jump).
- Glide smoothing time: [NEED: confirm by ear — start around 100 ms].
- Feedback and Mix changes are smoothed: no zipper noise or clicks.
- Toggling Ping-Pong during playback must not click or pop.
- Zero added latency.
- Reports a tail length to the host so repeats ring out after the input stops.
- All parameters are automatable and are saved and restored with the DAW session.
- Works at 44.1–192 kHz; the delay buffer is sized in `prepareToPlay` for the maximum delay at the current sample rate.

## Future Direction (not v0.1 — shapes the architecture only)
- The point of OCDelay: the user sets exactly how long the repeats last (ms, seconds, or bars synced to project BPM), and repeats sound at full volume within that tail.
- A drawable curve editor (in the spirit of Serum 2's LFO editor or Bitwig's Curves device) shapes volume across the tail, and later other parameters.
- UX is a first-class requirement for these features.
- The maximum delay time will grow well beyond 2 s.
- v0.1 must keep the per-repeat gain and the maximum buffer length easy to change later, so these features can replace feedback as the gain source.

## Out of Scope (for now)
- Tempo sync / bars mode
- Tail-length control and the curve editor
- Preset system or preset browser
- Filters on the repeats (low cut / high cut)
- Modulation / wobble, saturation, tape character
- Custom UI graphics beyond JUCE's default look-and-feel
- Plugin formats other than VST3 (no AU, CLAP, AAX, or Standalone)
- Linux builds
- Code signing and notarization (needed before anyone else gets a build)
