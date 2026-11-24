import importlib.util
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("vita_budget", ROOT / "tools" / "vita-budget.py")
vita_budget = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vita_budget)

BUDGETS = json.loads((ROOT / "budgets.json").read_text(encoding="utf-8"))
META = {"scenario": "tour", "vita_commit": "a", "engine_commit": "b", "package": "c", "build_vars": ""}


class BudgetTest(unittest.TestCase):
    BASE = {"p99": 10.0}

    def test_within_regression_limit_passes(self):
        self.assertEqual(vita_budget.check({"frames": 5000, "p99": 10.9}, META, BUDGETS, self.BASE)[0], 0)

    def test_regression_over_limit_fails(self):
        self.assertEqual(vita_budget.check({"frames": 5000, "p99": 11.5}, META, BUDGETS, self.BASE)[0], 1)

    def test_missing_baseline_is_untrusted(self):
        self.assertEqual(vita_budget.check({"frames": 5000, "p99": 1.0}, META, BUDGETS)[0], 2)

    def test_goal_not_met_is_reported_but_not_a_failure(self):
        code, lines = vita_budget.check({"frames": 5000, "p99": 50.0}, META, BUDGETS, {"p99": 50.0})
        self.assertEqual(code, 0)
        self.assertIn("not met", lines[0])

    def test_missing_metadata_is_untrusted(self):
        self.assertEqual(vita_budget.check({"frames": 5000, "p99": 1.0}, {"scenario": "tour"}, BUDGETS, self.BASE)[0], 2)

    def test_unknown_scenario_and_short_trace_are_untrusted(self):
        self.assertEqual(vita_budget.check({"frames": 5000, "p99": 1.0}, dict(META, scenario="x"), BUDGETS)[0], 2)
        self.assertEqual(vita_budget.check({"frames": 5, "p99": 1.0}, META, BUDGETS)[0], 2)


if __name__ == "__main__":
    unittest.main()
