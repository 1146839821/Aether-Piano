# ADR-0003: P02 C4 双向波导结构

- 状态：Accepted（P02 范围）
- 日期：2026-11-02
- 背景及约束：P01 的高精度模态/琴槌求解器可作为离线参照；P02 需要独立于 JUCE、可在音频回调运行的 C4 单弦模型。首要约束是稳定、可复现的基频/频散/损耗，以及无实时分配。
- 候选方案：继续使用截断模态模型；用脉冲激励叠加滤波器；采用双向行波数字波导并耦合非线性琴槌。选择第三种，以保留往返传播、撞击点反作用和可解释的边界。
- 比较指标：基频调音误差、B=0/B>0 泛音、T60、琴槌冲量与动量、P01 瞬态对齐误差、回调分配和单音 CPU。
- 实验来源：`experiments/P02/feature_results.csv`、`dispersion_results.csv`、`sample_rate_results.csv`、CTest 和 Release RT 基准；参数为 `synthetic_research`，并非实琴测量。

## 决策

弦波以横向速度行波表示。弦张力 `T` 与线密度 `μ` 给出特性阻抗 `Z = sqrt(T μ)`；在击弦点，入射速度波与接触力按 `F/(2Z)` 散射。琴槌用单边压缩律 `F = k max(δ, 0)^p`，通过固定 32 次二分迭代隐式求解离散力，不在 `process` 中分配内存。

两侧固定端反射采用速度反相。整数延迟加一阶 Thiran allpass 提供分数延迟；`prepare` 根据往返相位解算右侧延迟，使损耗、频散和延迟合成后 C4 基频命中目标。引擎先读出已存波、后算本采样的出射波，因此状态交接隐含一个样本；显式环路和拾音延迟各补偿一采样，避免整体音高偏移。

频率相关损耗采用对称、非负三抽头 FIR，系数和受限以保持被动衰减，并在目标基频标定 T60。刚性弦频散采用两个一阶 allpass 串联；`B=0` 完全旁路，`B>0` 调节相位以贴近第二分音的理论偏移。Note Off 先用基频环路增益近似制音；它不是 P06 的制音器机械模型。

P02 保留一根 C4、一处琴槌/拾音点、固定端和单声道双通道复制。没有多弦、琴桥机械负载、音板、辐射、实琴拟合或 MIDI 全音域映射。插件及离线 WAV 输出是拾音点横向速度（m/s），不能解释为声压或实琴音色结果。

- 已知缺点：波导攻击与 P01 对齐相关约 0.95–0.995，但相对 RMS 差约 9.8%–31.2%；这是不同离散模型与有限步长参考之间的差异，尚无实琴数据判断听感。基频长 T60 约 210 秒属于合成参数结果。
- 回滚策略：撤销 `src/dsp/string/` 与 `PianoEngine`/插件适配变更，恢复 P01 离线参考路径；P01 代码不依赖本决策。
- 待验证事项：P03 复音下的实时余量；P04 多弦/琴桥无源耦合；P07 用合法录音拟合实际弦参数。不得将本阶段的合成参数标注为 measured。

## 参考资料

- Balázs Bank, *Physics-Based Sound Synthesis of the Piano*, 2003: <https://home.mit.bme.hu/~bank/publist/jasp03.pdf>
- Julius O. Smith, *Digital Waveguide Models*: <https://www.dsprelated.com/freebooks/pasp/Digital_Waveguide_Models.html>
- Julius O. Smith, *Fractional Delay Filtering by Linear Interpolation*: <https://www.dsprelated.com/freebooks/pasp/Fractional_Delay_Filtering_Linear.html>
- Julius O. Smith, *Stiff String Synthesis Models*: <https://mail.dsprelated.com/freebooks/pasp/Stiff_String_Synthesis_Models.html>
- DAFx 2006 stiff-string waveguide paper: <https://www.dafx.de/paper-archive/2006/papers/p_071.pdf>
