# P03 阶段验收报告：88 键与复音

- 日期：2026-11-03
- 分支：`master`（基于 `c51743f`）
- 环境：macOS 27.0 arm64，Apple M4；Apple Clang 21.0.0，CMake 4.4.0，Ninja；JUCE 子模块 `d60b7a78030661574a615b586d9325ba7a7fa7dd`

## 交付清单

- `NoteParameters` 为 MIDI 21–108 建立每键参数曲线，未实测的材料参数明确标为估计；分离目标基频、拉伸调律与非谐性 B。
- `PerformanceEngine` 使用固定 88 键槽处理 MIDI sample offset、同音复奏、重叠 Note Off、CC64 延音、释放回收和静音滞后。回归覆盖踏板反序事件；踏板按下不会复活已开始衰减的音。
- 增加实时性能测试、离线渲染与复音 benchmark；更新参数规范、README 和 ADR-0004。
- 色散滤波器新增 A0/C8、44.1–192 kHz 的稳定性/群延迟/频响边界测试；实时分配探针覆盖 64 声部、CC64 与 Note Off。
- MuseScore C4 与四音和弦宿主 WAV、MusicXML/MSCZ、MIDI 事件核验、分析结果及构建、测试、签名日志均保存在 `experiments/P03/`。

## 实际运行命令和结果

| 命令 | 结果 | 记录 |
|---|---|---|
| `cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPython3_EXECUTABLE="$PWD/.venv/bin/python"` | PASS | `configure-debug.log` |
| `cmake --build build/debug --parallel` | PASS | `build-debug.log` |
| `ctest --test-dir build/debug --output-on-failure` | PASS，5/5 | `ctest-debug.log` |
| `cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DPython3_EXECUTABLE="$PWD/.venv/bin/python"` | PASS | `configure-release.log` |
| `cmake --build build/release --parallel` | PASS | `build-release.log` |
| `ctest --test-dir build/release --output-on-failure` | PASS，5/5，31.92 s | `ctest-release.log` |
| `cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPython3_EXECUTABLE="$PWD/.venv/bin/python"` | PASS | `configure-debug.log` |
| `cmake --build build/debug --parallel` | PASS | `build-debug.log` |
| `ctest --test-dir build/debug --output-on-failure` | PASS，5/5，45.03 s | `ctest-debug.log` |
| `.venv/bin/python tools/analyzer/analyze_p03.py --renderer build/release/performance_render` | PASS，440 个音高配置及 4 个事件场景 | `analysis-release.log` |
| `build/release/polyphony_bench` | 命令 PASS；记录 `PerformanceEngine::process` 耗时，deadline 验收 FAIL | `polyphony-bench-final.log` |
| `build/release/plugin_callback_bench` | PASS，实调 JUCE `AudioProcessor::processBlock`；零普通 `new/new[]`、零非有限输出；32/64 声部有少量壁钟超时 | `plugin-callback-bench-release.log` |
| `auval -v aumu AeP0 Aeth` | PASS | `auval-release.log` |
| `codesign --verify --deep --strict --verbose=2 ~/Library/Audio/Plug-Ins/VST3/Aether\ Piano.vst3` | PASS | `codesign-vst3.log` |
| `.venv/bin/python experiments/P03/prepare_musescore_score.py` | PASS；MIDI 音符为 60、64、67、72，tick 均为 0 | `musescore-score.log`、`musescore-midi-validation.json` |
| `.venv/bin/python experiments/P03/analyze_musescore_export.py` | PASS；MuseScore C4 单音宿主输出 | `musescore-host-validation.log`、`musescore-host-validation.json` |
| `'/Applications/MuseScore 4.app/Contents/MacOS/mscore' -o experiments/P03/MuseScore-P03-four-note-chord-native.wav experiments/P03/MuseScore-P03-four-note-chord-native.mscz` | FAIL；MuseScore CLI 生成 0 字节 WAV | `musescore-native-render.log` |
| MuseScore Studio GUI：打开 `MuseScore-P03-four-note-chord-native.mscz`，File > Export，WAV、44.1 kHz、Main Score | PASS；该谱的 Piano 声部音源为 Aether Piano VST3，输出结构与四音频谱核验通过 | `musescore-native-chord-export.log`、`musescore-native-chord-analysis.log` |
| `.venv/bin/python experiments/P03/analyze_native_chord.py` | PASS；格式、有限值与四个目标频段粗检 | `musescore-native-chord-analysis.log` |
| `pluginval` | NOT RUN；本机未安装 | — |

## 声学结果

- MIDI 21–108 在 44.1、48、96、192 kHz 等调律，以及 48 kHz、拉伸 0.25 下完成测量。440 次基频误差范围为 **−0.0024 至 +0.0032 cents**。
- 音阶、琶音、四音和弦、同音复奏均无非有限输出，结束时活动声部为 0；峰值活动声部分别为 16、13、4、1。B 从 0 改为估计值时，A0/C4/C8 的最大基频变化为 **0.001267 cents**。
- MuseScore 4.5.2 通过已安装的 Aether Piano VST3 导出的 C4 WAV 为 44.1 kHz 双声道；测得 261.6298 Hz，对 A4=440 等律目标误差 **0.0282 cents**，无 NaN/Inf。
- 四音和弦原生 MusicXML→MuseScore→MIDI 路径已验证音符为 C4/E4/G4/C5；MuseScore GUI 的钢琴声部加载 Aether Piano VST3，并成功导出 44.1 kHz、双声道、32-bit float、3.5 s WAV。154,350 帧均为有限值；全文件 RMS 为 0.174，峰值为 1.627（float 满幅以上，非整数 PCM 截幅；下游余量仍需评估）。0.1–0.8 s 窗口内粗 FFT 在四个目标基频附近均检测到谱峰，结果见 `musescore-native-chord-analysis.log`。该频谱检查只确认四音输出存在，不代替精确调律或音色真实性验收。主观听音检查未执行；没有经校准钢琴数据可用于音色真实性验收。

## 实时约束与阶段门

- 自动测试覆盖 `PerformanceEngine::process` 零 C++ 分配、MIDI 偏移、变块长、零长度块、88 键边界和复奏/释放状态。插件最低支持采样率为 44.1 kHz。
- 48 kHz 下引擎基准的 p99 低于块预算，但单次最大值超预算：64 帧/32 声部 **1.866 ms > 1.333 ms**，64 声部 **3.272 ms > 1.333 ms**；128 帧/32 声部 **3.296 ms > 2.667 ms**，64 声部 **6.365 ms > 2.667 ms**。本次平均 CPU 为 **0.18%–4.33%**。
- 新增 `plugin_callback_bench` 直接实例化插件处理器并测量 JUCE `processBlock`。64 帧块/32 声部有 **3/2250** 次超预算（最大 **1.642 ms**），64 声部有 **3/2250** 次（最大 **3.121 ms**）；128 帧块/32 声部有 **2/1125** 次（最大 **3.251 ms**），64 声部有 **2/1125** 次（最大 **5.983 ms**）。其余场景无超预算；所有 p99 均低于块预算，CPU 为 **0.17%–4.00%**。超预算集中在各场景开始的前几个回调。此工具运行在普通进程线程，不模拟 DAW 音频线程优先级或宿主调度。
- 插件回调探针记录到 64 声部峰值输出 **15.72**（float 样本单位）；四音 MuseScore WAV 峰值 **1.627**。音源仍输出未声压标定的弦速度量值，未加 limiter；与满幅的映射和余量需由后续辐射/输出模型处理。
- `plugin_callback_bench` 与单元测试探针只拦截全局普通 `new/new[]`；64 声部测试未见分配，直接 `malloc/free` 与锁 API 未做运行时拦截。回调静态路径检查未见锁、文件/网络访问或同步日志。插件没有可调 GUI 参数，GUI 调参场景为 **NOT RUN**。
- Release 插件构建日志包含 JUCE ad-hoc 签名修正提示；安装后的 VST3 `codesign --verify` 通过。AU Validator 通过。

## 被拒绝的实验

- 手工 MSCX 曾将目标 G4 导出为 MIDI C♯4，判为 **REJECTED / SUPERSEDED**。临时 MSCX 在本次实验记录前已移除，原件无法恢复；保留的拒收说明见 `rejected-manual-musescore-score.md`。有效替代谱的 MIDI 和 MuseScore Aether Piano VST3 宿主 WAV 均已验证。

**阶段门：BLOCKED。** P03 代码、离线音高回归、插件格式验证及 MuseScore Aether Piano VST3 四音宿主渲染已交付。真实 `processBlock` 基准补齐了适配层时间和 C++ 分配数据，但仍观察到块预算超时；直接 `malloc/free` 与运行时锁计数也未覆盖。没有目标宿主实时调度下稳定复音的证据前，暂不进入 P04。
