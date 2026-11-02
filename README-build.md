# Build and Verify

P00 is a silent engineering scaffold: `PianoDSP` clears output to zero and does not yet implement a piano model. C++20 and CMake 3.22+ are required. JUCE 9.0.5 is pinned as a submodule at commit `d60b7a78030661574a615b586d9325ba7a7fa7dd`.

Initialize dependencies with `git submodule update --init --recursive`. Configure, build, and run CTest:

```sh
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

Render a stereo 48 kHz float WAV containing silence:

```sh
build/release/offline_render --output build/silence.wav --seconds 1
```

On macOS, the default build also creates AU, VST3, and Standalone JUCE targets. On Windows, use a Visual Studio generator and add `--config Debug` or `--config Release` to build and CTest commands; AU is macOS-only. Set `-DAETHER_BUILD_PLUGIN=OFF` to build the independent DSP/CLI/test subset without JUCE.
