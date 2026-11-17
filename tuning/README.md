# Real boiler tuning results (2026-11-17)

Measured on the user's machine with the `pid-tuning` firmware (probe setpoint 97 C, pump 20 s = about 101 ml In at
5.1 ml/s, valve open, water inlet about 22 C). One run per setting, so differences of a few tenths of a degree are noise.
The heater element (1100 W) is saturated from the start of the flow (100 ml from 22 to 97 C needs about 1.5 kW).

## Feedforward (Kp 40 / Ki 0.1 / Kd 500)

| ff | delay | drop of the probe | back within 0.5 C |
|---|---|---|---|
| 0 | - | 11.5 C (to 85.2) | about 90 s |
| 0.3 | 0 | 9.7 C | 78 s |
| 0.5 | 0 | 8.7 C | 68 s |
| 0.7 | 0 | 8.0 C | 108 s (one run) |
| 1.0 | 0 | 7.75 C | 69 s, no overshoot |
| 1.0 | 3 / 6 s | 9.2 / 9.4 C | worse |
| 0.7 | 3 s | 9.4 C | worse |

Best: ff 1.0, delay 0 (a delay only loses time with a saturated element).

## PID gains (ff 1.0)

| Kp / Ki / Kd | drop | back within 0.5 C | max after the minimum (setpoint 97) | end |
|---|---|---|---|---|
| 40 / 0.1 / 500 (old) | 7.4 | 74 s | 96.7 | 96.6 (0.4 under) |
| **40 / 0.3 / 500** | 6.5 | 62 s | 97.1 | 97.0 |
| 40 / 0.6 / 500 | 6.8 | 57 s | 97.7 | 97.35 |
| 80 / 0.1 / 500 | 6.7 | 51 s | 98.7 (overshoot) | 96.6 |
| 20 / 0.1 / 500 | 6.2 | 111 s | 96.4 | 96.3 |
| 40 / 0.1 / 0 | 6.8 | 56 s | 98.4 (overshoot) | 96.3 |
| 40 / 0.1 / 1000 | 6.9 | 60 s | 96.7 | 96.5 |

Best: Kp 40, Ki 0.3, Kd 500 (no permanent offset, no overshoot): now in `master`.

## Files
- `data/ff_series_live.csv`, `data/ff_results.txt`: feedforward series (telemetry `ms,probe,setpoint,pid_ms,ff_ms,on_ms,heater,pump,flow_in,vol_in,vol_out` and the summary lines).
- `data/pid_series_live.csv`, `data/pid_results.txt`: PID gains series.
- `data/earlier_run_live2.csv`, `data/earlier_run_live3.csv`: raw logs of earlier runs (preparation and an aborted run), kept for completeness.
- `scripts/`: the PowerShell tools used (Windows PowerShell): `live_server.ps1` (reads COM8, charts on localhost:8765, token-protected commands, automatic stop above 104 C), `ff_series.ps1` and `pid_series.ps1` (drive the server over HTTP). They expect their CSV in the current folder (`live.csv`, `live2.csv`).
