#!/usr/bin/env python3
"""Summarizes a per-frame trace (docs/VITA-60.md) in about 25 lines: where the time goes, which action and menu cause spikes.
   python tools/vita-trace.py <trace.csv|trace.bin> [--scenario FILE] [--log FILE] [--budget MS] [--baseline FILE.json] [--save FILE.json]
CSV columns include frame,tick,steps,instr,menu,action,marks,logic_us,emit_us,work_us,layout_us,draw_us,swap_us,upload_us.
Older PC CSV traces use total_us.
.bin = the Vita host's records: 'NGUT', u32 count, then 16 little-endian u32 per frame:
frame,t_us,work_us,logic,layout,emit,draw,swap,upload,tick,steps,instr,menu,action,marks,flags."""
import sys, struct, json, argparse, statistics as st

ap = argparse.ArgumentParser()
ap.add_argument("trace"); ap.add_argument("--scenario"); ap.add_argument("--log")
ap.add_argument("--budget", type=float, default=16.7); ap.add_argument("--baseline"); ap.add_argument("--save")
a = ap.parse_args()

COLS = "frame t_us work_us logic_us layout_us emit_us draw_us swap_us upload_us tick steps instr menu action marks flags".split()
def load(path):
    if path.endswith(".bin"):
        b = open(path, "rb").read()
        assert b[:4] == b"NGUT", "not a trace"
        n = (len(b) - 8) // 64  # the header count is not rewritten by the host
        return [dict(zip(COLS, struct.unpack_from("<16I", b, 8 + 64 * i))) for i in range(n)]
    rows = []
    with open(path) as f:
        head = f.readline().strip().split(",")
        for line in f:
            rows.append({k: int(v) for k, v in zip(head, line.strip().split(","))})
    return rows

rows = load(a.trace)
if not rows: sys.exit("empty trace")
def work(r):
    if "work_us" in r:
        return r["work_us"]
    if "total_us" in r:
        return r["total_us"]
    return sum(r.get(k, 0) for k in ("logic_us", "emit_us", "draw_us"))

# scenario actions in execution order (one per frame, `pad abc` = three) and mark names from the log
acts = []
if a.scenario:
    for line in open(a.scenario):
        line = line.strip()
        if not line or line.startswith("#"): continue
        t, verb, *rest = line.split(None, 2)
        rest = rest[0] if rest else ""
        acts += [f"{t} pad {c}" for c in rest.replace(" ", "")] if verb == "pad" else [line]
marks = []
if a.log:
    for line in open(a.log, errors="replace"):
        if "[autoplay] mark " in line:
            marks.append(line.split("[autoplay] mark ", 1)[1].split()[1])
def mark_of(r):
    i = r.get("marks", 0)
    return marks[i - 1] if 0 < i <= len(marks) else (f"mark{i}" if i else "start")

ms = [work(r) / 1000 for r in rows]
if "t_us" in rows[0] and len(rows) > 10:
    iv = [(b["t_us"] - c["t_us"]) / 1000 for c, b in zip(rows, rows[1:]) if 0 < b["t_us"] - c["t_us"] < 5_000_000]
    drops = sum(1 for x in iv if x > 25.0)  # more than 1.5 refreshes between two frames = a missed vblank
    if iv:
        repeats = sum(r.get("flags", 0) & 1 for r in rows)
        print(f"render loop {1000 / (sum(iv) / len(iv)):.1f} fps average; intervals over 25 ms: {drops} ({100 * drops / len(iv):.1f}%), longest gap {max(iv):.0f} ms; records flagging scanout repeats {repeats}")
over = [i for i, m in enumerate(ms) if m > a.budget]
q = lambda xs, p: sorted(xs)[min(len(xs) - 1, int(len(xs) * p))]
out = {"frames": len(rows), "p50": q(ms, .5), "p95": q(ms, .95), "p99": q(ms, .99), "max": max(ms), "over": len(over)}
print(f"{len(rows)} frames, {rows[-1].get('tick', 0)} ticks | work ms p50 {out['p50']:.1f} p95 {out['p95']:.1f} p99 {out['p99']:.1f} max {out['max']:.1f} | over {a.budget} ms: {len(over)} ({100*len(over)/len(rows):.1f}%)")
for k in ("logic_us", "layout_us", "emit_us", "draw_us", "swap_us", "upload_us"):
    if any(r.get(k) for r in rows): print(f"  {k[:-3]:>7}: p50 {q([r.get(k, 0) / 1000 for r in rows], .5):.1f} p99 {q([r.get(k, 0) / 1000 for r in rows], .99):.1f} ms")

# per phase (mark): worst phases first
ph = {}
for r, m in zip(rows, ms): ph.setdefault(mark_of(r), []).append(m)
worst = sorted(ph.items(), key=lambda kv: -q(kv[1], .99))[:6]
print("worst phases (p50 / p99 / max ms, frames over budget):")
for name, v in worst: print(f"  {name:<14} {q(v, .5):5.1f} {q(v, .99):6.1f} {max(v):6.1f}  {sum(x > a.budget for x in v)}/{len(v)}")

# worst spikes with the action that precedes them
print("worst spikes (frame, tick, ms, phase, last action):")
for i in sorted(range(len(ms)), key=lambda i: -ms[i])[:5]:
    r = rows[i]; aid = r.get("action", 0)
    print(f"  #{i} tick {r.get('tick', 0)} {ms[i]:.0f} ms {mark_of(r)} | {acts[aid - 1] if 0 < aid <= len(acts) else 'action ' + str(aid)}")
big = [rows[i].get("tick", 0) for i in range(len(ms)) if ms[i] > 3 * max(out["p50"], 1)]
gaps = [b - c for c, b in zip(big, big[1:]) if b - c > 3]
if len(gaps) >= 4:
    print(f"spikes > 3x median: {len(big)}; typical spacing {st.median(gaps):.0f} ticks (~{st.median(gaps) / 30:.1f} s of game time)")

if a.save: json.dump(out, open(a.save, "w"))
if a.baseline:
    b = json.load(open(a.baseline))
    print("vs baseline: " + ", ".join(f"{k} {out[k]:.1f}/{b[k]:.1f}" for k in ("p50", "p99", "max")) + f", over {out['over']}/{b['over']}")
