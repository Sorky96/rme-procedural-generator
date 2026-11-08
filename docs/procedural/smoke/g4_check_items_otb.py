#!/usr/bin/env python3
"""G4-0 / D11: independent consistency check of items.otb against Tibia.dat of one client.

Read-only. Uses tools/tibia_data.py as a library. Compares, per OTB item that has a client id,
OTB group/flags with the DAT flags of that client id, and reads the G3 import reports of the
reference maps (header items version, unknown ids). Output is deterministic JSON on stdout.

Usage: py -I g4_check_items_otb.py --dat Tibia.dat --spr Tibia.spr --otb items.otb [--reports G3_SET_DIR]
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[3] / "tools"
sys.path.insert(0, str(TOOLS))
import tibia_data as td  # noqa: E402


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--dat", required=True)
    p.add_argument("--spr", required=True)
    p.add_argument("--otb", required=True)
    p.add_argument("--reports")
    a = p.parse_args()

    args = td.build_parser().parse_args(["--dat", a.dat, "--spr", a.spr, "--otb", a.otb, "info"])
    ctx = td.Context(args)
    otb = td.load_otb(ctx)
    ctx.detect_dat_features()
    first_id, last_id = td.dat_thing_range(ctx.dat_header)["item"]
    keys = {("item", i): None for i in range(first_id, last_id + 1)}
    things = ctx.collect_dat_things(keys)

    pairs = [
        ("otb_ground_vs_dat_isGround", lambda s, t: (s.group_raw == 1) == t.flags["isGround"]),
        ("otb_unpassable_vs_dat_isUnpassable",
         lambda s, t: bool(s.model_flags.get("unpassable")) == t.flags["isUnpassable"]),
        ("otb_stackable_vs_dat_stackable",
         lambda s, t: bool(s.model_flags.get("stackable")) == t.flags["stackable"]),
        ("otb_container_vs_dat_isContainer", lambda s, t: (s.group_raw == 2) == t.flags["isContainer"]),
        ("otb_blockMissiles_vs_dat_blockMissile",
         lambda s, t: bool(s.model_flags.get("blockMissiles")) == t.flags["blockMissile"]),
        ("otb_pickupable_vs_dat_pickupable",
         lambda s, t: bool(s.model_flags.get("pickupable")) == t.flags["pickupable"]),
    ]
    mismatch = {name: 0 for name, _ in pairs}
    compared = 0
    no_client = 0
    out_of_range = 0
    deprecated = 0
    for s in otb.items:
        if s.group_raw == 14:
            deprecated += 1
            continue
        cid = s.client_id
        if not cid:
            no_client += 1
            continue
        t = things.get(("item", cid))
        if t is None:
            out_of_range += 1
            continue
        compared += 1
        for name, fn in pairs:
            if not fn(s, t):
                mismatch[name] += 1

    result = {
        "schema": "g4-items-otb-check/1",
        "otb_sha256": sha256(a.otb),
        "dat_sha256": sha256(a.dat),
        "otb_header": {"major": otb.version_major, "minor": otb.version_minor,
                       "build": otb.version_build, "csd": otb.csd_version},
        "otb_items": len(otb.items),
        "otb_deprecated": deprecated,
        "otb_without_client_id": no_client,
        "otb_client_id_outside_dat_range": out_of_range,
        "dat_item_range": [first_id, last_id],
        "compared_items": compared,
        "mismatches": mismatch,
    }

    if a.reports:
        maps = {}
        for d in sorted(Path(a.reports).glob("M[0-9][0-9]")):
            rp = d / "run" / "reference-report.json"
            if not rp.exists():
                continue
            r = json.loads(rp.read_text(encoding="utf-8"))
            h = r["header"]
            maps[d.name] = {"client": r["client"], "items_major": h["items_major"],
                            "items_minor": h["items_minor"], "compat": r["compatibility"],
                            "unknown_ground": r["unknown_ground"], "unknown_items": r["unknown_items"],
                            "unknown_distinct_ids": len(r["unknown_ids"])}
        result["maps"] = maps
    json.dump(result, sys.stdout, indent=2, sort_keys=True)
    print()


if __name__ == "__main__":
    main()
