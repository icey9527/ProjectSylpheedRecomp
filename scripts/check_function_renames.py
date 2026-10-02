"""Verify that generated snapshots differ only in registered names/declaration ordering."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

from symbol_index import generated_registrations


def compare(before, after):
    old = generated_registrations(before)
    new = generated_registrations(after)
    if not old or old.keys() != new.keys():
        raise ValueError("Generated registration address set changed or is empty")
    renames = {}
    for address in old:
        if old[address] != new[address]:
            current, previous = new[address], old[address]
            if current in renames and renames[current] != previous:
                raise ValueError(f"Ambiguous renamed identifier: {current}")
            renames[current] = previous
    if not renames:
        raise ValueError("No function name changes found")
    pattern = re.compile(r"(?<![A-Za-z0-9_])(?:__imp__)?(?:" +
                         "|".join(re.escape(name) for name in sorted(renames, key=len, reverse=True)) +
                         r")(?![A-Za-z0-9_])")

    def normalize(match):
        value = match[0]
        prefix = "__imp__" if value.startswith("__imp__") else ""
        return prefix + renames[value[len(prefix):]]

    suffixes = {".cpp", ".h", ".cmake"}
    files = {path.name for path in before.iterdir() if path.suffix in suffixes}
    if files != {path.name for path in after.iterdir() if path.suffix in suffixes}:
        raise ValueError("Generated source/header file set changed")
    declaration = re.compile(r"(?m)^DECLARE_REX_FUNC\((\w+)\);\n")
    changed = []
    for filename in sorted(files):
        original = (before / filename).read_text(encoding="utf-8")
        current = (after / filename).read_text(encoding="utf-8")
        normalized = pattern.sub(normalize, current)
        if original != normalized:
            # Per-file declaration headers are sorted by C++ name. After
            # reversing the names, check the declaration multiset and every
            # other byte; do not relax checks on C++ function bodies.
            declarations_equal = Counter(declaration.findall(original)) == Counter(
                declaration.findall(normalized))
            remaining_equal = declaration.sub("", original) == declaration.sub("", normalized)
            if Path(filename).suffix != ".h" or not declarations_equal or not remaining_equal:
                raise ValueError(f"Change beyond identifiers/declaration ordering: {filename}")
        if original != current:
            changed.append(filename)
    return dict(registered_addresses=len(old), restored_names=len(renames),
                source_files_checked=len(files), files_changed=len(changed),
                only_identifiers_and_declaration_order_changed=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", required=True, type=Path)
    parser.add_argument("--after", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    try:
        report = compare(args.before, args.after)
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report))
        return 0
    except (OSError, ValueError) as error:
        print(f"Rename verification failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
