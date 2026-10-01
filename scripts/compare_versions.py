"""Read-only XEX/PE metadata and relative-path comparison; Python 3.12+.

Uses only the standard library. No unpacking, decryption or guest execution.
Detailed reports belong outside the Git repository. File equality does not
establish that different game executables can use each other's resources.
"""

import argparse
from collections import Counter
import hashlib
from itertools import combinations
import json
from pathlib import Path
import struct


ANALYSIS_EXTENSIONS = {
    ".pdb", ".map", ".idb", ".i64", ".id0", ".id1", ".id2", ".nam", ".til",
    ".log", ".obj", ".lib", ".pyc",
}
HELPER_EXTENSIONS = {".py", ".ps1", ".bat", ".cmd"}
EXECUTABLE_EXTENSIONS = {".exe", ".xex", ".dll"}


def category(path):
    suffix = Path(path).suffix.lower()
    if suffix in ANALYSIS_EXTENSIONS:
        return "analysis_or_build"
    if suffix in HELPER_EXTENSIONS:
        return "helper_script"
    if suffix in EXECUTABLE_EXTENSIONS:
        return "executable"
    # A candidate, not proof of belonging to the game (e.g. readme/config).
    return "content_candidate"


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def version_word(value):
    return {
        "raw": f"0x{value:08X}",
        "decoded": f"{value >> 28}.{(value >> 24) & 15}.{(value >> 8) & 65535}.{value & 255}",
    }


class Header:
    def __init__(self, data):
        self.data = data

    def span(self, offset, size):
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise ValueError(f"Truncated/invalid header range: {offset}+{size}")
        return self.data[offset:offset + size]

    def unpack(self, fmt, offset):
        return struct.unpack(fmt, self.span(offset, struct.calcsize(fmt)))


def parse_xex(data):
    h = Header(data)
    magic, flags, pe_offset, _, security_offset, count = h.unpack(">4s5I", 0)
    if magic != b"XEX2":
        raise ValueError("Not XEX2")
    if pe_offset != len(data):
        raise ValueError("XEX header extent mismatch")
    h.span(24, count * 8)
    options = {}
    for index in range(count):
        key, value = h.unpack(">II", 24 + index * 8)
        if key in options:
            raise ValueError("Duplicate XEX optional header")
        options[key] = value
    result = {"format": "XEX2", "module_flags": f"0x{flags:08X}"}
    for key, field in ((0x10100, "entry_point"), (0x10201, "image_base")):
        if key in options:
            result[field] = f"0x{options[key]:08X}"
    result["image_size"] = h.unpack(">I", security_offset + 4)[0]
    if 0x40006 in options:
        media, version, base, title, platform, table, disc, discs, save = h.unpack(
            ">4I4BI", options[0x40006]
        )
        result.update({
            "title_id": f"{title:08X}", "media_id": f"{media:08X}",
            "version": version_word(version), "base_version": version_word(base),
            "platform": platform, "executable_table": table,
            "disc_number": disc, "disc_count": discs, "savegame_id": f"{save:08X}",
        })
    if 0x183FF in options:
        offset = options[0x183FF]
        size = h.unpack(">I", offset)[0]
        if size < 4:
            raise ValueError("Invalid original PE name size")
        result["original_pe_name"] = h.span(offset + 4, size - 4).split(b"\0", 1)[0].decode(
            "utf-8", errors="replace"
        )
    if 0x103FF in options:
        offset = options[0x103FF]
        size, string_size, names_count = h.unpack(">III", offset)
        if size < 12 or string_size > size - 12:
            raise ValueError("Invalid import table size")
        h.span(offset, size)
        table_data = h.span(offset + 12, string_size)
        cursor = 0
        names = []
        for _ in range(names_count):
            end = table_data.find(b"\0", cursor)
            if end < 0:
                raise ValueError("Truncated import name table")
            names.append(table_data[cursor:end].decode("ascii", errors="replace"))
            cursor = (end + 4) & ~3
        result["import_library_names"] = names
    return result


def inspect_image(path):
    # Read only public container metadata; embedded image may be compressed.
    with path.open("rb") as stream:
        prefix = stream.read(64)
        if prefix.startswith(b"XEX2"):
            pe_offset = Header(prefix).unpack(">I", 8)[0]
            if not 24 <= pe_offset <= min(path.stat().st_size, 16 * 1024 * 1024):
                raise ValueError("Unsupported/invalid XEX header extent")
            stream.seek(0)
            result = parse_xex(stream.read(pe_offset))
        elif prefix.startswith(b"MZ"):
            pe_offset = Header(prefix).unpack("<I", 60)[0]
            stream.seek(pe_offset)
            signature, machine, sections, timestamp = Header(stream.read(12)).unpack("<4sHHI", 0)
            if signature != b"PE\0\0":
                raise ValueError("Invalid PE signature")
            result = {"format": "PE", "machine": f"0x{machine:04X}",
                      "sections": sections, "timestamp": timestamp}
        else:
            raise ValueError("Expected XEX2 or PE image")
    result.update({"bytes": path.stat().st_size, "sha256": sha256(path)})
    return result


def inventory(root):
    files = {}
    extensions, tops, categories = Counter(), Counter(), Counter()
    def fail(error):
        raise error

    for directory, dirs, names in root.walk(on_error=fail, follow_symlinks=False):
        dirs.sort()
        names.sort()
        for name in dirs + names:
            if (directory / name).is_symlink() or (directory / name).is_junction():
                raise ValueError(f"Linked paths are not supported: {directory / name}")
        for name in names:
            file_path = directory / name
            relative = file_path.relative_to(root).as_posix()
            row = {"bytes": file_path.stat().st_size, "category": category(relative)}
            files[relative] = row
            extensions[file_path.suffix.lower() or "(none)"] += 1
            tops[relative.split("/", 1)[0]] += 1
            categories[row["category"]] += 1
    return {
        "file_count": len(files), "bytes": sum(row["bytes"] for row in files.values()),
        "extensions": dict(extensions.most_common()), "top_levels": dict(sorted(tops.items())),
        "categories": dict(categories), "files": files,
    }


def compare(left, right, roots, versions, cache):
    a = versions[left]["inventory"]["files"]
    b = versions[right]["inventory"]["files"]
    # Exact relative spelling is intentional. No fuzzy/case-insensitive matching.
    common = sorted(a.keys() & b.keys())
    equal, changed, untested = [], [], []
    for name in common:
        if a[name]["bytes"] != b[name]["bytes"]:
            changed.append({"path": name, "reason": "size"})
        elif a[name]["category"] != "content_candidate":
            untested.append(name)
        else:
            digests = []
            for label in (left, right):
                key = (label, name)
                if key not in cache:
                    cache[key] = sha256(roots[label] / name)
                digests.append(cache[key])
                versions[label]["inventory"]["files"][name]["sha256"] = cache[key]
            if digests[0] == digests[1]:
                equal.append(name)
            else:
                changed.append({"path": name, "reason": "sha256"})
    return {
        "left": left, "right": right, "common_path_count": len(common),
        "only_left": sorted(a.keys() - b.keys()), "only_right": sorted(b.keys() - a.keys()),
        "content_equal_sha256": equal, "different": changed,
        "same_size_excluded_from_hash": untested,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", nargs=3, metavar=("NAME", "ROOT", "IMAGE"),
                        action="append", required=True, help="Repeat for each version; IMAGE relative to ROOT")
    parser.add_argument("--output", type=Path, required=True, help="Local JSON report outside repository")
    args = parser.parse_args()
    if len(args.version) < 2:
        parser.error("At least two versions are required")
    roots, versions = {}, {}
    try:
        output = args.output.resolve()
        repository = Path(__file__).resolve().parents[1]
        if (repository / ".git").exists() and output.is_relative_to(repository):
            raise ValueError("Detailed report must be outside the Git repository")
        for _, directory, _ in args.version:
            if output.is_relative_to(Path(directory).resolve()):
                raise ValueError("Report must be outside the scanned input directories")
        for label, directory, image_name in args.version:
            if label in roots:
                raise ValueError(f"Duplicate label: {label}")
            root = Path(directory).resolve(strict=True)
            if not root.is_dir():
                raise ValueError(f"Not a directory: {root}")
            image = (root / image_name).resolve(strict=True)
            if Path(image_name).is_absolute() or not image.is_relative_to(root):
                raise ValueError("IMAGE must be a relative path inside ROOT")
            roots[label] = root
            print(f"Inspecting {label}...", flush=True)
            versions[label] = {"image_path": image.relative_to(root).as_posix(),
                               "image": inspect_image(image), "inventory": inventory(root)}
        cache = {}
        comparisons = [compare(a, b, roots, versions, cache) for a, b in combinations(roots, 2)]
        report = {
            "schema_version": 1,
            "method": "Exact relative paths; SHA256 for same-size content candidates only; other categories excluded from content hashing.",
            "limits": "Directory totals include tools/analysis. No archive contents, function matching, PDB identity or runtime compatibility verified.",
            "versions": versions, "comparisons": comparisons,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError) as error:
        parser.exit(1, f"Comparison failed: {error}\n")
    for pair in comparisons:
        print(f"{pair['left']} / {pair['right']}: {pair['common_path_count']} shared paths, "
              f"{len(pair['content_equal_sha256'])} equal content candidates, "
              f"{len(pair['different'])} different files")


if __name__ == "__main__":
    main()
