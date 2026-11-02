# P02 阶段报告 — C4 实时双向波导

- Phase：P02 — C4 实时单弦
- 日期：2026-11-03
- 负责人：Codex
- Commit / Branch：P02 交付 commit 在 `master`；精确提交号可由 `git log -1 --oneline` 查询。
- 环境：Apple M4 / arm64，macOS 27.0.1，Apple Clang 21.0.0，CMake 4.4.0，Ninja，JUCE 9.0.5（submodule `d60b7a78030661574a615b586d9325ba7a7fa7dd`），Python 3.14.6；MuseScore Studio 4.5.2。P01 Python 包版本见 [`../P01/environment.md`](../P01/environment.md)。

## 交付清单

- [x] C++20 DSP：`src/dsp/string/` 的波导、分数延迟、琴槌、色散与损耗；`PianoEngine`、JUCE MIDI 适配和离线渲染更新。
- [x] 单元/数值回归：`tests/test_main.cpp` 与 `tools/analyzer/analyze_p02.py`；覆盖分数延迟、滤波稳定性、接触冲量/动量、采样偏移、重触发、Note Off、极端速度和回调分配探针。
- [x] 速度 24/80/120 与采样率 WAV、P01 特征比较 CSV/图、Release 单声部基准。
- [x] MuseScore 4.5.2 VST3 实际导出、AU `auval` 验证和 VST3 签名检查。
- [x] 算法边界和依据：[`ADR-0003`](../../docs/decision-log/ADR-0003-p02-waveguide.md)。

## 实际运行命令和结果

| Command / check | 状态 | 日志或证据 |
|---|---|---|
| `cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 -DPython3_EXECUTABLE="$PWD/.venv/bin/python"`；`cmake --build build/debug --parallel 4` | PASS | [`configure-debug.log`](configure-debug.log), [`build-debug.log`](build-debug.log) |
| `ctest --test-dir build/debug --output-on-failure` | PASS，3/3 | [`ctest-debug.log`](ctest-debug.log) |
| `cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DPython3_EXECUTABLE="$PWD/.venv/bin/python"`；`cmake --build build/release --parallel 4` | PASS | [`configure-release.log`](configure-release.log), [`build-release.log`](build-release.log) |
| `ctest --test-dir build/release --output-on-failure` | PASS，3/3 | [`ctest-release.log`](ctest-release.log) |
| `.venv/bin/python tools/analyzer/analyze_p02.py --render build/release/offline_render` | PASS | [`analysis-render.log`](analysis-render.log)、CSV 与 [`feature_comparison.png`](feature_comparison.png) |
| `./build/release/rt_bench`（48 kHz、单 C4、10 秒、4 种块长） | PASS | [`rt-bench-release.log`](rt-bench-release.log) |
| `ditto "$PWD/build/release/AetherPianoPlugin_artefacts/Release/VST3/Aether Piano.vst3" "/Users/zhangbo/Library/Audio/Plug-Ins/VST3/Aether Piano.vst3"`；`ditto "$PWD/build/release/AetherPianoPlugin_artefacts/Release/AU/Aether Piano.component" "/Users/zhangbo/Library/Audio/Plug-Ins/Components/Aether Piano.component"` | PASS | Final Release bundles installed for the host checks |
| `codesign --verify --deep --strict --verbose=2 "/Users/zhangbo/Library/Audio/Plug-Ins/VST3/Aether Piano.vst3"` | PASS | [`vst3-codesign.log`](vst3-codesign.log)；构建版与安装版二进制 SHA-256 相同，见 [`installed-plugin-sha256.log`](installed-plugin-sha256.log) |
| `auval -v aumu AeP0 Aeth` | PASS | [`auval.log`](auval.log) |
| MuseScore Studio 4.5.2 载入最终 VST3 并导出 C4 WAV | PASS | [`musescore-validation.md`](musescore-validation.md)、[`musescore-host-validation.json`](musescore-host-validation.json) |
| `pluginval` / Steinberg VST3 Validator | NOT RUN：工具未安装 | MuseScore 实际宿主与 AU `auval` 已覆盖；这不替代专用 Validator |

## 声学结果

测试参数是标注为 `synthetic_research` 的 C4（目标 261.625565 Hz，`B=0.0003`），并非实琴测量。44.1/48/96 kHz 测得基频分别为 261.625566/261.625563/261.625563 Hz，最大误差约 0.00002 cents，非有限状态为 0。速度 24/80/120 时，槌速为 0.30/1.0667/1.76 m/s，初始能量为 0.00036/0.004551/0.012390 J；峰值力 2.002/7.976/13.751 N，接触 4.229/3.313/3.208 ms。三档波导冲量与槌动量变化差在记录精度下为 0。

基频 T60 用 6 秒 WAV 中 0.5–5.0 秒、0.4 秒 lock-in 窗口线性拟合并外推，结果约 210.046 秒；这是很长的合成衰减参数，不是实琴衰减测量。`B=0` 的泛音保持谐波；`B=0.0003` 时第 2–6 分音约为 523.486、785.798、1048.740、1312.448、1577.010 Hz，随阶数升高且接近刚性弦理论。力度 24→120 的攻击谱质心由约 691 Hz 升至 1280 Hz。对齐 P01 瞬态相关系数为 0.950–0.995，相对 RMS 差为 9.8%–31.2%；两种离散模型的波形仍有明显差异，见 `feature_results.csv`。

## 实时约束、限制与阶段门

`PianoEngine::process` 的全局分配计数探针为 0；该探针不覆盖宿主/JUCE 的整个回调。Release 单声部 CPU 占比在 64/128/256/512 样本块下为 0.063/0.060/0.059/0.059%，p99 分别 0.000917/0.001667/0.003709/0.007375 ms，最坏块测量 0.067–0.145 ms。该微基准只测一根弦，不代表高复音或 DAW 最坏调度。

已知近似：仅一根 C4、固定端反射、单一击弦/拾音点、Note Off 用基频环路增益近似制音；没有实琴标定、琴桥、音板、辐射、多弦或完整 MIDI 音域。MuseScore 验证证明当前本机版本可加载并渲染此 VST3，不代表跨宿主/平台 QA 完成。AU codesign 单独验证曾给出资源签名警告，因此此处只报告 `auval` PASS，不报告 AU 签名 PASS。

**阶段门：PASS（P02 范围）。** Debug/Release CTest、离线特征回归、跨采样率调音、无分配探针、单音性能和实际宿主导出均有证据。停止在 P02；P03 复音及后续琴桥/音板仍需独立任务与验收。
