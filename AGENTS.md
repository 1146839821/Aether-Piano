# Aether Piano — Codex Project Instructions

## Mission

Implement an original physically modeled piano VST3/AU/Standalone instrument. Prioritize acoustical correctness, numerical stability, reproducible experiments, real-time audio safety, then UI. Do not copy proprietary commercial instrument code, presets or resources.

## Before editing

1. Read `README.md`, `docs/00_项目目标与边界.md`, `docs/01_技术栈与系统架构.md`, `docs/07_测试与量化验收.md` and the currently assigned `codex/Pxx_*.md`.
2. Inspect repository status and affected modules. Keep changes within the phase scope. Record any necessary scope expansion.
3. Never claim tests passed without actually running and recording command output. Report environment limitations exactly.

## Engineering rules

- C++20 core independent of JUCE; JUCE used only in plugin adapter/UI.
- Physical quantities must have explicit units; state assumptions and energy flows.
- No dynamic allocation, deallocation, blocking locks, logging, file/network IO, or throwing exceptions from audio callbacks; allocate in prepare/non-RT worker.
- Event sample offsets are authoritative; handle zero blocks, host variable block size and sample-rate changes.
- DSP changes require numerical/unit tests and offline WAV/feature regression where appropriate.
- Test extreme valid inputs and ensure no NaN, Inf, unbounded unintended growth, self-oscillation or denormals.
- Do not use output limiter to conceal unstable internal dynamics.
- Parameter JSON/schema changes require migration or versioned compatibility tests.
- Keep dependency versions pinned and maintain license/source metadata.
- Prefer small reviewable commits. Do not remove unrelated tests to pass a gate.

## Workflow

- Implement one phase at a time following `codex/README.md` and its Pxx instruction.
- Update research notes/ADR when changing algorithms, filter designs, sample rate strategy or plugin-facing parameters.
- Run Debug/Release build, CTest, phase-specific experiments, and (when relevant) plugin validators.
- Write phase report using `templates/PHASE_REPORT.md`, with PASS/FAIL/SKIP/NOT RUN and exact commands.
- Stop at unresolved blockers. Do not silently progress to next phase.

## Report format

1. Implemented / files changed.
2. Reproduction steps and environment.
3. Tests run + results with direct logs/paths.
4. Audio artifacts, quantitative results, graphs and profile data.
5. Known approximations, unresolved risks, next task.
