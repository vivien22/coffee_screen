#!/usr/bin/env python3
"""Plots a telemetry log of the pid-tuning firmware.

    pio device monitor --filter log2file     (or any serial terminal that saves the output to a file)
    python3 tools/plot_log.py logfile.txt [picture.png]

The lines that do not start with a number (comments "# ...", the header) are ignored.
Needs matplotlib. Without a picture name, a window is opened.
"""
import sys

import matplotlib
if len(sys.argv) > 2:
    matplotlib.use("Agg")
import matplotlib.pyplot as plt

COLUMNS = ["ms", "probe", "setpoint", "pid_ms", "ff_ms", "on_ms", "heater", "pump", "flow_in", "vol_in", "vol_out"]
rows = []
for line in open(sys.argv[1], errors="replace"):
    parts = line.strip().split(",")
    if len(parts) != len(COLUMNS) or not parts[0].isdigit():
        continue
    rows.append([float(p) if p != "nan" else float("nan") for p in parts])
if not rows:
    sys.exit("no telemetry line found")

data = {name: [row[i] for row in rows] for i, name in enumerate(COLUMNS)}
seconds = [ms / 1000.0 for ms in data["ms"]]

fig, (top, middle, bottom) = plt.subplots(3, 1, sharex=True, figsize=(10, 8))
top.plot(seconds, data["probe"], label="probe")
top.plot(seconds, data["setpoint"], "--", label="setpoint")
top.set_ylabel("deg C")
top.legend()
middle.plot(seconds, data["pid_ms"], label="PID")
middle.plot(seconds, data["ff_ms"], label="feedforward")
middle.plot(seconds, data["on_ms"], label="total")
middle.set_ylabel("ms of heating per window")
middle.legend()
bottom.plot(seconds, data["flow_in"], label="flow in (ml/s)")
bottom.fill_between(seconds, data["pump"], step="mid", alpha=0.2, label="pump")
bottom.set_ylabel("ml/s")
bottom.set_xlabel("s")
bottom.legend()
fig.tight_layout()

if len(sys.argv) > 2:
    fig.savefig(sys.argv[2], dpi=110)
else:
    plt.show()
