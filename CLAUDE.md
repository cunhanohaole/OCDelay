# OCDelay

## What This Is
A free VST3 repeater for single-shot sounds: it captures a slice after a hit and repeats it for an exact number of bars, with a drawn volume curve. The spec is in `PRD.md` — build only what it lists.

## Tech Stack
- **Framework:** JUCE 9 (git submodule in `JUCE/`), native JUCE UI with the default look-and-feel
- **Language:** C++23 (set by `pamplejuce_shared_code_defaults()`)
- **Build:** CMake from the Pamplejuce template; CPM for non-JUCE libraries
- **Format:** VST3 only. **Platforms:** macOS 11+ (universal arm64 + x86_64) and Windows
- **IDE:** Xcode. **Tests:** Catch2 (`tests/`); CI runs pluginval
- **CI:** GitHub Actions on a private repo — watch the CI minutes
- **Code signing:** not set up yet; it's required before distributing builds to anyone

## Build Commands
```bash
cmake -B Builds -G Xcode                      # configure; then open Builds/OCDelay.xcodeproj
cmake --build Builds --config Debug           # build from the CLI
ctest --test-dir Builds -C Debug --output-on-failure
```
Builds default to Debug. Use `--config Release` when judging CPU use or audio dropouts.

## Project Identity (don't change without asking)
- Company `Sagaz`, bundle ID `com.sagaz.ocdelay`, manufacturer code `Sagz`, plugin code `OcDl`. Changing the codes breaks saved DAW sessions.
- CLAP, Intel IPP, Linux CI, and code signing / notarization are commented out in `CMakeLists.txt` and `.github/workflows/build_and_test.yml`. Re-enable signing before distributing.

## Architecture
- `PluginProcessor` owns the APVTS and composes the DSP classes. It contains no raw DSP math.
- DSP (trigger detection, capture, playback) lives in its own class(es) under `source/dsp/`, with no dependency on the editor or APVTS, so it can be unit-tested directly.
- The volume curve is model data, separate from its editor component. The audio thread reads a lookup table rebuilt on the message thread and swapped in lock-free; it never touches the editable curve.
- Define all parameters in one `createParameterLayout()` function, with IDs in a single constants header. Never rename a parameter ID once it ships.
- The editor only reads and writes parameters through APVTS attachments. Deleting the editor must not affect the audio.
- Plugin sources link through the `SharedCode` INTERFACE library so that `Tests` sees the same code.

## Coding Principles
- **Single responsibility.** Every function does one thing; if describing it needs an "and", split it.
- **Small functions** (~30 lines max). Extract sub-steps into well-named functions.
- **No god classes.** Prefer composition over deep inheritance.
- **Descriptive names over comments.** Comment *why*, not *what*.
- **Fail early and clearly.** Validate at boundaries; use `jassert` for invariants.

## Code Style
- Use `.clang-format`: Allman braces, 4-space indent, no column limit.
- Use camelCase for methods and variables, PascalCase for classes, and an explicit `juce::` prefix (no `using namespace juce`).
- Use RAII and `std::unique_ptr`. No owning raw pointers.

## Realtime / DSP Rules
- `processBlock` never allocates, locks, logs or does I/O. Allocate in `prepareToPlay`.
- Smooth every audio-rate parameter (`juce::SmoothedValue`). Fade slice edges and restarts so nothing clicks.
- Retired parameter IDs (e.g. `feedback`) are never reused.
- Prefer `juce::dsp` building blocks before writing custom DSP.
- Audio → UI goes through atomics polled by a `juce::Timer`. Never call UI code from the audio thread.

## What NOT To Do
- Don't add features, formats or parameters that aren't in `PRD.md`.
- Don't add third-party dependencies without asking.
- Don't put DSP in editor code, or UI code in the processor.
- Don't leave compiler warnings. Treat them as errors and fix them before calling a task done.
- Ignore clangd/LSP "undeclared identifier"/"file not found" errors unless the real build fails (JUCE modules confuse it).
