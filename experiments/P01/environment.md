# P01 Environment

- Date: 2026-11-02 (Asia/Shanghai)
- OS / CPU: macOS 27.0.1, Darwin 27.0.0, Apple M4 / arm64
- Toolchain: Apple clang 21.0.0, CMake 4.4.0, Ninja, Xcode 27.2
- Python: 3.14.6 in repository-local `.venv`
- Pinned Python packages: NumPy 2.5.4, SciPy 1.18.1, Matplotlib 3.11.2 (`requirements-p01.txt`)
- JUCE: pinned 9.0.5 submodule; no JUCE or plugin DSP changes in P01

Recreate the local Python environment with:

```sh
uv venv .venv --python python3.14
uv pip install --python .venv/bin/python -r requirements-p01.txt
```

P00 records the full native toolchain and its SDK lookup caveat in `experiments/P00/environment.md`.
