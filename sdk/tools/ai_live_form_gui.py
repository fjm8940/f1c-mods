#!/usr/bin/env python3
"""AI Live Form GUI - point-and-click mid-race AI pace control for F1C 99-02.

Wraps tools/ai_live_form.py (see docs/43 sec3.8). Needs the game running,
in a session. No dependencies beyond the standard library.

Run:  python tools/ai_live_form_gui.py     (or double-click ai_live_form_gui.bat)
"""
import os
import sys
import threading
import time

# pythonw (double-click launch) has stdout/stderr = None; poke.py reconfigures
# stdout at import and would crash. Give it a sink first.
if sys.stdout is None:
    sys.stdout = open(os.devnull, "w")
if sys.stderr is None:
    sys.stderr = open(os.devnull, "w")

import tkinter as tk
from tkinter import messagebox, ttk

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ai_live_form as alf  # noqa: E402
from poke import Game, find_pid  # noqa: E402

POLL_MS = 2000
PRESETS = [
    ("+2.0% heroics", 1.02), ("+1.0% hero day", 1.01), ("+0.5% strong", 1.005),
    ("-0.5% off", 0.995), ("-1.0% bad day", 0.99), ("-2.0% struggling", 0.98),
    ("-3.0% tire trouble", 0.97),
]


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("F1C AI Live Form")
        self.geometry("860x520")
        self.g = None
        self.fade_thread = None
        self.fade_stop = threading.Event()
        self._build()
        self._tick()

    # ---------------- UI ----------------
    def _build(self):
        top = ttk.Frame(self)
        top.pack(fill="x", padx=8, pady=6)
        self.status = ttk.Label(top, text="connecting...", font=("Segoe UI", 10, "bold"))
        self.status.pack(side="left")
        ttk.Button(top, text="Reset ALL", command=self.reset_all).pack(side="right")
        ttk.Button(top, text="Stop fades", command=self.stop_fades).pack(side="right", padx=4)
        ttk.Button(top, text="Refresh", command=self.refresh).pack(side="right")

        cols = ("slot", "fs", "baseline", "live", "mod", "name")
        self.tree = ttk.Treeview(self, columns=cols, show="headings", height=16)
        for c, w, t in [("slot", 45, "slot"), ("fs", 40, "FS"),
                        ("baseline", 85, "baseline"), ("live", 85, "live"),
                        ("mod", 65, "mod"), ("name", 220, "Driver")]:
            self.tree.heading(c, text=t)
            self.tree.column(c, width=w, anchor="center" if c != "name" else "w")
        self.tree.pack(fill="both", expand=True, padx=8)
        self.tree.bind("<<TreeviewSelect>>", lambda e: self._sel())

        ctl = ttk.LabelFrame(self, text="Form control (player excluded)")
        ctl.pack(fill="x", padx=8, pady=6)

        row1 = ttk.Frame(ctl)
        row1.pack(fill="x", pady=4)
        for label, f in PRESETS:
            ttk.Button(row1, text=label,
                       command=lambda v=f: self.apply(v)).pack(side="left", padx=2)

        row2 = ttk.Frame(ctl)
        row2.pack(fill="x", pady=4)
        ttk.Label(row2, text="custom factor:").pack(side="left")
        self.factor_var = tk.StringVar(value="0.99")
        ttk.Entry(row2, textvariable=self.factor_var, width=7).pack(side="left", padx=4)
        ttk.Label(row2, text="fade minutes (0 = instant):").pack(side="left", padx=(12, 0))
        self.minutes_var = tk.StringVar(value="0")
        ttk.Entry(row2, textvariable=self.minutes_var, width=5).pack(side="left", padx=4)
        ttk.Button(row2, text="Apply", command=self.apply_custom).pack(side="left", padx=8)
        ttk.Button(row2, text="Reset driver", command=self.reset_one).pack(side="left")

        hint = ("1% = ~0.9 s/lap at a 90 s track. Pace walks to the new value over "
                "seconds - looks natural. Session reload clears everything.")
        ttk.Label(ctl, text=hint, foreground="#555").pack(anchor="w", pady=(2, 4))

        self.log = tk.Text(self, height=6, state="disabled", foreground="#333")
        self.log.pack(fill="x", padx=8, pady=(0, 8))

    # ---------------- helpers ----------------
    def logline(self, msg):
        self.log.configure(state="normal")
        self.log.insert("end", time.strftime("%H:%M:%S  ") + msg + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def connect(self):
        if self.g:
            return True
        if find_pid():
            try:
                self.g = Game()
                return True
            except SystemExit:
                pass
        self.g = None
        return False

    def refresh(self):
        if not self.connect():
            self.status.config(text="game NOT running (or not admin)", foreground="#a00")
            return
        self.status.config(text="connected", foreground="#080")
        st = alf.load_state()
        selected = self._selected_slot()
        self.tree.delete(*self.tree.get_children())
        for x in alf.cars(self.g):
            key = f'{x["slot"]}:{x["name"]}'
            mod = f'x{x["baseline"]/st[key]["original"]:.3f}' if key in st else ""
            name = x["name"] + ("  (player)" if x["slot"] == alf.PLAYER_SLOT else "")
            self.tree.insert("", "end", iid=str(x["slot"]),
                             values=(x["slot"], x["fs"], f'{x["baseline"]:.5f}',
                                     f'{x["live"]:.5f}', mod, name))
        if selected is not None and self.tree.exists(str(selected)):
            self.tree.selection_set(str(selected))

    def _tick(self):
        try:
            self.refresh()
        except Exception as e:  # keep GUI alive on transient read errors
            self.status.config(text=f"read error: {e}", foreground="#a00")
        self.after(POLL_MS, self._tick)

    def _selected_slot(self):
        sel = self.tree.selection()
        return int(sel[0]) if sel else None

    def _sel(self):
        pass

    def _car(self):
        slot = self._selected_slot()
        if slot is None:
            messagebox.showinfo("select", "select a driver row first")
            return None
        if slot == alf.PLAYER_SLOT:
            messagebox.showinfo("nope", "that's you - AI only")
            return None
        for x in alf.cars(self.g):
            if x["slot"] == slot:
                return x
        return None

    # ---------------- actions ----------------
    def apply(self, factor):
        car = self._car()
        if not car:
            return
        if not (alf.FACTOR_MIN <= factor <= alf.FACTOR_MAX):
            if not messagebox.askyesno(
                    "EXTREME value",
                    f"x{factor:.3f} is outside the safe range "
                    f"[{alf.FACTOR_MIN}, {alf.FACTOR_MAX}].\n\n"
                    "Beyond ~+-3% the AI asks for more than its physics tables "
                    "allow: braking points move past the corner, wall likely. "
                    "5-10x = guaranteed crash missile.\n\n"
                    "Apply anyway (testing)?"):
                return
        elif factor > 1.03:
            if not messagebox.askyesno(
                    "over-drive warning",
                    f"x{factor:.3f} exceeds the AI capability tables (brake/grip "
                    "usage) - corner overshoot likely. Continue?"):
                return
        minutes = self._minutes()
        if minutes > 0:
            self._start_fade(car, factor, minutes)
            return
        st = alf.load_state()
        try:
            alf.apply_factor(self.g, st, car, factor, quiet=True)
            self.logline(f'{car["name"]}: x{factor:.3f} applied (instant)')
        except SystemExit as e:
            messagebox.showerror("refused", str(e))
        self.refresh()

    def apply_custom(self):
        try:
            factor = float(self.factor_var.get())
        except ValueError:
            messagebox.showerror("bad factor", "factor must be a number like 0.99")
            return
        self.apply(factor)

    def _minutes(self):
        try:
            return max(0.0, float(self.minutes_var.get()))
        except ValueError:
            return 0.0

    def _start_fade(self, car, factor, minutes):
        if self.fade_thread and self.fade_thread.is_alive():
            messagebox.showinfo("fade busy", "a fade is already running")
            return
        st = alf.load_state()
        key = f'{car["slot"]}:{car["name"]}'
        if key not in st:
            st[key] = {"original": car["baseline"]}
            alf.save_state(st)
        orig = st[key]["original"]
        start = car["baseline"]
        steps = max(1, int(minutes * 60 / 5.0))
        self.logline(f'{car["name"]}: fading x{start/orig:.3f} -> x{factor:.3f} '
                     f'over {minutes:.1f} min')

        def run():
            g2 = Game()
            for i in range(1, steps + 1):
                if self.fade_stop.is_set():
                    self.logline(f'{car["name"]}: fade STOPPED at {i * 100 // steps}%')
                    return
                f_now = (start / orig) + (factor - start / orig) * i / steps
                try:
                    alf.apply_factor(g2, st, car, f_now, quiet=True)
                except Exception:
                    return
                time.sleep(5.0)
            self.logline(f'{car["name"]}: fade complete (x{factor:.3f})')

        self.fade_stop.clear()
        self.fade_thread = threading.Thread(target=run, daemon=True)
        self.fade_thread.start()

    def stop_fades(self):
        self.fade_stop.set()

    def reset_one(self):
        car = self._car()
        if not car:
            return
        st = alf.load_state()
        key = f'{car["slot"]}:{car["name"]}'
        if key in st:
            self.g.write_val(car["veh"] + alf.OFF_BASELINE, "float", st[key]["original"])
            del st[key]
            alf.save_state(st)
            self.logline(f'{car["name"]}: restored to {st.get(key, {}).get("original", 0):.5f}'
                         if False else f'{car["name"]}: restored')
        else:
            self.logline(f'{car["name"]}: no captured original')
        self.refresh()

    def reset_all(self):
        st = alf.load_state()
        if not st:
            self.logline("nothing to reset")
            return
        for x in alf.cars(self.g):
            key = f'{x["slot"]}:{x["name"]}'
            if key in st:
                self.g.write_val(x["veh"] + alf.OFF_BASELINE, "float",
                                 st[key]["original"])
        alf.save_state({})
        self.logline("all baselines restored")
        self.refresh()


if __name__ == "__main__":
    App().mainloop()
