# ADR-0001: P00 JUCE and plugin baseline

- Status: accepted for P00; licensing review remains open before distribution.
- Context: the repository contained project documents only. The project targets C++20 with an independent DSP core and JUCE plugin adapters.
- Decision: pin JUCE 9.0.5 at commit `d60b7a78030661574a615b586d9325ba7a7fa7dd` as a Git submodule. Build VST3, Standalone, and macOS AU. Disable `JUCE_VST3_CAN_REPLACE_VST2` because no prior VST2 product or parameter IDs exist. Keep the DSP library independent of JUCE.
- Consequences: the plugin now compiles on Apple Silicon with the current Xcode toolchain; plugin distribution remains subject to selecting JUCE's applicable license. The current processor intentionally emits silence and is not a playable instrument.
- Evidence: P00 build and validation logs in `experiments/P00/`.
