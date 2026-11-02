# Hammer/String Reference Interface

The future C++ reference adapter consumes SI-valued input arrays and returns SI-valued output arrays. It is an offline validation interface; it is not called from the real-time audio callback.

## Inputs

- `sample_rate_hz`: finite positive `double` (output sampling rate).
- `duration_s`: finite nonnegative `double`.
- `string_length_m`, `linear_density_kg_m`, `tension_N`, `bending_stiffness_N_m2`, `modal_damping_ratio`: finite string parameters.
- `mode_count`: integer in `[2,128]`; mode `n` uses `sin(n*pi*x/L)`.
- `hammer_mass_kg`, `contact_stiffness_N_m_p`, `contact_exponent`, `initial_velocity_m_s`: finite hammer parameters. The stiffness unit is `N/m^p`.
- `strike_position_m`: point on the open interval `(0,L)`.
- `readout_position_m`: point on `[0,L]` where string velocity is sampled.
- `max_step_s`, `relative_tolerance`, `absolute_tolerance`: integrator controls.

## Outputs

- `time_s[N]`: monotonically increasing sample times, seconds.
- `string_velocity_m_s[N]`: reconstructed transverse velocity at the readout position, metres per second.
- `contact_force_N[K]` and `contact_time_s[K]`: nonnegative force trace during initial contact.
- `modal_displacement_m[mode_count][N]` and `modal_velocity_m_s[mode_count][N]`: optional modal states for diagnostics.
- Scalar diagnostics: `contact_duration_s`, `peak_force_N`, `hammer_impulse_N_s`, `initial_energy_J`, `damping_loss_J`, and `max_energy_balance_error_J`.

The producer must also return status for invalid parameters, no contact, failed integration, and non-finite state. Arrays use row-major mode-major order where applicable. No output normalization is allowed; all values preserve the model's physical scale.
