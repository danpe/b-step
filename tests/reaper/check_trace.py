"""Validate a completed seven-instance host-tempo-test.lua trace directory."""
import collections
import csv
from pathlib import Path
import sys

traces = sorted(Path(sys.argv[1]).glob("clock-*.csv"))
assert len(traces) == 7, f"Expected 7 instances, found {len(traces)}"
reference = None
for path in traces:
    assert "dropped," not in path.read_text(), f"Trace overflow: {path}"
    rows = list(csv.DictReader(path.open()))
    start = int(next(r["block"] for r in rows if r["event"] == "1"))
    end = int(next(r["block"] for r in rows
                   if r["event"] == "3" and float(r["quarter_notes"]) > 500))
    continuous = [r for r in rows if start <= int(r["block"]) < end]
    assert not any(r["event"] in ("2", "3", "4", "5") for r in continuous)
    clocks = [r for r in continuous if r["event"] == "10"]
    assert len(clocks) > 650
    for a, b in zip(clocks, clocks[1:]):
        assert int(b["clock_or_midi"]) == int(a["clock_or_midi"]) + 1
    signature = [(r["event"], r["samples"], r["offset"], r["clock_or_midi"])
                 for r in continuous if r["event"] in ("10", "20")]
    if reference is None:
        reference = signature
    assert signature == reference, f"Instance timing differs: {path}"
    events = collections.Counter(r["event"] for r in rows)
    assert events["1"] == 2 and events["2"] == 2 and events["3"] == 3
    assert events["4"] >= 2 and events["5"] == 0
    held = set()
    for row in rows:
        if row["event"] == "20":
            raw = int(row["clock_or_midi"])
            status, pitch, velocity = (raw >> 16) & 255, (raw >> 8) & 127, raw & 127
            key = (status & 15, pitch)
            if status & 240 == 144 and velocity:
                held.add(key)
            else:
                held.discard(key)
    assert not held, f"Notes remain held after stop: {path}"
    print(f"{path.name}: {len(clocks)} continuous clocks, intentional transport OK, no held notes")
print("PASS: all seven instances emit identical clock and MIDI timing during the tempo sweep")
