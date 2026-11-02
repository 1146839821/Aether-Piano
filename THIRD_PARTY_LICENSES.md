# Third-party licenses

| Component | Version / revision | License status | Use |
|---|---|---|---|
| JUCE | 9.0.5, `d60b7a78030661574a615b586d9325ba7a7fa7dd` | JUCE dual licensing; product distribution model and applicable license must be reviewed before release. See `external/JUCE/LICENSE.md`. | Plugin wrapper and audio processor base classes |
| NumPy | 2.5.4 | BSD-3-Clause for the project; binary wheels can include separately licensed BLAS/compiler runtime components. See [upstream license notes](https://github.com/numpy/numpy/blob/main/doc/source/license.rst) and retain wheel notices if redistributing the analysis environment. | P01 offline numerical model |
| SciPy | 1.18.1 | BSD-3-Clause. See [upstream license](https://github.com/scipy/scipy/blob/main/LICENSE.txt). | P01 ODE integration, filtering and spectral analysis |
| Matplotlib | 3.11.2 | Matplotlib License Agreement, based on the PSF license and BSD-compatible. See [upstream license](https://github.com/matplotlib/matplotlib/blob/main/LICENSE/LICENSE). | P01 report plots |

The C++ DSP/plugin has no Python runtime dependency; the P01 analyzer uses the pinned development-only Python dependencies in `requirements-p01.txt`. Review this ledger and retain required notices before redistributing the analysis environment or application. No piano recordings or commercial instrument assets are included.
