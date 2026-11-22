#!/usr/bin/env python3
"""journal_view: a graphical view of the journal of the last extractions (one HTML page, no dependency, no network).

The journal is read by the journal_dump sketch (tools/journal_dump.cpp), which prints one line per extraction on the serial line.
This script turns those lines into a page with three charts and a table:

  1. where the water went, for each extraction: the volume in the cup, the loss (CAL) and the water that came back by the Out
     sensor (the three add up to the water counted by In); a tick marks the target volume; under the bars, how it ended;
  2. the duration of each extraction;
  3. the temperature at the start and at the end.

Usage:
  python tools/journal_view.py dump.txt                 # the text printed by journal_dump (saved from a serial monitor)
  python tools/journal_view.py --port COM8              # reads the board itself (needs pyserial: pip install pyserial); the
                                                        # board must run the journal_dump sketch (pio run -e journal_dump -t upload)
  python tools/journal_view.py dump.txt -o page.html    # another name for the page (default: journal.html)
  python tools/journal_view.py --demo                   # an example with made up extractions

The extractions are drawn from the oldest (left) to the last one (right). The page follows the light or dark theme of the system.
"""
import argparse
import html
import re
import sys

LINE = re.compile(
    r"#(?P<seq>\d+)\s+(?P<reason>\w+)\s+ended in (?P<phase>\w+)\s+tgt (?P<tgt>\d+) pre (?P<pre>\d+) wait (?P<wait>\d+)"
    r"\s*\|\s*(?P<dur>\d+) s\s*\|\s*in (?P<inn>[\d.]+) out (?P<out>[\d.]+) net (?P<net>-?[\d.]+) loss (?P<loss>[\d.]+) cup (?P<cup>\d+)(?: cal (?P<cal>\d+))?"
    r"\s*\|\s*delay (?P<delay>[\d.]+)\s*\|\s*(?P<t0>\d+) -> (?P<t1>\d+) C")

# how an extraction ended: label, glyph (never the color alone), color token (status colors of the palette)
REASONS = {
    "TARGET":   ("target reached", "✓", "good"),
    "USER":     ("stopped with the button", "●", "info"),
    "NO_WATER": ("no water (no-flow safety)", "✕", "critical"),
    "MAX_TIME": ("maximum duration", "!", "warning"),
    "TIMER":    ("timer elapsed", "◷", "info"),
}
PHASES = {"PRE": "pre-infusion", "WAIT": "wait", "RUN": "main extraction"}


def parse(text):
    """The entries, oldest first (the dump prints the last one first)."""
    entries = []
    for line in text.splitlines():
        m = LINE.search(line)
        if m:
            d = m.groupdict()
            entries.append({
                "seq": int(d["seq"]), "reason": d["reason"], "phase": d["phase"], "target": int(d["tgt"]), "pre": int(d["pre"]),
                "wait": int(d["wait"]), "duration": int(d["dur"]), "inn": float(d["inn"]), "out": float(d["out"]),
                "net": float(d["net"]), "loss": float(d["loss"]), "cup": int(d["cup"]), "cal": int(d["cal"] or 0), "delay": float(d["delay"]),
                "t0": int(d["t0"]), "t1": int(d["t1"]),
            })
    entries.reverse()
    return entries


def read_port(port):
    try:
        import serial
        import time
    except ImportError:
        sys.exit("--port needs pyserial: pip install pyserial")
    s = serial.Serial(port, 115200, timeout=2)
    s.dtr = False
    time.sleep(0.1)
    s.dtr = True   # reset: the sketch prints at start
    text, end = "", time.time() + 15
    while time.time() < end and "END" not in text:
        text += s.read(400).decode(errors="replace")
    return text


def demo():
    rows = [
        # reason, phase, target, pre, wait, duration, in, out, loss, cup, delay, t0, t1
        ("TARGET", "RUN", 40, 0, 0, 22, 71.0, 0.6, 30.0, 40, 0.39, 91, 88),
        ("TARGET", "RUN", 40, 0, 0, 23, 72.4, 1.2, 30.0, 41, 0.38, 90, 87),
        ("USER", "RUN", 60, 40, 5, 20, 80.9, 0.8, 36.2, 44, 0.37, 90, 85),
        ("NO_WATER", "PRE", 60, 40, 5, 9, 12.3, 0.0, 36.3, 0, 0.37, 62, 66),
        ("TARGET", "RUN", 60, 40, 5, 34, 99.0, 2.1, 36.3, 60, 0.37, 91, 86),
        ("MAX_TIME", "RUN", 60, 40, 5, 121, 76.0, 0.0, 36.3, 40, 0.37, 91, 80),
        ("USER", "WAIT", 60, 40, 5, 12, 41.0, 0.3, 36.3, 4, 0.37, 90, 88),
        ("TARGET", "RUN", 60, 40, 5, 33, 98.2, 1.9, 36.3, 60, 0.36, 90, 85),
    ]
    out = []
    for i, r in enumerate(reversed(rows)):
        out.append("#%d %s  ended in %s  tgt %d pre %d wait %d | %d s | in %.1f out %.1f net %.1f loss %.1f cup %d | delay %.2f | %d -> %d C" % (
            len(rows) - 1 - i, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[6] - r[7], r[8], r[9], r[10], r[11], r[12]))
    return "\n".join(out)


def nice_max(v):
    """A round top for an axis, and the ticks."""
    if v <= 0:
        return 10, [0, 5, 10]
    for step in (1, 2, 5, 10, 20, 25, 50, 100, 200):
        if v / step <= 5:
            top = step * (int(v / step) + (0 if v % step == 0 else 1))
            return top, [i * step for i in range(int(top / step) + 1)]
    return int(v) + 1, [0, int(v) + 1]


def chart_body(entries):
    n = len(entries)
    slot = max(16, min(64, 900 // max(n, 1)))
    bar = max(10, int(slot * 0.62))
    left, right = 62, 20
    width = max(left + right + n * slot, 640)   # a page with a few extractions is not blown up

    def x(i):
        return left + i * slot + (slot - bar) / 2

    def esc(s):
        return html.escape(str(s), quote=True)

    parts = []

    def tip(e):
        reason = REASONS.get(e["reason"], (e["reason"], "?", "info"))
        cup_calc = e["inn"] - e["out"] - e["loss"]
        return ("#%d: %s, in the %s&#10;target %d ml, pre-infusion %d ml, wait %d s&#10;in %.1f, out %.1f, net %.1f ml&#10;"
                "loss %.1f ml, final volume %d ml (In - Out - loss = %.1f)%s&#10;duration %d s, stop delay %.2f s&#10;temperature %d -> %d C") % (
            e["seq"], esc(reason[0]), PHASES.get(e["phase"], e["phase"]), e["target"], e["pre"], e["wait"], e["inn"], e["out"],
            e["net"], e["loss"], e["cup"], cup_calc, ("&#10;CAL: cup weighed %d g" % e["cal"]) if e["cal"] else "", e["duration"], e["delay"], e["t0"], e["t1"])

    # ---------------- chart 1: the water, stacked
    top, ticks = nice_max(max([e["inn"] for e in entries] + [e["target"] for e in entries]) * 1.05)
    h1, y0 = 250, 20

    def y1(v):
        return y0 + h1 - v / top * h1

    s = ['<svg viewBox="0 0 %d %d" role="img" aria-labelledby="t1">' % (width, y0 + h1 + 76),
         '<title id="t1">Water of each extraction: cup, loss and Out, with the target volume</title>']
    for t in ticks:
        s.append('<line class="grid" x1="%d" x2="%d" y1="%.1f" y2="%.1f"/><text class="axis" x="%d" y="%.1f" text-anchor="end">%d</text>' % (
            left, width - right, y1(t), y1(t), left - 8, y1(t) + 4, t))
    s.append('<text class="axis" x="14" y="%.1f" transform="rotate(-90 14 %.1f)" text-anchor="middle">ml</text>' % (y0 + h1 / 2, y0 + h1 / 2))
    for i, e in enumerate(entries):
        net = max(e["net"], 0.0)
        loss = min(e["loss"], net)
        cup = max(net - loss, 0.0)
        segs = [("cup", cup), ("loss", loss), ("out", e["out"])]
        acc = 0.0
        s.append('<g class="bar" data-tip="%s" tabindex="0">' % tip(e))
        s.append('<rect class="hit" x="%.1f" y="%d" width="%d" height="%d"/>' % (left + i * slot, y0, slot, h1 + 76))
        for name, v in segs:
            if v <= 0:
                continue
            ya, yb = y1(acc + v), y1(acc)
            hgt = max(yb - ya, 1)
            rounded = ' rx="3"' if name == "out" or (name == "cup" and loss == 0 and e["out"] <= 0) else ""
            s.append('<rect class="seg %s" x="%.1f" y="%.1f" width="%d" height="%.1f"%s/>' % (name, x(i), yb - hgt, bar, hgt, rounded))
            acc += v
        # target tick
        s.append('<line class="target" x1="%.1f" x2="%.1f" y1="%.1f" y2="%.1f"/>' % (x(i) - 4, x(i) + bar + 4, y1(e["target"]), y1(e["target"])))
        # how it ended: a glyph and a letter for the phase (never the color alone)
        label, glyph, tone = REASONS.get(e["reason"], (e["reason"], "?", "info"))
        cx = x(i) + bar / 2
        s.append('<circle class="dot %s" cx="%.1f" cy="%d" r="9"/><text class="glyph" x="%.1f" y="%d" text-anchor="middle">%s</text>' % (
            tone, cx, y0 + h1 + 24, cx, y0 + h1 + 28, esc(glyph)))
        s.append('<text class="axis" x="%.1f" y="%d" text-anchor="middle">%s</text>' % (cx, y0 + h1 + 50, esc(e["phase"][0])))
        s.append('<text class="axis" x="%.1f" y="%d" text-anchor="middle">#%d</text>' % (cx, y0 + h1 + 68, e["seq"]))
        s.append('</g>')
    s.append('</svg>')
    parts.append("".join(s))

    # ---------------- chart 2: the duration
    top2, ticks2 = nice_max(max(e["duration"] for e in entries) * 1.05)
    h2 = 110
    s = ['<svg viewBox="0 0 %d %d" role="img" aria-labelledby="t2">' % (width, y0 + h2 + 24),
         '<title id="t2">Duration of each extraction in seconds</title>']
    for t in ticks2:
        yy = y0 + h2 - t / top2 * h2
        s.append('<line class="grid" x1="%d" x2="%d" y1="%.1f" y2="%.1f"/><text class="axis" x="%d" y="%.1f" text-anchor="end">%d</text>' % (
            left, width - right, yy, yy, left - 8, yy + 4, t))
    s.append('<text class="axis" x="14" y="%.1f" transform="rotate(-90 14 %.1f)" text-anchor="middle">s</text>' % (y0 + h2 / 2, y0 + h2 / 2))
    for i, e in enumerate(entries):
        hh = max(e["duration"] / top2 * h2, 1)
        s.append('<g class="bar" data-tip="%s" tabindex="0"><rect class="hit" x="%.1f" y="%d" width="%d" height="%d"/>'
                 '<rect class="seg neutral" x="%.1f" y="%.1f" width="%d" height="%.1f" rx="3"/></g>' % (
                     tip(e), left + i * slot, y0, slot, h2 + 24, x(i), y0 + h2 - hh, bar, hh))
    s.append('</svg>')
    parts.append("".join(s))

    # ---------------- chart 3: the temperature, start -> end
    lo = min(min(e["t0"], e["t1"]) for e in entries)
    hi = max(max(e["t0"], e["t1"]) for e in entries)
    lo, hi = int(lo / 5) * 5 - 5, (int(hi / 5) + 1) * 5 + 5
    h3 = 130

    def y3(v):
        return y0 + h3 - (v - lo) / (hi - lo) * h3

    s = ['<svg viewBox="0 0 %d %d" role="img" aria-labelledby="t3">' % (width, y0 + h3 + 24),
         '<title id="t3">Temperature at the start (hollow) and at the end (filled) of each extraction</title>']
    for t in range(lo, hi + 1, 10 if hi - lo > 30 else 5):
        s.append('<line class="grid" x1="%d" x2="%d" y1="%.1f" y2="%.1f"/><text class="axis" x="%d" y="%.1f" text-anchor="end">%d</text>' % (
            left, width - right, y3(t), y3(t), left - 8, y3(t) + 4, t))
    s.append('<text class="axis" x="14" y="%.1f" transform="rotate(-90 14 %.1f)" text-anchor="middle">°C</text>' % (y0 + h3 / 2, y0 + h3 / 2))
    for i, e in enumerate(entries):
        cx = x(i) + bar / 2
        s.append('<g class="bar" data-tip="%s" tabindex="0"><rect class="hit" x="%.1f" y="%d" width="%d" height="%d"/>'
                 '<line class="templine" x1="%.1f" x2="%.1f" y1="%.1f" y2="%.1f"/>'
                 '<circle class="tstart" cx="%.1f" cy="%.1f" r="4.5"/><circle class="tend" cx="%.1f" cy="%.1f" r="4.5"/></g>' % (
                     tip(e), left + i * slot, y0, slot, h3 + 24, cx, cx, y3(e["t0"]), y3(e["t1"]), cx, y3(e["t0"]), cx, y3(e["t1"])))
    s.append('</svg>')
    parts.append("".join(s))
    return parts


PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Journal of the extractions</title>
<style>
:root {
  color-scheme: light;
  --surface: #fcfcfb; --page: #f3f2ef; --ink: #0b0b0b; --ink2: #52514e; --muted: #75746f; --grid: #e4e3de;
  --cup: #2a78d6; --loss: #eb6834; --out: #1baf7a; --neutral: #8a8984;
  --good: #0ca30c; --warning: #fab219; --critical: #d03b3b; --info: #2a78d6;
}
@media (prefers-color-scheme: dark) { :root:where(:not([data-theme="light"])) {
  color-scheme: dark;
  --surface: #1a1a19; --page: #121211; --ink: #f0efec; --ink2: #c3c2b7; --muted: #99988f; --grid: #2f2f2d;
  --cup: #3987e5; --loss: #d95926; --out: #199e70; --neutral: #a6a59e; --info: #3987e5;
} }
:root[data-theme="dark"] {
  color-scheme: dark;
  --surface: #1a1a19; --page: #121211; --ink: #f0efec; --ink2: #c3c2b7; --muted: #99988f; --grid: #2f2f2d;
  --cup: #3987e5; --loss: #d95926; --out: #199e70; --neutral: #a6a59e; --info: #3987e5;
}
* { box-sizing: border-box; }
body { margin: 0; background: var(--page); color: var(--ink); font: 15px/1.45 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }
main { max-width: 1040px; margin: 0 auto; padding: 24px 16px 48px; }
h1 { font-size: 22px; margin: 0 0 4px; }
.sub { color: var(--ink2); margin: 0 0 20px; }
section { background: var(--surface); border-radius: 12px; padding: 16px 16px 12px; margin-bottom: 16px; }
h2 { font-size: 16px; margin: 0 0 2px; }
.note { color: var(--muted); font-size: 13px; margin: 0 0 10px; }
.legend { display: flex; flex-wrap: wrap; gap: 6px 18px; margin: 4px 0 10px; font-size: 13px; color: var(--ink2); }
.legend span { display: inline-flex; align-items: center; gap: 6px; }
.sw { width: 12px; height: 12px; border-radius: 3px; display: inline-block; }
.sw.cup { background: var(--cup); } .sw.loss { background: var(--loss); } .sw.out { background: var(--out); }
.tick { width: 16px; height: 0; border-top: 2px solid var(--ink); display: inline-block; }
.scroll { overflow-x: auto; }
svg { display: block; width: 100%; min-width: 520px; height: auto; }
.grid { stroke: var(--grid); stroke-width: 1; }
.axis { fill: var(--muted); font-size: 12px; }
.seg { stroke: var(--surface); stroke-width: 2; }
.seg.cup { fill: var(--cup); } .seg.loss { fill: var(--loss); } .seg.out { fill: var(--out); } .seg.neutral { fill: var(--neutral); }
.target { stroke: var(--ink); stroke-width: 2; }
.dot.good { fill: var(--good); } .dot.warning { fill: var(--warning); } .dot.critical { fill: var(--critical); } .dot.info { fill: var(--info); }
.glyph { fill: #fff; font-size: 12px; font-weight: 700; }
.dot.warning + .glyph { fill: #1a1a19; }
.templine { stroke: var(--neutral); stroke-width: 2; }
.tstart { fill: var(--surface); stroke: var(--ink2); stroke-width: 2; } .tend { fill: var(--ink2); stroke: var(--surface); stroke-width: 2; }
.hit { fill: transparent; }
.bar:hover .hit, .bar:focus .hit { fill: color-mix(in srgb, var(--ink) 6%, transparent); }
.bar:focus { outline: none; }
#tip { position: fixed; z-index: 5; pointer-events: none; display: none; background: var(--ink); color: var(--surface); padding: 8px 10px;
  border-radius: 8px; font-size: 13px; line-height: 1.4; white-space: pre; max-width: 90vw; }
table { border-collapse: collapse; width: 100%; font-size: 13px; }
th, td { text-align: right; padding: 6px 8px; border-bottom: 1px solid var(--grid); white-space: nowrap; }
th { color: var(--ink2); font-weight: 600; } td:nth-child(2), th:nth-child(2), td:nth-child(3), th:nth-child(3) { text-align: left; }
.chip { display: inline-block; min-width: 18px; text-align: center; border-radius: 9px; color: #fff; font-weight: 700; padding: 0 5px; }
.chip.good { background: var(--good); } .chip.warning { background: var(--warning); color: #1a1a19; } .chip.critical { background: var(--critical); } .chip.info { background: var(--info); }
.empty { color: var(--ink2); padding: 24px 0; }
</style></head><body><main>
<h1>Journal of the extractions</h1>
<p class="sub">@SUB@</p>
@CONTENT@
</main><div id="tip"></div>
<script>
const tip = document.getElementById('tip');
document.querySelectorAll('.bar').forEach(function (g) {
  const show = function (x, y) { tip.textContent = g.dataset.tip; tip.style.display = 'block';
    const w = tip.offsetWidth, h = tip.offsetHeight; tip.style.left = Math.max(8, Math.min(x + 14, window.innerWidth - w - 8)) + 'px';
    tip.style.top = Math.max(8, Math.min(y + 14, window.innerHeight - h - 8)) + 'px'; };
  g.addEventListener('mousemove', function (e) { show(e.clientX, e.clientY); });
  g.addEventListener('mouseleave', function () { tip.style.display = 'none'; });
  g.addEventListener('focus', function () { const r = g.getBoundingClientRect(); show(r.left + r.width / 2, r.top); });
  g.addEventListener('blur', function () { tip.style.display = 'none'; });
});
</script></body></html>
"""


def build(entries, source):
    if not entries:
        content = '<section><p class="empty">No extraction in the text: is it the output of the journal_dump sketch?</p></section>'
        return PAGE.replace("@SUB@", html.escape(source)).replace("@CONTENT@", content)
    c1, c2, c3 = chart_body(entries)
    reasons = "".join('<span><span class="chip %s">%s</span> %s</span>' % (t, html.escape(g), html.escape(l)) for l, g, t in
                      [REASONS[k] for k in ("TARGET", "USER", "NO_WATER", "MAX_TIME")])
    content = (
        '<section><h2>Where the water went</h2>'
        '<p class="note">For each extraction, from the oldest (left) to the last one (right): the volume in the cup, the loss (CAL) and the '
        'water that came back by the Out sensor. Together they are the water counted by In. Under the bars: how it ended, and in which '
        'phase (P pre-infusion, W wait, R main extraction). Hover or focus a bar for the details.</p>'
        '<div class="legend"><span><i class="sw cup"></i>cup</span><span><i class="sw loss"></i>loss</span><span><i class="sw out"></i>Out</span>'
        '<span><i class="tick"></i>target</span>%s</div><div class="scroll">%s</div></section>' % (reasons, c1) +
        '<section><h2>Duration</h2><p class="note">Seconds from the start of the extraction (the pre-infusion included).</p><div class="scroll">%s</div></section>' % c2 +
        '<section><h2>Temperature</h2><p class="note">At the start (hollow) and at the end (filled), in degrees C, as shown on the screen.</p><div class="scroll">%s</div></section>' % c3 +
        '<section><h2>The extractions</h2><div class="scroll"><table><thead><tr><th>#</th><th>Ended</th><th>In phase</th><th>Target</th><th>Pre / wait</th><th>s</th>'
        '<th>In</th><th>Out</th><th>Net</th><th>Loss</th><th>Cup</th><th>CAL</th><th>Delay</th><th>&deg;C</th></tr></thead><tbody>%s</tbody></table></div></section>' % table_rows(entries))
    return PAGE.replace("@SUB@", html.escape(source)).replace("@CONTENT@", content)


def table_rows(entries):
    out = []
    for e in reversed(entries):
        label, glyph, tone = REASONS.get(e["reason"], (e["reason"], "?", "info"))
        out.append("<tr><td>#%d</td><td><span class=\"chip %s\">%s</span> %s</td><td>%s</td><td>%d</td><td>%d / %d</td><td>%d</td>"
                   "<td>%.1f</td><td>%.1f</td><td>%.1f</td><td>%.1f</td><td>%d</td><td>%s</td><td>%.2f</td><td>%d &rarr; %d</td></tr>" % (
                       e["seq"], tone, html.escape(glyph), html.escape(label), PHASES.get(e["phase"], e["phase"]), e["target"], e["pre"], e["wait"],
                       e["duration"], e["inn"], e["out"], e["net"], e["loss"], e["cup"], e["cal"] or "", e["delay"], e["t0"], e["t1"]))
    return "".join(out)


def main():
    ap = argparse.ArgumentParser(description="A graphical view (one HTML page) of the journal of the last extractions")
    ap.add_argument("input", nargs="?", help="the text printed by the journal_dump sketch ('-' for the standard input)")
    ap.add_argument("--port", help="a serial port to read the journal from (the journal_dump sketch must be running)")
    ap.add_argument("--demo", action="store_true", help="an example with made up extractions")
    ap.add_argument("-o", "--output", default="journal.html", help="the page to write (default journal.html)")
    a = ap.parse_args()
    if a.demo:
        text, source = demo(), "Example: made up extractions, not the machine's."
    elif a.port:
        text, source = read_port(a.port), "Read from " + a.port
    elif a.input == "-":
        text, source = sys.stdin.read(), "Read from the standard input"
    elif a.input:
        with open(a.input, encoding="utf-8", errors="replace") as f:
            text = f.read()
        source = "Read from " + a.input
    else:
        ap.error("give a file, --port or --demo")
    entries = parse(text)
    with open(a.output, "w", encoding="utf-8") as f:
        f.write(build(entries, "%s %d extraction(s)." % (source, len(entries))))
    print("%s: %d extraction(s)" % (a.output, len(entries)))


if __name__ == "__main__":
    main()
