# MuseScore VST3 Host Check

- Host: MuseScore Studio 4.5.2 on macOS 27.0.1, Apple M4 arm64.
- Plug-in: Aether Piano VST3, built from the Release target and copied to `~/Library/Audio/Plug-Ins/VST3/Aether Piano.vst3`.
- Discovery: after restarting MuseScore, `known_audio_plugins.json` listed the plug-in as enabled, type `VstPlugin`, category `Instrument|Synth`, vendor `Aether Piano`.
- Load: created a temporary piano score and selected `Aether Piano` as its mixer sound. The mixer displayed that name, and the saved score's `audiosettings.json` records `type: vst_plugin`, `id: Aether Piano`, and serialized component state.
- Playback: entered one C5 quarter note and started MuseScore playback. The transport advanced through the score without a crash. The plugin intentionally outputs digital silence in P00, so this verifies host integration and scheduling only, not audible sound.
- Reproduction artifact: [`MuseScore-VST3-test.mscz`](MuseScore-VST3-test.mscz).
- The temporary score was also saved by MuseScore to `~/Documents/MuseScore4/Scores/未命名乐谱.mscz`; the repository copy above is the P00 evidence artifact.
