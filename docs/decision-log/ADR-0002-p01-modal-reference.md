# ADR-0002: P01 Modal Stiff-String Reference

- Status: Accepted for offline P01 validation
- Date: 2026-11-02

## Decision

Use a truncated sine-mode Galerkin representation of a simply supported stiff string, coupled to a nonlinear hammer by `F=K*max(0,x_h-y_s)^p`. Integrate the finite-dimensional ODE with SciPy DOP853 and terminate at the first separating zero crossing. Generate anti-aliased string point-velocity proxy WAVs without per-render gain normalization.

## Rationale

The basis enforces fixed transverse displacement at both endpoints and produces an explicit dispersion law. A modal point-force projection preserves the equal-and-opposite contact power flow. It is suitable as an offline numerical reference while the real-time C++ model remains independent of Python and JUCE.

## Limits

Modal truncation, synthetic hammer/action parameters, constant modal damping, and the point-velocity output are approximations. The reference is not a full piano, acoustic radiation model, or measured instrument calibration. Convergence and energy residuals are recorded in `experiments/P01/`.
