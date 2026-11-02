# PHASE_REPORT — 阶段验收报告模板

- Phase：Pxx
- 日期：
- 负责人：
- Commit / Branch：
- OS / CPU / Toolchain / JUCE commit：

## 交付清单

- [ ] 源代码 / 数据格式 / 文档
- [ ] 自动测试
- [ ] 离线 WAV/图表（适用时）
- [ ] 性能分析（适用时）
- [ ] 第三方许可检查（适用时）

## 实际运行命令和结果

| Command | PASS/FAIL/SKIP/NOT RUN | 日志路径 | 原因（未运行时） |
|---|---|---|---|
| `cmake ...` |  |  |  |
| `ctest ...` |  |  |  |
| `offline_render ...` |  |  |  |
| `pluginval ...` |  |  |  |

## 声学结果

- 测试输入：
- 基频与分音：
- 接触/能量：
- 衰减/T60：
- 动态：
- A/B：

## RT 约束与性能

- 线程分配/锁检查：
- 采样率、块大小、复音数：
- CPU / p50 / p99 / max：
- 风险与已知近似：

## 阶段门结果

**PASS / FAIL / BLOCKED**：

未通过项：

进入下一阶段的条件：
