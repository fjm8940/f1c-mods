"""Preservation and transition-policy regressions. Via the canonical SDK gate."""
from pathlib import Path
import copy
import hashlib
import json
import sys
import unittest
from unittest.mock import patch, mock_open
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from qualifying_order import CapturedMemory, completion_policy, compare, write_new, run_command
from process_memory_snapshot import SnapshotError


def state(**overrides):
    return dict(session_type=5, session_start=30.0, session_end=3630.0,
                session_clock=4069.5, **overrides)


def vehicle(name="Driver", **overrides):
    row = dict(driver=name, speed=0.0, active_flag=0, finish_state=0,
               best_lap=86.0, position=1, stored_qualifying_time=86.0,
               stored_grid_position=1)
    row.update(overrides)
    return row


class QualifyingOrderTests(unittest.TestCase):
    def test_live_capture_namespace_does_not_require_inspect_argument(self):
        args=SimpleNamespace(action="capture",tag="new",process="game.exe",output="new.json")
        with patch.object(Path,"exists",return_value=False), \
             patch("qualifying_order.capture_snapshot",return_value=("manifest.json",{})) as capture, \
             patch("qualifying_order.decode",return_value={"changed_positions":0}), \
             patch("qualifying_order.write_new") as save:
            run_command(ROOT,args)
            capture.assert_called_once_with(ROOT,"new",process_name="game.exe",scope="module")
            save.assert_called_once()

    def test_clock_expired_inactive_11_lap_driver_is_preservation_candidate(self):
        result = completion_policy(state(), [vehicle(lap=11)])
        self.assertEqual(result["proposed_action"], "PRESERVE_EXISTING_RESULTS")
        self.assertFalse(result["suppress_simulation"])  # archive tool never writes

    def test_empty_track_before_time_limit_does_not_seal(self):
        context = state(); context["session_clock"] = 600
        self.assertEqual(completion_policy(context, [vehicle()])["proposed_action"], "KEEP_NATIVE_SIMULATION")

    def test_expired_clock_with_active_stationary_car_does_not_seal(self):
        self.assertEqual(completion_policy(state(), [vehicle(active_flag=1)])["proposed_action"], "KEEP_NATIVE_SIMULATION")

    def test_expired_clock_with_moving_car_does_not_seal(self):
        self.assertEqual(completion_policy(state(), [vehicle(speed=10)])["proposed_action"], "KEEP_NATIVE_SIMULATION")

    def test_race_never_qualifies(self):
        context = state(); context["session_type"] = 7
        self.assertEqual(completion_policy(context, [vehicle()])["state"], "NOT_QUALIFYING")

    def test_zero_duration_does_not_seal(self):
        context = state(); context["session_start"] = context["session_end"] = 0
        self.assertEqual(completion_policy(context, [vehicle()])["proposed_action"], "KEEP_NATIVE_SIMULATION")

    def test_untimed_driver_stays_untimed_and_order_not_resorted_by_name(self):
        rows = [vehicle("Z", position=1), vehicle("A", position=2, best_lap=-1)]
        before = dict(schema="f1c.qualifying_order.v1", globals=state(), executable={"sha256": "A"},
                      captured="before", pid=1, rows=rows)
        after = copy.deepcopy(before); after["rows"].reverse()
        result = compare(before, after)
        self.assertEqual(result["changed_positions"], 0)
        self.assertEqual(result["rows"][1]["before_best_lap"], -1)

    def test_changed_lineup_is_rejected(self):
        before = dict(schema="f1c.qualifying_order.v1", globals=state(), executable={"sha256":"A"},
                      captured="before", pid=1, rows=[vehicle()])
        after = copy.deepcopy(before); after["rows"][0]["driver"] = "Someone else"
        with self.assertRaises(SnapshotError): compare(before, after)

    def test_zero_page_cross_boundary_and_missing_page(self):
        digest = hashlib.sha256(bytes(4096)).hexdigest()
        manifest = dict(page_size=4096, pages=[dict(address=hex(base), size=4096, sha256=digest, zero=True)
                                            for base in (4096,8192)])
        memory = CapturedMemory(manifest)
        self.assertEqual(memory.read(8190, 6), bytes(6))
        self.assertEqual(memory.value(8190, "d"), 0.0)
        with self.assertRaises(SnapshotError): memory.read(12288, 1)

    def test_page_integrity_is_enforced(self):
        memory = CapturedMemory(dict(page_size=4096, pages=[dict(address="0x1000", size=4096,
                                                              sha256="0"*64, zero=True)]))
        with self.assertRaises(SnapshotError): memory.read(4096, 1)

    def test_export_uses_exclusive_creation(self):
        with patch.object(Path, "mkdir"), patch.object(Path, "open", mock_open()) as opened:
            write_new("unused.json", {"read_only": True})
            opened.assert_called_once_with("x", encoding="utf-8")

    def test_real_regression_when_saved_capture_available(self):
        from qualifying_order import decode
        source = ROOT / "runtime_memory/ra00-qualifying-20260907-elevated/manifest.json"
        if not source.exists(): self.skipTest("owner capture unavailable")
        before = decode(ROOT, source)
        after = decode(ROOT, "ra00-race-transition-20260907")
        result = compare(before, after)
        self.assertEqual(result["changed_positions"], 18)
        self.assertAlmostEqual(before["globals"]["session_clock"],4069.5001184,places=5)
        self.assertEqual(before["completion"]["proposed_action"],"PRESERVE_EXISTING_RESULTS")
        self.assertEqual(sum(r["before_best_lap"] != r["after_stored_qualifying_time"] for r in result["rows"]),6)


if __name__ == "__main__": unittest.main()
