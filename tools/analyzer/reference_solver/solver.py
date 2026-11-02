"""High-accuracy offline hammer/string model using a stiff-string modal basis.

Coordinates are metres. Hammer motion is positive toward the string; the
string's transverse displacement is positive in the same direction. Therefore
compression is max(0, x_h - y_s), the hammer receives -F and the string +F.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy.integrate import quad, solve_ivp
from scipy.optimize import minimize_scalar


@dataclass(frozen=True)
class String:
    length_m: float
    linear_density_kg_m: float
    tension_N: float
    bending_stiffness_N_m2: float
    modes: int
    damping_ratio: float = 2.0e-5

    def __post_init__(self) -> None:
        if not (0.1 <= self.length_m <= 3.0):
            raise ValueError("length_m must be in [0.1, 3.0]")
        if not (1.0e-4 <= self.linear_density_kg_m <= 0.1):
            raise ValueError("linear_density_kg_m must be in [1e-4, 0.1]")
        if not (1.0 <= self.tension_N <= 5000.0):
            raise ValueError("tension_N must be in [1, 5000]")
        if not (0.0 <= self.bending_stiffness_N_m2 <= 1.0):
            raise ValueError("bending_stiffness_N_m2 must be in [0, 1]")
        if not (2 <= self.modes <= 128):
            raise ValueError("modes must be in [2, 128]")
        if not (0.0 <= self.damping_ratio <= 0.05):
            raise ValueError("damping_ratio must be in [0, 0.05]")

    @property
    def modal_mass_kg(self) -> float:
        return self.linear_density_kg_m * self.length_m / 2.0

    @property
    def inharmonicity_B(self) -> float:
        return (
            self.bending_stiffness_N_m2
            * np.pi**2
            / (self.tension_N * self.length_m**2)
        )

    def angular_frequencies(self) -> np.ndarray:
        n = np.arange(1, self.modes + 1, dtype=np.float64)
        k = n * np.pi / self.length_m
        return np.sqrt(
            self.tension_N / self.linear_density_kg_m * k**2
            + self.bending_stiffness_N_m2 / self.linear_density_kg_m * k**4
        )


@dataclass(frozen=True)
class Hammer:
    mass_kg: float
    stiffness_N_m_p: float
    exponent_p: float
    strike_position_m: float
    initial_velocity_m_s: float

    def validate_for(self, string: String) -> None:
        if not (5.0e-4 <= self.mass_kg <= 0.1):
            raise ValueError("mass_kg must be in [5e-4, 0.1]")
        if not (1.0e5 <= self.stiffness_N_m_p <= 1.0e12):
            raise ValueError("stiffness_N_m_p must be in [1e5, 1e12]")
        if not (1.0 <= self.exponent_p <= 4.0):
            raise ValueError("exponent_p must be in [1, 4]")
        if not (0.0 < self.strike_position_m < string.length_m):
            raise ValueError("strike_position_m must be inside the string")
        if not (0.0 <= self.initial_velocity_m_s <= 5.0):
            raise ValueError("initial_velocity_m_s must be in [0, 5]")


@dataclass
class ContactResult:
    string: String
    hammer: Hammer
    solution: object | None
    release_time_s: float
    release_state: np.ndarray
    peak_force_N: float
    impulse_N_s: float
    hammer_momentum_change_N_s: float
    impulse_momentum_error_N_s: float
    initial_energy_J: float
    damping_loss_J: float
    max_energy_balance_error_J: float

    @property
    def had_contact(self) -> bool:
        return self.solution is not None

    def state_at(self, time_s: np.ndarray | float) -> np.ndarray:
        times = np.asarray(time_s, dtype=np.float64)
        shape = (2 + 2 * self.string.modes,) + times.shape
        if not self.had_contact:
            return np.zeros(shape, dtype=np.float64)
        assert self.solution is not None
        out = np.empty(shape, dtype=np.float64)
        during = times <= self.release_time_s
        if np.any(during):
            out[..., during] = self.solution.sol(times[during])
        if np.any(~during):
            elapsed = times[~during] - self.release_time_s
            out[..., ~during] = free_string_state(
                self.string, self.release_state, elapsed
            )
            out[0, ..., ~during] = self.release_state[0] + self.release_state[1] * elapsed
            out[1, ..., ~during] = self.release_state[1]
        if times.ndim == 0:
            return out.reshape(2 + 2 * self.string.modes)
        return out

    def force_at(self, time_s: np.ndarray | float) -> np.ndarray:
        state = self.state_at(time_s)
        xh = state[0]
        q = state[2 : 2 + self.string.modes]
        phi = np.sin(
            np.arange(1, self.string.modes + 1, dtype=np.float64)
            * np.pi
            * self.hammer.strike_position_m
            / self.string.length_m
        )
        ys = np.tensordot(phi, q, axes=(0, 0))
        compression = np.maximum(0.0, xh - ys)
        force = self.hammer.stiffness_N_m_p * compression**self.hammer.exponent_p
        return np.where(np.asarray(time_s) < self.release_time_s, force, 0.0)


def free_string_state(string: String, state: np.ndarray, elapsed_s: np.ndarray) -> np.ndarray:
    """Analytic damped modal continuation after hammer separation."""
    elapsed = np.asarray(elapsed_s, dtype=np.float64)
    m = string.modes
    q0 = state[2 : 2 + m, None]
    v0 = state[2 + m :, None]
    omega = string.angular_frequencies()[:, None]
    zeta = string.damping_ratio
    wd = omega * np.sqrt(1.0 - zeta**2)
    t = elapsed.reshape(1, -1)
    decay = np.exp(-zeta * omega * t)
    c = np.cos(wd * t)
    s = np.sin(wd * t)
    a = (v0 + zeta * omega * q0) / wd
    q = decay * (q0 * c + a * s)
    v = decay * (
        -zeta * omega * (q0 * c + a * s)
        - q0 * wd * s
        + a * wd * c
    )
    result = np.zeros((2 + 2 * m, elapsed.size), dtype=np.float64)
    result[2 : 2 + m] = q
    result[2 + m :] = v
    if np.ndim(elapsed_s) == 0:
        return result[:, 0]
    return result


def _energy(string: String, hammer: Hammer, state: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    m = string.modes
    xh = state[0]
    vh = state[1]
    q = state[2 : 2 + m]
    v = state[2 + m :]
    omega = string.angular_frequencies()
    phi = np.sin(
        np.arange(1, m + 1, dtype=np.float64)
        * np.pi
        * hammer.strike_position_m
        / string.length_m
    )
    compression = np.maximum(0.0, xh - np.dot(phi, q))
    energy = (
        0.5 * hammer.mass_kg * vh**2
        + 0.5
        * string.modal_mass_kg
        * np.sum(v**2 + (omega[:, None] * q) ** 2, axis=0)
        + hammer.stiffness_N_m_p
        / (hammer.exponent_p + 1.0)
        * compression ** (hammer.exponent_p + 1.0)
    )
    dissipation_W = (
        2.0
        * string.damping_ratio
        * string.modal_mass_kg
        * np.sum(omega[:, None] * v**2, axis=0)
    )
    return energy, dissipation_W


def simulate_contact(
    string: String,
    hammer: Hammer,
    *,
    max_step_s: float,
    rtol: float = 2.0e-10,
    atol: float = 2.0e-13,
    max_contact_s: float = 0.1,
) -> ContactResult:
    """Integrate initial impact through first separation with DOP853.

    The string uses fixed-displacement (simply supported modal) endpoints and
    M sine modes. A point force is projected at the hammer's strike position.
    """
    hammer.validate_for(string)
    if not (0.0 < max_step_s <= 1.0e-3):
        raise ValueError("max_step_s must be in (0, 1e-3]")
    if not (1.0e-13 <= rtol <= 1.0e-3 and 1.0e-15 <= atol <= 1.0e-3):
        raise ValueError("rtol/atol outside supported finite precision range")
    if not (0.0 < max_contact_s <= 0.25):
        raise ValueError("max_contact_s must be in (0, 0.25]")
    m = string.modes
    frequencies = string.angular_frequencies()
    if not np.all(np.isfinite(frequencies)):
        raise ValueError("non-finite modal frequency")
    initial = np.zeros(2 + 2 * m, dtype=np.float64)
    initial[1] = hammer.initial_velocity_m_s
    initial_energy = 0.5 * hammer.mass_kg * hammer.initial_velocity_m_s**2
    if hammer.initial_velocity_m_s == 0.0:
        return ContactResult(
            string, hammer, None, 0.0, initial, 0.0, 0.0,
            0.0, 0.0, initial_energy, 0.0, 0.0
        )
    phi = np.sin(
        np.arange(1, m + 1, dtype=np.float64)
        * np.pi
        * hammer.strike_position_m
        / string.length_m
    )

    def rhs(_t: float, y: np.ndarray) -> np.ndarray:
        xh, vh = y[:2]
        q = y[2 : 2 + m]
        v = y[2 + m :]
        compression = max(0.0, xh - float(np.dot(phi, q)))
        force = hammer.stiffness_N_m_p * compression**hammer.exponent_p
        acceleration = (
            -(frequencies**2) * q
            - 2.0 * string.damping_ratio * frequencies * v
            + force * phi / string.modal_mass_kg
        )
        result = np.empty_like(y)
        result[0] = vh
        result[1] = -force / hammer.mass_kg
        result[2 : 2 + m] = v
        result[2 + m :] = acceleration
        return result

    def separation(_t: float, y: np.ndarray) -> float:
        return float(y[0] - np.dot(phi, y[2 : 2 + m]))

    separation.terminal = True  # type: ignore[attr-defined]
    separation.direction = -1  # type: ignore[attr-defined]
    solution = solve_ivp(
        rhs,
        (0.0, max_contact_s),
        initial,
        method="DOP853",
        rtol=rtol,
        atol=atol,
        max_step=max_step_s,
        dense_output=True,
        events=separation,
    )
    if not solution.success:
        raise RuntimeError(f"DOP853 failed: {solution.message}")
    if len(solution.t_events[0]) == 0:
        raise RuntimeError("hammer did not separate before max_contact_s")
    release_time = float(solution.t_events[0][0])
    release_state = solution.y_events[0][0]
    if not np.all(np.isfinite(solution.y)) or not np.all(np.isfinite(release_state)):
        raise FloatingPointError("non-finite state during hammer/string contact")
    energy, dissipated_power = _energy(string, hammer, solution.y)
    balance = energy - initial_energy + np.concatenate(
        ([0.0], np.cumsum(0.5 * (dissipated_power[1:] + dissipated_power[:-1]) * np.diff(solution.t)))
    )

    def force_at_time(time_s: float) -> float:
        state = solution.sol(time_s)
        compression = max(0.0, float(state[0] - np.dot(phi, state[2 : 2 + m])))
        return hammer.stiffness_N_m_p * compression**hammer.exponent_p

    def damping_power_at_time(time_s: float) -> float:
        state = solution.sol(time_s)
        modal_velocity = state[2 + m :]
        return float(
            2.0
            * string.damping_ratio
            * string.modal_mass_kg
            * np.sum(frequencies * modal_velocity**2)
        )

    dissipated = float(
        quad(damping_power_at_time, 0.0, release_time, epsabs=1e-15, epsrel=1e-12)[0]
    )
    impulses = float(
        quad(force_at_time, 0.0, release_time, epsabs=1e-14, epsrel=1e-12, limit=300)[0]
    )
    momentum_change = hammer.mass_kg * (
        hammer.initial_velocity_m_s - float(release_state[1])
    )
    node_force = hammer.stiffness_N_m_p * np.maximum(
        0.0,
        solution.y[0] - np.dot(phi, solution.y[2 : 2 + m]),
    ) ** hammer.exponent_p
    peak_index = int(np.argmax(node_force))
    left = float(solution.t[max(0, peak_index - 1)])
    right = float(solution.t[min(len(solution.t) - 1, peak_index + 1)])
    if right > left:
        peak = max(
            force_at_time(left),
            force_at_time(right),
            -float(minimize_scalar(
                lambda time_s: -force_at_time(float(time_s)),
                bounds=(left, right),
                method="bounded",
                options={"xatol": max(1e-15, max_step_s * 1e-6)},
            ).fun),
        )
    else:
        peak = float(node_force[peak_index])
    return ContactResult(
        string=string,
        hammer=hammer,
        solution=solution,
        release_time_s=release_time,
        release_state=release_state,
        peak_force_N=float(peak),
        impulse_N_s=float(impulses),
        hammer_momentum_change_N_s=float(momentum_change),
        impulse_momentum_error_N_s=float(impulses - momentum_change),
        initial_energy_J=float(initial_energy),
        damping_loss_J=dissipated,
        max_energy_balance_error_J=float(np.max(np.abs(balance))),
    )


def modal_velocity_at(string: String, state: np.ndarray, position_m: float) -> np.ndarray:
    """Reconstruct transverse string velocity at position_m, in m/s."""
    if not (0.0 <= position_m <= string.length_m):
        raise ValueError("position_m must lie on the string")
    n = np.arange(1, string.modes + 1, dtype=np.float64)
    phi = np.sin(n * np.pi * position_m / string.length_m)
    return np.tensordot(phi, state[2 + string.modes :], axes=(0, 0))
