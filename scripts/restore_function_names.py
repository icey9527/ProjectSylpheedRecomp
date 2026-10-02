"""Restore readable names for existing CONFIG entries without adding function boundaries.

Python 3.11+; Windows DbgHelp is needed when deriving C++ decorated names.
Writes only the name field in the three established function configuration files.
"""

import argparse
import hashlib
import json
from pathlib import Path, PureWindowsPath
import re
import sys
import tomllib

from symbol_index import demangle, read_map, read_pdb

REPO = Path(__file__).resolve().parents[1]
CONFIG_FILES = ("map-functions.toml", "runtime-functions.toml", "catch-boundaries.toml")


def verify_inputs(repo):
    manifest = json.loads((repo / "config/game-inputs.json").read_text(encoding="utf-8"))
    for entry in manifest["files"]:
        path = repo / "assets" / entry["name"]
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        if path.stat().st_size != entry["size"] or digest != entry["sha256"]:
            raise ValueError(f"Input mismatch: {entry['name']}; no name configuration written")


def choose_symbol(rows, public, modules):
    verified = [row for row in rows if row["map_name"] in
                (public.get((row["segment"], row["offset"]), set()) |
                 {entry[0] for entry in modules.get((row["segment"], row["offset"]), set())})]
    if not verified:
        raise ValueError("No exact MAP/PDB name and section:offset match")
    # One deterministic link name is needed; other aliases remain in the MAP,
    # index and report rather than being discarded or asserted equivalent.
    return min(verified, key=lambda row: (row["map_name"], row["object"]))


def cpp_name(row):
    raw = row["map_name"]
    readable = demangle(raw, 0x1000)  # UNDNAME_NAME_ONLY: class/function, no signature.
    if raw.startswith("?") and readable == raw:
        raise ValueError(f"Cannot undecorate {row['address']}; Windows DbgHelp is required")
    if raw.startswith("__catch$"):
        owner = PureWindowsPath(row["object"].split(":")[-1]).stem
        readable = f"{owner}::Catch_{raw.split('$', 1)[1]}"
    base = re.sub(r"[^A-Za-z0-9_]+", "_", readable).strip("_")
    if not base:
        raise ValueError(f"Empty readable identifier for {row['address']}")
    if base[0].isdigit():
        base = "Function_" + base
    # Avoid reserved double-underscore identifiers, collisions from overloads,
    # punctuation folding, long templates, or repeated local catch numbers.
    base = re.sub(r"_+", "_", base)[:120].rstrip("_")
    return f"{base}_{int(row['address'], 16):08X}", readable


def edit_config(text, names):
    before = tomllib.loads(text)["functions"]
    if set(before) != set(names):
        raise ValueError("Name update must cover exactly the existing CONFIG entries")
    pattern = re.compile(r'(?m)^("(0x[0-9A-Fa-f]+)"\s*=\s*\{)([^\r\n]*)(\})')
    seen = set()

    def replace(match):
        address = match[2]
        name = names[address]
        seen.add(address)
        fields = match[3].strip()
        existing = before[address].get("name")
        if existing:
            if existing != name:
                raise ValueError(f"Refusing to overwrite a customized name at {address}")
            return match[0]
        if "name" in before[address]:
            fields = re.sub(r'\bname\s*=\s*""\s*,?\s*', "", fields).strip().rstrip(",")
        new_fields = "name = " + json.dumps(name) + (", " + fields if fields else "")
        return match[1] + " " + new_fields + " " + match[4]

    updated = pattern.sub(replace, text)
    if seen != set(names):
        raise ValueError("Expected quoted, single-line inline function configuration entries")
    after = tomllib.loads(updated)["functions"]
    for address, config in before.items():
        original = {k: v for k, v in config.items() if k != "name"}
        actual = {k: v for k, v in after[address].items() if k != "name"}
        if original != actual or after[address]["name"] != names[address]:
            raise ValueError(f"Non-name field changed at {address}")
    return updated


def plan_updates(repo):
    verify_inputs(repo)
    _, rows = read_map(repo / "assets/Xacalite_ScriptTeam.map")
    _, public, modules = read_pdb(repo / "assets/Xacalite_ScriptTeam.pdb")
    by_address = {}
    for row in rows:
        by_address.setdefault(row["address"], []).append(row)
    updates, report, used_names = [], [], set()
    for filename in CONFIG_FILES:
        path = repo / "config" / filename
        text = path.read_bytes().decode("utf-8")
        entries = tomllib.loads(text)["functions"]
        names = {}
        for address in entries:
            selected = choose_symbol(by_address.get(address, []), public, modules)
            name, readable = cpp_name(selected)
            if name in used_names:
                raise ValueError(f"Duplicate restored name: {name}")
            used_names.add(name)
            names[address] = name
            position = (selected["segment"], selected["offset"])
            report.append(dict(address=address, name=name, readable=readable,
                               selected_raw_name=selected["map_name"], object=selected["object"],
                               public_exact=selected["map_name"] in public.get(position, set()),
                               module_exact=selected["map_name"] in
                               {entry[0] for entry in modules.get(position, set())},
                               aliases=[dict(name=row["map_name"], object=row["object"])
                                        for row in by_address[address]]))
        updates.append((path, text, edit_config(text, names)))
    return updates, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify configured names without writing")
    parser.add_argument("--report", type=Path, help="Optional local JSON evidence; not a codegen input")
    args = parser.parse_args()
    try:
        updates, report = plan_updates(REPO)  # Validate every input/file before any write.
        pending = [path for path, before, after in updates if before != after]
        if args.check and pending:
            raise ValueError("Names need regeneration: " + ", ".join(path.name for path in pending))
        if not args.check:
            for path, before, after in updates:
                if before != after:
                    path.write_text(after, encoding="utf-8", newline="")
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                   encoding="utf-8")
        print(f"{'Verified' if args.check else 'Restored'} {len(report)} function names; "
              "CONFIG addresses and non-name fields preserved. No new entries added.")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Name restoration failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
