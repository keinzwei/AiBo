# Synthetic regression fixtures

`aim_baseline.csv` contains 1200 pitch/yaw/fire expectations for the deterministic
input sequence in `tests/aim_baseline_trace.cpp`. It is generated test data,
not a human recording or an archived implementation. The comparator checks
angles within 0.001 degree for compiler floating-point variation, and firing
decisions exactly. No previous release source or executable is required.

License: GPL-2.0-or-later, like the AiBo Human Copycat code that generated it.
