# P01 Model Assumptions

## Scope and state

This is a synthetic, offline reference for one transverse C4 string and one hammer. It is not fitted to a particular piano. The hammer state is face displacement `x_h` (m) and velocity `v_h` (m/s). The string has modal displacement `q_n` (m) and velocity `v_n` (m/s), with sine modes `phi_n(x)=sin(n*pi*x/L)`. Coordinates are oriented so the hammer travels in the positive direction and positive string displacement relieves compression.

The string length is `L=0.65 m`; linear density is `mu=0.0062 kg/m`; tension is `T=716.982381 N`; bending stiffness is `EI=0.009207818 N m^2`. Both derived parameters are stored and checked against their tuning equations in `reference_parameters.json`. The endpoints are simply supported (transverse displacement zero). Each sine mode has mass `mu*L/2` kg and angular frequency

`omega_n^2 = (T/mu)*(n*pi/L)^2 + (EI/mu)*(n*pi/L)^4`.

The first mode is tuned to `261.625565 Hz`, MIDI 60 with A4=440 Hz. The synthetic inharmonicity parameter is `B=EI*pi^2/(T*L^2)=0.0003`, so `f_n = n*f_1*sqrt((1+B*n^2)/(1+B))`. The ideal-string comparison sets `EI=0` at the same tension.

## Contact, energy, and signs

At the strike position `x_b`, the string displacement is `y_s=sum(phi_n(x_b)*q_n)`. Compression is `delta=max(0, x_h-y_s)` and force magnitude is `F=K*delta^p`. Since `delta` is metres and force is newtons, `K` has units `N/m^p` (here `N/m^2.5`). The hammer receives `-F`; the string mode receives `+F*phi_n(x_b)`. Contact potential is `K*delta^(p+1)/(p+1)` joules. Contact begins at `delta=0` with positive hammer speed and ends at the first zero crossing with decreasing `delta`.

Energy is hammer kinetic energy plus modal kinetic and potential energy plus contact potential. Modal damping removes power `sum(2*zeta*omega_n*m_n*v_n^2)`; the report tracks its integral and the residual energy imbalance. The incident hammer kinetic energy is the system's input. The integrated contact force also supplies hammer momentum change; string modal generalized impulse is projected with `phi_n(x_b)`.

## Numerical limits and interpretation

The spatial approximation is a truncated modal Galerkin expansion with 24, 48, or 96 sine modes; it is not a finite-difference grid. `solve_ivp` uses DOP853, tight tolerances, a maximum step tied to 48 kHz oversampling, and dense output. The fourth-order bending term gives the stiff-string dispersion relation above; modal truncation and time integration still introduce finite errors, measured by the convergence experiment. Fixed-end displacement is enforced by every basis function.

The MIDI velocity-to-hammer-speed table in `reference_parameters.json` is an explicit synthetic mapping, not a measured action calibration. WAV files contain unnormalized string transverse point velocity in m/s at `0.92 L`, after anti-alias filtering. They are mechanical proxy signals, not microphone pressure or acoustic SPL. No real piano recording is available, so comparison to a real instrument is N/A.
