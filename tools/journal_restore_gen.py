"""
journal_restore_gen.py: turns a saved journal (the text the machine answers to j, e.g. docs/calibration/journal_2026-11-28.txt)
into tools/journal_restore_data.h, that tools/journal_restore.cpp writes back into the EEPROM of the machine (env journal_restore).

    python tools/journal_restore_gen.py docs/calibration/journal_2026-11-28.txt [--loss SEQ=ML ...]

The loss of the journal is now in whole ml. In the text it is a decimal: it is rounded. The text of a journal written before 2026-11-28
shows 0.0 for a loss above 63.75 ml (the old field wrapped): give the real value with --loss (e.g. --loss 4-11=64), otherwise
it is taken as In - Out - cup when the extraction reached its target (cup > 0) and left as it is when not.
"""
import re
import sys

ENTRY = re.compile(
    r"#(?P<seq>\d+)\s+(?P<reason>\w+)\s+ended in (?P<phase>\w+)\s+tgt (?P<tgt>\d+) pre (?P<pre>\d+) wait (?P<wait>\d+)"
    r"\s*\|\s*(?P<dur>\d+) s\s*\|\s*in (?P<inn>[\d.]+) out (?P<out>[\d.]+) net (?P<net>-?[\d.]+) loss (?P<loss>[\d.]+) cup (?P<cup>\d+)"
    r"(?: cal (?P<cal>\d+))?\s*\|\s*delay (?P<delay>[\d.]+)\s*\|\s*(?P<t0>\d+) -> (?P<t1>\d+) C")
CURVE = re.compile(r"CURVE #(?P<seq>\d+) (?P<step>\d+) ms (?P<count>\d+) \| in (?P<inn>[\d,]*) \| out (?P<out>[\d,]*)")
REASONS = {"TARGET": "END_TARGET", "USER": "END_USER", "NO_WATER": "END_NO_WATER", "MAX_TIME": "END_MAX_TIME", "TIMER": "END_TIMER"}
PHASES = {"PRE": "EXTRACTION_PREINFUSION", "WAIT": "EXTRACTION_PREINFUSION_WAIT", "RUN": "EXTRACTION_RUNNING"}


def main():
    path = sys.argv[1]
    forced = {}
    args = sys.argv[2:]
    for i, a in enumerate(args):
        if a == "--loss":
            spec, ml = args[i + 1].split("=")
            first, _, last = spec.partition("-")
            for s in range(int(first), int(last or first) + 1):
                forced[s] = int(round(float(ml)))

    entries, curves = [], {}
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        m = ENTRY.match(line)
        if m:
            entries.append(m.groupdict())
            continue
        m = CURVE.match(line)
        if m:
            ins = [int(x) for x in m.group("inn").split(",") if x]
            outs = [int(x) for x in m.group("out").split(",") if x]
            assert len(ins) == len(outs) == int(m.group("count")), line[:40]
            curves[int(m.group("seq"))] = (ins, outs)
    entries.sort(key=lambda e: int(e["seq"]))
    seqs = [int(e["seq"]) for e in entries]
    assert seqs == list(range(len(seqs))), "the entries must be #0, #1, ... in a row: " + str(seqs)

    out = ["// Written by tools/journal_restore_gen.py from " + path.replace("\\", "/") + ": do not edit.",
           "// reason, phase, target, preinfusion, wait, duration, inTenths, outTenths, loss (ml), cup, delay100, tempStart, tempEnd, cal",
           "static const RestoredEntry RESTORED_ENTRIES[] = {"]
    for e in entries:
        seq = int(e["seq"])
        loss = float(e["loss"])
        if seq in forced:
            loss = forced[seq]
        elif loss == 0.0 and int(e["cup"]) > 0:
            loss = float(e["net"]) - int(e["cup"])   # wrapped: the cup is In - Out - loss
        loss = max(0, min(255, int(round(loss))))
        out.append("  {%s, %s, %s, %s, %s, %s, %d, %d, %d, %s, %d, %s, %s, %s},   // #%d" % (
            REASONS[e["reason"]], PHASES[e["phase"]], e["tgt"], e["pre"], e["wait"], e["dur"],
            round(float(e["inn"]) * 10), round(float(e["out"]) * 10), loss, e["cup"], round(float(e["delay"]) * 100),
            e["t0"], e["t1"], e["cal"] or "0", seq))
    out.append("};")
    out.append("")
    for seq, (ins, outs) in sorted(curves.items()):
        data = ",".join("%d,%d" % (a, b) for a, b in zip(ins, outs))
        out.append("static const uint8_t RESTORED_CURVE_%d[] = {%s};" % (seq, data))
    out.append("")
    out.append("static const RestoredCurve RESTORED_CURVES[] = {")
    for seq, (ins, _) in sorted(curves.items()):
        out.append("  {%d, %d, RESTORED_CURVE_%d}," % (seq, len(ins), seq))
    out.append("};")
    open("tools/journal_restore_data.h", "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    print("%d entries, %d curves" % (len(entries), len(curves)))


main()
