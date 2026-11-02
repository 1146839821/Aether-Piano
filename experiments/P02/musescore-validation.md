# MuseScore VST3 宿主验证

- 宿主：本机 MuseScore Studio 4.5.2（`musescore-version.txt`；应用包路径 `/Applications/MuseScore 4.app`）。
- 插件：仓库 Release 构建 `Aether Piano.vst3`，安装到 `~/Library/Audio/Plug-Ins/VST3/Aether Piano.vst3`。
- 测试：打开 `MuseScore-P02-C4-test.mscz`，确认 Mixer 音色通道加载 “Aether Piano”，播放 MIDI C4，并从 MuseScore 导出 WAV。
- 结果：44.1 kHz、双声道；截取有效播放前 3 秒后，活动区为 0–1.42 秒；估计基频 261.62559 Hz（误差 0.000171 cents），峰值 0.96218，非有限样本 0。原始导出 67 秒，其余主要是静音尾部。
- 最终 Release VST3 在 MuseScore 重启后重新扫描并导出；其安装版与构建版二进制 SHA-256 相同（`installed-plugin-sha256.log`）。
- 机器证据：`musescore-host-validation.log`、`musescore-host-validation.json`、`MuseScore-VST3-C4-playback.wav`、`installed-plugin-sha256.log`。测试谱生成过程：`prepare_musescore_score.py`。
- 边界：这验证了 MuseScore 可加载 VST3 并实际渲染本机插件；不代表完整 DAW 兼容性、全音域能力或实琴音色验证。AU 另由 `auval` 检查。
