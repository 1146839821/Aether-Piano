# 03｜DSP 算法路线、伪代码、稳定性和性能

## 方案比较（在 P01/P02 做实验，而非凭偏好确定）

| 琴弦方案 | 强项 | 风险 | 决策 |
|---|---|---|---|
| 双向数字波导（主线） | 传播、击弦位置、耦合物理直观，常见音乐 DSP 方案 | 刚性弦频散滤波、稳定的接触/端点网络复杂 | 首选 |
| 模态共振器组（并行基准） | 泛音频率/衰减易精准拟合、SIMD 友好 | 强非线性与激励位置需额外处理、模态数高 | 对照/备选 |
| 高精度 FDTD/隐式离散模型 | 对照物理方程和能量 | 实时成本及色散误差 | **离线真值近似**，不作为首版插件核心 |
| 预存波形/多采样层 | 容易试听和回归对比 | 不是目标物理建模 | 仅参考素材，禁止冒充引擎 |

## 处理每个样本的顺序（概念算法）

```text
for each audio sample n:
    apply events scheduled at n
    update action state / pedal / damper targets
    for each actively excited note:
        read traveling-wave states at hammer location
        solve hammer-string contact (implicit/energy-aware)
        inject equal/opposite interaction to wave states
        propagate waves + fractional delay + dispersion + loss
        collect forces/velocities at bridge ports
    solve passive bridge junction across active strings
    advance selected sympathetic strings / soundboard modal states
    propagate bridge reaction to string termination states
    project soundboard radiation to stereo acoustic outputs
    apply output safety gain (not a substitute for stability)
```

注意：上面是依赖顺序，实际实现必须**避免引入代数环**。例如分裂步、显隐混合、预计算散射系数、因果延迟和能量守恒互连都需要用参考求解器测试。

## A. 分数延迟与调律

- 使用明确的双向传播长度，不能直接把 `fs/f0` 全部塞进同一侧 delay line。
- 选择 Lagrange/Farrow 或 Thiran all-pass 等方法时测量群延迟、幅度响应与稳定性；对振动弦可能需要区分近 DC 与高频误差。
- 增益小于 1 的 loss loop 不等于“所有频点都无源”；需逐频验证。
- 将相位贡献 `D + phaseDispersion + phaseLoss + phaseBridge` 统一用于调律求根，而不是只调 `D`。

## B. 非谐性/色散

理论标定目标：`f_n ≈ n f_ideal sqrt(1+B n²)`。实际先从录音测得 `f_n`，对选定谐波拟合 `B`、有效基频；使用全通滤波器的**相位/群延迟**设计匹配分布，并对目标频率点计算误差。高频不可解析部分需防混叠。

关键单元测试：`|H_disp(e^jw)|≈1`、滤波器极点在单位圆内、无异常群延迟、目标泛音频率误差可控。

## C. 频率相关损耗

为各阶泛音拟合 `T60_n`，由单周期反馈幅度系数 `g_n` 得到粗略关系：

\[
g_n \approx 10^{-3/(f_n T60_n)}\]

这个式子只在每周期一次且对应第 `n` 阶泛音的等效衰减近似下使用；实际要考虑波导环路的往返时间、频率相关群延迟和模态衰减定义，不能机械地对每个 bin 直接套公式。

合成目标损耗频响后，以低阶稳定滤波器拟合并设定 `|H_loss|≤1` 的无源性约束。不要用随意的 EQ 替代弦损耗。

## D. 琴槌接触数值法

- 离线：使用小步长/自适应 ODE 或隐式耦合参考解；记录峰值力、接触时长、能量平衡。
- 实时：优先能量稳定的接触离散化，或经充分验证的隐式局部根求解；难收敛时**显式报错/回退**，不能输出 NaN。
- 如果过采样只发生在接触子系统，必须证明其与全速率波导互连在能量与相位上合理；滤波防混叠。
- 力正向、位置/速度单位、一致的采样点和符号约定用单元测试覆盖。

## E. 弦桥散射与音板模态

- 建立机械端口（力 `F`，速度 `v`）定义和能量流 `P=F·v`，网络的无源性可验证。
- 若简单耦合矩阵引起能量放大，不能用最后一级 limiter 掩盖问题。
- 模态二阶系统使用稳定离散化（例如基于连续极点的离散模型）；`ω_m` 接近 Nyquist 的模式选择必须有约束。
- 辐射与桥口负载相互独立标定；空间输出保持同一参考音量标准。

## F. 活跃音符/共鸣音符调度

- 每键持有独立 `noteState`，多键和不同 MIDI 通道的重叠 Note On 需明确定义策略。
- 用能量阈值 + 最小驻留时间释放静音 voice；阈值变化加入滞后，避免声学上可听到的突变。
- 共鸣弦可分层（低阶模态与高阶估计、按敏感度筛选），但必须记录近似误差；静音弦被踏板激活时不可凭空重置。
- 不在实时路径扩容 `std::vector`、读取模型 JSON、动态构建新音板或生成 FIR 卷积核。

## G. 采样率与块大小

- 处理独立于宿主块边界，事件 sample-offset 一致。
- sample rate 改变时，由 `prepare()` 在非 RT 阶段重新计算延时、滤波系数与模态参数，必要时重置状态并清晰文档化。
- `silence -> note -> sustain -> release` 横跨任意块，不允许后处理时重建整音。

## H. 性能预算设计

在 48 kHz、64 samples 下，块时间 `64/48000 ≈ 1.333 ms`。初步工程预算可先设 DSP 回调最多使用其 70%（约 0.933 ms），**但必须绑定硬件、复音数、踏板场景和测试方法**。采集 `p50/p95/p99/max`、音频中断及 CPU 利用率；测得 `max` 更优先于平均值。

有意保留谱缓存、SIMD、事件批处理、常量参数预计算和多速率结构的优化空间，但不以未经证明的近似破坏能量守恒与音高。

## I. 必须做的数值测试

1. 无外力/零阻尼参考中，离散能量不自发增长（明确边界/离散误差）。
2. 有损模型在外力结束后逐步衰减；任何速度/力/位移非有限值触发测试失败。
3. 边界参数（最低音、高音、极弱/极强、最小/最大允许值）以及快速重触发。
4. 谐波目标、FFT 频散误差、接触力-时间曲线、力学能量净变化。
5. 插件 callback 中被明确禁止的分配/锁等待/系统调用检测。

参考：[Smith — Stiff String](https://www.dsprelated.com/freebooks/pasp/Stiff_String.html)、[Bank — 2000 论文摘要](https://home.mit.bme.hu/~bank/thesis/thesabs.html)。
