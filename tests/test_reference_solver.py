import math
import unittest

import numpy as np

from tools.analyzer.reference_solver.run_experiments import (
    FS,
    estimate_peak_hz,
    load_parameters,
    render_anti_aliased,
)
from tools.analyzer.reference_solver.solver import Hammer, String, simulate_contact


TARGET_F0 = 261.625565
LENGTH = 0.65
DENSITY = 0.0062
B = 0.0003
TENSION = DENSITY * (2.0 * LENGTH * TARGET_F0) ** 2 / (1.0 + B)
BENDING = B * TENSION * LENGTH**2 / math.pi**2


def make_string(modes: int, damping: float = 2e-5) -> String:
    return String(LENGTH, DENSITY, TENSION, BENDING, modes, damping)


def make_hammer(speed: float = 0.8, **kwargs) -> Hammer:
    defaults = dict(
        mass_kg=0.008,
        stiffness_N_m_p=8e9,
        exponent_p=2.5,
        strike_position_m=0.12 * LENGTH,
        initial_velocity_m_s=speed,
    )
    defaults.update(kwargs)
    return Hammer(**defaults)


class HammerStringReferenceTests(unittest.TestCase):
    def test_parameter_file_is_explicit_and_self_consistent(self):
        params, target_f0, length, density, tension, bending = load_parameters()
        self.assertEqual(params["source_classification"], "synthetic_research")
        self.assertEqual(target_f0, TARGET_F0)
        self.assertAlmostEqual(length, LENGTH)
        self.assertAlmostEqual(density, DENSITY)
        self.assertAlmostEqual(tension, TENSION)
        self.assertAlmostEqual(bending, BENDING)

    def test_ideal_and_stiff_string_frequency_self_consistency(self):
        stiff = make_string(32)
        f1 = stiff.angular_frequencies()[0] / (2.0 * math.pi)
        f_ideal_same_tension = math.sqrt(stiff.tension_N / DENSITY) / (2.0 * LENGTH)
        self.assertAlmostEqual(f1, TARGET_F0, places=9)
        self.assertAlmostEqual(f_ideal_same_tension, TARGET_F0 / math.sqrt(1 + B), places=9)

    def test_no_input_stays_finite_and_has_no_energy_or_force(self):
        result = simulate_contact(
            make_string(16), make_hammer(0.0), max_step_s=1.0 / (FS * 8)
        )
        self.assertFalse(result.had_contact)
        np.testing.assert_array_equal(result.state_at(np.array([0.0, 0.01])), 0.0)
        np.testing.assert_array_equal(result.force_at(np.array([0.0, 0.01])), 0.0)
        self.assertEqual(result.initial_energy_J, 0.0)

    def test_contact_power_energy_and_momentum_flow_are_finite(self):
        result = simulate_contact(
            make_string(24), make_hammer(), max_step_s=1.0 / (FS * 8)
        )
        self.assertTrue(result.had_contact)
        self.assertGreater(result.peak_force_N, 0.0)
        self.assertGreater(result.impulse_N_s, 0.0)
        self.assertGreaterEqual(result.damping_loss_J, 0.0)
        self.assertLess(result.release_state[1], result.hammer.initial_velocity_m_s)
        self.assertGreater(np.linalg.norm(result.release_state[2 + result.string.modes :]), 0.0)
        self.assertLess(result.max_energy_balance_error_J / result.initial_energy_J, 2e-5)
        self.assertLess(abs(result.impulse_momentum_error_N_s), 2e-6)
        self.assertLess(float(result.force_at(result.release_time_s)), 1e-6)
        self.assertTrue(np.all(np.isfinite(result.state_at(np.linspace(0, 0.05, 32)))))

    def test_extreme_valid_hammer_inputs_remain_finite(self):
        result = simulate_contact(
            make_string(16),
            make_hammer(
                5.0,
                mass_kg=5e-4,
                stiffness_N_m_p=1e11,
                exponent_p=3.5,
            ),
            max_step_s=1.0 / (FS * 16),
        )
        self.assertTrue(result.had_contact)
        self.assertTrue(np.isfinite(result.peak_force_N))
        self.assertTrue(np.isfinite(result.release_time_s))
        self.assertTrue(np.all(np.isfinite(result.release_state)))
        self.assertLess(result.release_time_s, 0.1)

    def test_spatial_and_time_refinement_converge(self):
        rows = []
        for modes, oversample in ((24, 4), (48, 8), (96, 16)):
            result = simulate_contact(
                make_string(modes),
                make_hammer(),
                max_step_s=1.0 / (FS * oversample),
            )
            signal = render_anti_aliased(result, oversample, 0.5, 0.92)
            f1 = estimate_peak_hz(signal, FS, TARGET_F0 - 12, TARGET_F0 + 12)
            rows.append((result, signal, f1))
        peak_errors = [abs(row[0].peak_force_N - rows[-1][0].peak_force_N) for row in rows]
        contact_errors = [abs(row[0].release_time_s - rows[-1][0].release_time_s) for row in rows]
        self.assertLess(peak_errors[1], peak_errors[0])
        self.assertLess(contact_errors[1], contact_errors[0])
        self.assertLess(abs(rows[1][2] - rows[-1][2]), abs(rows[0][2] - rows[-1][2]))
        self.assertLess(abs(rows[-1][2] - TARGET_F0), 0.01)
        for _result, signal, _f1 in rows:
            self.assertTrue(np.all(np.isfinite(signal)))
        shared = min(rows[0][1].size, rows[-1][1].size)
        coarse_error = np.linalg.norm(rows[0][1][:shared] - rows[-1][1][:shared])
        medium_error = np.linalg.norm(rows[1][1][:shared] - rows[-1][1][:shared])
        self.assertLess(medium_error, coarse_error)


if __name__ == "__main__":
    unittest.main()
