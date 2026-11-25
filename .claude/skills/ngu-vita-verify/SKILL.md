---
name: ngu-vita-verify
description: Check the Vita build and gather evidence from actual hardware.
---

Follow the repository README and run `just doctor` and `just check`. Use
`tools/vita-cycle.sh` for the device workflow and an isolated save profile
for tests. Use `tools/vita-trace.py` and `tools/vita-budget.py` to collect
and evaluate device timing against the repository budgets. Report build
checks separately from real console observations.
