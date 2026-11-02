# P00 Environment

- Date: 2026-11-02 (Asia/Shanghai)
- OS: macOS 27.0.1, Darwin 27.0.0
- CPU / architecture: Apple M4 / arm64
- Compiler: Apple clang 21.0.0 (`clang-2100.4.1.1`)
- Build tools: CMake 4.4.0, Ninja (available on PATH), Xcode 27.2 (Build 27B52)
- Python: 3.14.6
- Git: 2.55.0
- Codex CLI: 0.166.0-alpha.5.1
- JUCE: tag 9.0.5, exact commit `d60b7a78030661574a615b586d9325ba7a7fa7dd` (submodule `external/JUCE`)
- Build architecture: arm64, explicitly set with `CMAKE_OSX_ARCHITECTURES=arm64`.
- Note: `xcrun --show-sdk-version` failed because the selected Command Line Tools SDK path was not found, although CMake configured and compiled all targets using the installed Xcode toolchain. SDK lookup environment should be checked on this machine before relying on other Apple build tools.
- MuseScore Studio 4.5.2 is installed and was used to scan, select, and play a test score through the VST3 target. Apple's `auval` also passed the AU target. No `pluginval` or Steinberg Validator executable was found on PATH.
