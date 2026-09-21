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
    npc = {"npc_success", "npc_budget", "npc_choice", "npc_events", "npc_stale", "npc_changed", "npc_waiting",
           "npc_late_change", "npc_shape", "npc_allocation", "npc_unwind", "npc_protect", "npc_flush"}
    seq = {"seq_mode0", "seq_mode2", "seq_no_actor", "seq_hidden", "seq_inactive", "seq_bad_event",
           "seq_rejected_type", "seq_paused", "seq_mode4", "seq_wrong_state", "seq_shape", "seq_callback", "seq_ambiguous"}
    expected |= {"image_headers_unreadable", "image_code_unreadable", "image_unwind_unreadable",
                 "image_literal_unreadable", "image_no_code", "image_text", "image_duplicate_names",
                 "image_unusual_names", "image_split_code", "image_cross_section_ambiguity",
                 "image_unsorted_unwind", "image_many_sections"}
    expected |= npc | seq
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
        image = obj["image_validation"]
        assert image["stage"] == values["Image validation stage"]
        assert str(image["executable_section_count"]) == values["Executable sections"]
        assert image["section_count"] == len(image["sections"])
        if obj["status"] == "active":
            assert image["stage"] == "ready" and image["reason"] is None
        if path.parent.name == "image_many_sections":
            assert image["section_count"] == 96
        if path.parent.name == "image_text":
            assert image["sections"][0]["name"] == ".text"
        if path.parent.name == "image_split_code":
            assert image["executable_section_count"] == 3
        if path.parent.name in {"image_headers_unreadable", "image_code_unreadable", "image_unwind_unreadable", "image_unsorted_unwind", "image_no_code"}:
            assert obj["status"] == "disabled" and image["reason"] == obj["reason"]
        dialogue = obj["npc_dialogue"]
        assert ("enabled" if dialogue["enabled"] else "disabled") == values["NPC dialogue feature"]
        for field, key in [("accepted", "NPC dialogue requests accepted"),
                           ("progression_steps", "NPC dialogue progression steps"),
                           ("choice_stops", "NPC choice boundaries"),
                           ("completion_paths", "NPC completion paths"),
                           ("reason", "NPC feature reason")]:
            assert str(dialogue[field]) == values[key], (path.parent.name, field)
        sequence = obj["sequencer_dialogue"]
        assert ("enabled" if sequence["enabled"] else "disabled") == values["Sequencer dialogue feature"]
        for field, key in [("mode_0_prompt_repairs", "Sequencer mode 0 prompt repairs"),
                           ("mode_2_prompt_repairs", "Sequencer mode 2 prompt repairs"),
                           ("hold_completions_forwarded", "Sequencer hold completions forwarded"),
                           ("mode_changes_after_native_input", "Sequencer mode changes after native input"),
                           ("guard_blocks", "Sequencer guard blocks"),
                           ("reason", "Sequencer feature reason")]:
            assert str(sequence[field]) == values[key], (path.parent.name, field)
        for field in ("appearance_callbacks_by_mode_0_to_4_then_other", "input_callbacks_by_mode_0_to_4_then_other"):
            counters = sequence[field]
            assert len(counters) == 6 and all(type(n) is int and n >= 0 for n in counters)
        assert sequence["prompt_repairs"] == sequence["mode_0_prompt_repairs"] + sequence["mode_2_prompt_repairs"]
        assert sequence["native_style_state_verified"] <= sequence["prompt_repairs"]
        assert "last_view" not in obj["runtime"] and "last_skip_input" not in obj["runtime"]
    results = {p.parent.name: json.loads(p.read_text())["npc_dialogue"] for p in reports if p.parent.name in npc}
    for name in ("npc_success", "npc_events", "npc_budget"):
        d = results[name]
        assert d["enabled"] == 1 and d["accepted"] == 1 and d["completion_paths"] == 1
        assert d["progression_steps"] == (399 if name == "npc_budget" else 11)
        assert d["guard_blocks"] == 0 and d["stalls"] == 0
    assert results["npc_choice"]["choice_stops"] == 1 and results["npc_choice"]["completion_paths"] == 0
    for name in ("npc_stale", "npc_changed", "npc_waiting"):
        assert results[name]["accepted"] == 0 and results[name]["progression_steps"] == 0
    for name in ("npc_late_change", "npc_shape", "npc_allocation", "npc_unwind", "npc_protect", "npc_flush"):
        assert results[name]["enabled"] == 0 and results[name]["progression_steps"] == 0
    sequence_results = {p.parent.name: json.loads(p.read_text()) for p in reports if p.parent.name in seq}
    for name, obj in sequence_results.items():
        d = obj["sequencer_dialogue"]
        assert obj["status"] == "active" and obj["npc_dialogue"]["enabled"] == 1
        assert obj["npc_dialogue"]["accepted"] == 0 and obj["npc_dialogue"]["progression_steps"] == 0
        assert d["enabled"] == (0 if name in {"seq_shape", "seq_callback", "seq_ambiguous"} else 1)
        if name in {"seq_shape", "seq_callback", "seq_ambiguous", "seq_paused", "seq_mode4", "seq_wrong_state"}:
            assert d["prompt_repairs"] == d["hold_completions_forwarded"] == 0
        else:
            assert d["prompt_repairs"] == d["native_style_state_verified"] == 101
            assert d["mode_0_prompt_repairs"] == (101 if name == "seq_mode0" else 0)
            assert d["mode_2_prompt_repairs"] == (0 if name == "seq_mode0" else 101)
            assert d["guard_blocks"] == (1 if name in {"seq_hidden", "seq_inactive", "seq_bad_event"} else 0)
            assert d["hold_completions_forwarded"] == (0 if name in {"seq_hidden", "seq_inactive", "seq_bad_event"} else 1)
            assert d["mode_changes_after_native_input"] == (1 if name in {"seq_mode0", "seq_mode2"} else 0)
            mode = 0 if name == "seq_mode0" else 2
            assert d["appearance_callbacks_by_mode_0_to_4_then_other"][mode] >= 201
            assert d["input_callbacks_by_mode_0_to_4_then_other"][mode] == 2
    print(f"PASS {len(reports)} compiled-ASI JSON snapshots parsed; session, process, revision, status, reason and counters match their logs")

if __name__ == "__main__":
    main()
