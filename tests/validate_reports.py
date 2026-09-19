#!/usr/bin/env python3
"""Parse compiled-ASI harness reports and cross-check their paired text logs."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    header = (Path(__file__).resolve().parent.parent / "source/Version.h").read_text()
    version = re.search(r'#define\s+SULMP_VERSION\s+"([0-9.]+)"', header).group(1)
    reports = sorted(args.directory.glob("*/report.json"))
    expected = {"success", "busy", "allocation", "unwind", "protect", "suspend", "context", "flush",
                "duplicate_after", "case", "mutex", "pin", "image", "shape", "short", "write", "zero", "rename"}
    assert {p.parent.name for p in reports} == expected, "Missing/unexpected diagnostic test scenarios"
    for path in reports:
        obj = json.loads(path.read_text(encoding="utf-8"))
        text = (path.parent / "report.log").read_text(encoding="utf-8")
        values = dict(line.split(": ", 1) for line in text.splitlines() if ": " in line)
        assert obj["version"] == version and obj["schema_version"] == 1
        for field, key in [("session_id", "Session"), ("process_id", "Process ID"),
                           ("revision", "Revision"), ("status", "Status"), ("reason", "Reason")]:
            assert str(obj[field]) == values[key], (path.parent.name, field)
        for field, key in [("appearance_repairs", "Appearance repairs"),
                           ("native_style_state_verified", "Native style-state checks passed"),
                           ("safety_gate_blocks", "Safety-gate blocks")]:
            assert str(obj["runtime"][field]) == values[key]
        assert str(obj["diagnostics"]["previous_write_errors"]) == values["Previous diagnostic write errors"]
        assert "last_view" not in obj["runtime"] and "last_skip_input" not in obj["runtime"]
    print(f"PASS {len(reports)} compiled-ASI JSON snapshots parsed; session, process, revision, status, reason and counters match their logs")

if __name__ == "__main__":
    main()
