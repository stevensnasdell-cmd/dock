#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Steven Snasdell
"""Dock's factory Line-ups from text files.

A Line-up file (".dock", JSON) holds the whole band: what the emulator's "Export Line-up" button downloads
(emu/dock_export.c has the format). The factory Line-ups are such files in firmware/lineups/, in the order that
firmware/lineups/order lists; this tool checks them and writes firmware/src/dock_factory.h, the table the firmware
builds in (dock_lineups.c DOCK_LINEUPS). So "make my Yacht the real one" is:

    python3 tools/dock_lineup_import.py ~/Downloads/yacht.dock

which replaces firmware/lineups/yacht.dock (a new name is added at the end of the order) and writes the header.

    --as NAME        import it under another name (up to 7 letters): a new Line-up
    --check          only check that dock_factory.h is what the files make (tests/dock_tests.sh)
    --header OUT     write just these files' table to OUT (the tests' export -> import round trip)
    --regen          write dock_factory.h again from the files (after editing one by hand)
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LINEUPS = os.path.join(ROOT, "firmware", "lineups")
ORDER = os.path.join(LINEUPS, "order")
HEADER = os.path.join(ROOT, "firmware", "src", "dock_factory.h")
MEMBERS = ["Bass", "Keys", "Lead", "Drums"]
NAME_MAX = 7          # dock_lineups.c DOCK_NAME 8, 0-terminated
PATCH_MAX = 144       # DOCK_PATCH
FX_SLOTS = 3


class Bad(Exception):
    pass


def _int(v, lo, hi, what):
    if not isinstance(v, int) or isinstance(v, bool) or not lo <= v <= hi:
        raise Bad(f"{what}: {v!r} is not a whole number from {lo} to {hi}")
    return v


def load(path, rename=None):
    """a .dock file -> a checked record (dict)"""
    try:
        with open(path, encoding="utf-8") as f:
            j = json.load(f)
    except (OSError, ValueError) as e:
        raise Bad(f"{path}: not a Line-up file ({e})")
    if j.get("dock_lineup") != 1:
        raise Bad(f"{path}: not a Dock Line-up (format 1) file")
    name = rename or j.get("name")
    if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9 \-]{1,%d}" % NAME_MAX, name):
        raise Bad(f"{path}: the name {name!r} must be 1 to {NAME_MAX} letters, digits, spaces or dashes")
    pc = _int(j.get("p_count"), 1, 1024, "p_count")
    r = {"name": name, "p_count": pc}
    v = j.get("voices")
    if not isinstance(v, list) or len(v) != 4:
        raise Bad(f"{path}: voices must be four numbers")
    r["voices"] = [_int(x, 0, 255, "voices") for x in v]
    room = j.get("room") or {}
    fx = room.get("fx")
    if not isinstance(fx, list) or len(fx) != FX_SLOTS:
        raise Bad(f"{path}: the room needs {FX_SLOTS} effect slots")
    r["room"] = {"mode": _int(room.get("mode"), 0, 1, "room mode"), "return": _int(room.get("return"), 0, 255, "return"),
                 "fx": [{"type": _int(s.get("type"), 0, 255, "fx type"), "name": str(s.get("name", "")),
                         "mix": _int(s.get("mix"), 0, 255, "fx mix"), "rate": _int(s.get("rate"), 0, 255, "fx rate"),
                         "char": _int(s.get("char"), 0, 255, "fx char"), "tone": _int(s.get("tone"), -128, 127, "fx tone")}
                        for s in fx]}
    m = j.get("macros", [[0] * 4] * 4)
    if not isinstance(m, list) or len(m) != 4 or any(not isinstance(x, list) or len(x) != 4 for x in m):
        raise Bad(f"{path}: macros must be four rows of four")
    r["macros"] = [[_int(x, -64, 63, "macro") for x in row] for row in m]   # Tone Bite Shape Space per part (0: as made)
    g = j.get("groove", [0] * 4)
    if not isinstance(g, list) or len(g) != 4:
        raise Bad(f"{path}: groove must be four numbers")
    r["groove"] = [_int(x, 0, 255, "groove") for x in g]
    parts = j.get("parts")
    if not isinstance(parts, list) or len(parts) != 4:
        raise Bad(f"{path}: a Line-up has four parts (Bass, Keys, Lead, Drums)")
    r["parts"] = []
    for k, p in enumerate(parts):
        what = f"{path}: {MEMBERS[k]}"
        params = p.get("params")
        if not isinstance(params, list) or len(params) != pc:
            raise Bad(f"{what}: params must be {pc} numbers")
        hexs = p.get("patch", [])
        hexs = "".join(hexs) if isinstance(hexs, list) else str(hexs)
        if not re.fullmatch(r"([0-9a-fA-F]{2})*", hexs) or len(hexs) // 2 > PATCH_MAX:
            raise Bad(f"{what}: the patch must be hex, {PATCH_MAX} bytes at most")
        env, lfo = p.get("env", [0] * 4), p.get("lfo", [0] * 2)   # the common ENV / LFO offsets (0: as made)
        if not isinstance(env, list) or len(env) != 4 or not isinstance(lfo, list) or len(lfo) != 2:
            raise Bad(f"{what}: env must be four numbers (ATK DEC SUS REL), lfo two (RATE DEPTH)")
        r["parts"].append({"member": MEMBERS[k], "engine": _int(p.get("engine"), 0, 255, f"{what} engine"),
                           "engine_name": str(p.get("engine_name", "")), "preset": _int(p.get("preset"), 0, 255, f"{what} preset"),
                           "sound": str(p.get("sound", "")),
                           "env": [_int(x, -64, 63, f"{what} env") for x in env],
                           "lfo": [_int(x, -64, 63, f"{what} lfo") for x in lfo],
                           "params": [_int(x, -32768, 32767, f"{what} param {i}") for i, x in enumerate(params)],
                           "patch": bytes.fromhex(hexs)})
    if r["parts"][3]["engine"] != 10 or any(p["engine"] == 10 for p in r["parts"][:3]):
        raise Bad(f"{path}: only the drummer (the fourth part) plays DRUM (engine 10), and it plays nothing else")
    return r


def _s(v):
    return json.dumps(v, ensure_ascii=True)


def dump(r):
    """a record -> the .dock text (the layout emu/dock_export.c writes)"""
    o = ["{", '  "dock_lineup": 1,', f'  "name": {_s(r["name"])},', f'  "p_count": {r["p_count"]},',
         f'  "voices": [{", ".join(map(str, r["voices"]))}],',
         f'  "room": {{"mode": {r["room"]["mode"]}, "return": {r["room"]["return"]}, "fx": [']
    fx = r["room"]["fx"]
    for i, s in enumerate(fx):
        o.append(f'    {{"type": {s["type"]}, "name": {_s(s["name"])}, "mix": {s["mix"]}, "rate": {s["rate"]}, '
                 f'"char": {s["char"]}, "tone": {s["tone"]}}}' + ("," if i + 1 < len(fx) else ""))
    o.append("  ]},")
    o.append('  "macros": [' + ", ".join("[" + ", ".join(map(str, m)) + "]" for m in r["macros"]) + "],")
    o.append(f'  "groove": [{", ".join(map(str, r["groove"]))}],')
    o.append('  "parts": [')
    for k, p in enumerate(r["parts"]):
        o.append(f'    {{"member": {_s(p["member"])}, "engine": {p["engine"]}, "engine_name": {_s(p["engine_name"])}, '
                 f'"preset": {p["preset"]}, "sound": {_s(p["sound"])},')
        o.append(f'     "env": [{", ".join(map(str, p["env"]))}], "lfo": [{", ".join(map(str, p["lfo"]))}],')
        rows = [", ".join(map(str, p["params"][i:i + 16])) for i in range(0, len(p["params"]), 16)]
        o.append('     "params": [' + ",\n                ".join(rows) + "],")
        h = p["patch"].hex()
        lines = [h[i:i + 64] for i in range(0, len(h), 64)]
        o.append('     "patch": [' + ",\n               ".join(_s(x) for x in lines) + "]}" + ("," if k + 1 < 4 else ""))
    o.append("  ]")
    o.append("}")
    return "\n".join(o) + "\n"


def _c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def header(recs, sources, array="DOCK_LINEUPS"):
    """records -> the C table (dock_lineups.c's dock_lineup_t)"""
    pcs = {r["p_count"] for r in recs}
    if len(pcs) != 1:
        raise Bad(f"the Line-ups were made by firmware with different parameter counts: {sorted(pcs)}")
    pc = pcs.pop()
    o = ["/* SPDX-License-Identifier: GPL-3.0-only",
         " * Copyright (C) 2026 Steven Snasdell",
         " * Generated by tools/dock_lineup_import.py from " + ", ".join(sources) + ": do not edit.",
         " * Change a Line-up on the emulator, export it and import it with the tool (or edit its file, then --regen). */",
         f"#define DOCK_FACTORY_P_COUNT {pc}",
         '_Static_assert(P_COUNT == DOCK_FACTORY_P_COUNT, "the factory Line-ups were made with another parameter count: '
         're-export them");',
         f"static const dock_lineup_t {array}[] = {{"]
    for r in recs:
        fx = ", ".join("{%d, %d, %d, %d, %d}" % (s["type"], s["mix"], s["rate"], s["char"], s["tone"]) for s in r["room"]["fx"])
        o.append("    {%s, {%s}, {%d, %d, {%s}}," % (_c_str(r["name"]), ", ".join(map(str, r["voices"])),
                                                    r["room"]["mode"], r["room"]["return"], fx))
        o.append("     {%s}, {%s}," % (", ".join("{" + ", ".join(map(str, m)) + "}" for m in r["macros"]),
                                      ", ".join(map(str, r["groove"]))))
        o.append("     {")
        for k, p in enumerate(r["parts"]):
            o.append("      /* %s: %s %s */" % (p["member"], p["engine_name"] or p["engine"],
                                                p["sound"].replace("*/", "* /") or "#%d" % p["preset"]))
            o.append("      {%d, %d," % (p["engine"], p["preset"]))
            rows = [", ".join(map(str, p["params"][i:i + 20])) for i in range(0, len(p["params"]), 20)]
            o.append("       {" + ",\n        ".join(rows) + "},")
            b = p["patch"]
            rows = [", ".join("0x%02x" % x for x in b[i:i + 16]) for i in range(0, len(b), 16)]
            o.append("       {" + (",\n        ".join(rows) if b else "0") + "}}" + ("," if k + 1 < 4 else ""))
        o.append("     },")
        o.append("     {%s}}," % ", ".join("{" + ", ".join(map(str, p["env"] + p["lfo"] + [0, 0])) + "}" for p in r["parts"]))
    o.append("};")
    return "\n".join(o) + "\n"


def order():
    with open(ORDER, encoding="utf-8") as f:
        return [l.strip() for l in f if l.strip() and not l.startswith("#")]


def factory_header():
    names = order()
    recs = [load(os.path.join(LINEUPS, n + ".dock")) for n in names]
    seen = set()
    for r in recs:
        if r["name"].lower() in seen:
            raise Bad(f"two Line-ups are named {r['name']}")
        seen.add(r["name"].lower())
    return header(recs, ["firmware/lineups/{" + ",".join(names) + "}.dock"])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*", help="exported .dock files")
    ap.add_argument("--as", dest="rename", help="import under this name")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--regen", action="store_true")
    ap.add_argument("--header", metavar="OUT")
    ap.add_argument("--array", default="DOCK_LINEUPS", help=argparse.SUPPRESS)
    a = ap.parse_args()
    try:
        if a.header:
            recs = [load(f, a.rename) for f in a.files]
            with open(a.header, "w", encoding="utf-8") as f:
                f.write(header(recs, [os.path.basename(x) for x in a.files], a.array))
            return 0
        if a.check:
            with open(HEADER, encoding="utf-8") as f:
                ok = f.read() == factory_header()
            print("dock_factory.h matches firmware/lineups/" if ok else
                  "dock_factory.h differs from firmware/lineups/: run tools/dock_lineup_import.py --regen")
            return 0 if ok else 1
        if not a.files and not a.regen:
            ap.print_help()
            return 2
        if a.rename and len(a.files) != 1:
            raise Bad("--as takes one file")
        names = order()
        for path in a.files:
            r = load(path, a.rename)
            stem = re.sub(r"[^a-z0-9]+", "-", r["name"].lower()).strip("-")
            if stem not in names:
                names.append(stem)
                print(f"new Line-up {r['name']}: added at the end ({len(names)} in all)")
            with open(os.path.join(LINEUPS, stem + ".dock"), "w", encoding="utf-8") as f:
                f.write(dump(r))
            print(f"{r['name']}: firmware/lineups/{stem}.dock")
        with open(ORDER, "w", encoding="utf-8") as f:
            f.write("# the factory Line-ups in SELECT's order (the first plays at power-on): their files' names\n")
            f.write("".join(n + "\n" for n in names))
        text = factory_header()
        with open(HEADER, "w", encoding="utf-8") as f:
            f.write(text)
        print("firmware/src/dock_factory.h written: build the firmware or the emulator to hear it")
        return 0
    except Bad as e:
        print("dock_lineup_import: " + str(e), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
