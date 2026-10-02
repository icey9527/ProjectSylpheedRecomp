"""Local MAP/PDB public-symbol index; Python 3.11+, standard library only.

Reads MSF 7.0 identity/DBI, S_PUB32 and module procedure/label records.
This is not a complete PDB/type/local-variable parser or an XEX identity verifier.
"""

import argparse
import collections
import ctypes
import csv
import hashlib
import json
from pathlib import Path
import re
import struct
import uuid

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INDEX = ROOT.parent / "symbols" / "functions.csv"


class Msf:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        if self.data[:32] != b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0":
            raise ValueError("Only MSF 7.0 PDB files are supported")
        self.block_size, _, block_count, directory_size, _, block_map = struct.unpack_from(
            "<6I", self.data, 32
        )
        if self.block_size not in (512, 1024, 2048, 4096) or block_count * self.block_size != len(self.data):
            raise ValueError("Invalid MSF block geometry")
        count = (directory_size + self.block_size - 1) // self.block_size
        if count * 4 > self.block_size:
            raise ValueError("Multi-block directory maps are not supported")
        blocks = struct.unpack_from(f"<{count}I", self.data, block_map * self.block_size)
        directory = self.blocks(blocks)[:directory_size]
        stream_count = struct.unpack_from("<I", directory)[0]
        sizes = struct.unpack_from(f"<{stream_count}I", directory, 4)
        cursor = 4 + 4 * stream_count
        self.streams = []
        for size in sizes:
            if size == 0xFFFFFFFF:
                self.streams.append(None)
                continue
            n = (size + self.block_size - 1) // self.block_size
            pages = struct.unpack_from(f"<{n}I", directory, cursor)
            cursor += n * 4
            self.streams.append((size, pages))

    def blocks(self, pages):
        if any(page * self.block_size >= len(self.data) for page in pages):
            raise ValueError("MSF block out of bounds")
        return b"".join(self.data[p * self.block_size:(p + 1) * self.block_size] for p in pages)

    def stream(self, number):
        entry = self.streams[number]
        if entry is None:
            raise ValueError(f"Missing PDB stream {number}")
        size, pages = entry
        return self.blocks(pages)[:size]


def read_pdb(path):
    msf = Msf(path)
    info, dbi = msf.stream(1), msf.stream(3)
    version, signature, age = struct.unpack_from("<3I", info)
    dbi_age = struct.unpack_from("<I", dbi, 8)[0]
    symbol_stream = struct.unpack_from("<H", dbi, 20)[0]
    machine = struct.unpack_from("<H", dbi, 58)[0]
    metadata = dict(version=version, signature=f"0x{signature:08X}", age=age,
                    guid=str(uuid.UUID(bytes_le=info[12:28])), dbi_age=dbi_age,
                    machine=f"0x{machine:04X}", symbol_stream=symbol_stream)
    records = msf.stream(symbol_stream)
    symbols = collections.defaultdict(set)
    types = collections.Counter()
    cursor = 0
    while cursor < len(records):
        length, kind = struct.unpack_from("<HH", records, cursor)
        end = cursor + 2 + length
        if length < 2 or end > len(records):
            raise ValueError(f"Invalid CodeView symbol record at {cursor}")
        types[f"0x{kind:04X}"] += 1
        if kind == 0x110E:  # S_PUB32: flags, offset, segment, zero-terminated name
            if length < 13:
                raise ValueError("Truncated S_PUB32")
            flags, offset, segment = struct.unpack_from("<IIH", records, cursor + 4)
            name_bytes = records[cursor + 14:end].split(b"\0", 1)[0]
            name = name_bytes.decode("utf-8", errors="backslashreplace")
            symbols[(segment, offset)].add(name)
        # Exact lengths include CodeView padding bytes.
        cursor = end
    metadata["record_types"] = dict(types)
    metadata["public_symbol_count"] = sum(map(len, symbols.values()))
    private = collections.defaultdict(set)
    modules = 0
    cursor, module_end = 64, 64 + struct.unpack_from("<i", dbi, 24)[0]
    while cursor < module_end:
        stream, symbol_bytes = struct.unpack_from("<HI", dbi, cursor + 34)
        name_start = cursor + 64
        name_end = dbi.index(0, name_start)
        object_end = dbi.index(0, name_end + 1)
        module = dbi[name_start:name_end].decode("utf-8", errors="backslashreplace")
        cursor = (object_end + 4) & ~3
        modules += 1
        if stream == 0xFFFF or symbol_bytes < 4:
            continue
        records = msf.stream(stream)[:symbol_bytes]
        if struct.unpack_from("<I", records)[0] != 4:
            raise ValueError("Unsupported module CodeView signature")
        position = 4
        while position < len(records):
            length, kind = struct.unpack_from("<HH", records, position)
            end = position + 2 + length
            if length < 2 or end > len(records):
                raise ValueError(f"Invalid module symbol record in stream {stream}")
            body = position + 4
            if kind in (0x110F, 0x1110):  # S_LPROC32 / S_GPROC32
                offset, segment = struct.unpack_from("<IH", records, body + 28)
                name_start = body + 35
                category = "procedure"
            elif kind == 0x1105:  # S_LABEL32 (including compiler-generated labels)
                offset, segment = struct.unpack_from("<IH", records, body)
                name_start = body + 7
                category = "label"
            else:
                position = end
                continue
            name = records[name_start:end].split(b"\0", 1)[0].decode("utf-8", errors="backslashreplace")
            private[(segment, offset)].add((name, module, category))
            position = end
    metadata["module_count"] = modules
    metadata["module_procedure_label_count"] = sum(map(len, private.values()))
    return metadata, symbols, private


def demangle(name, flags=0):
    if not name.startswith("?"):
        return name
    try:
        function = ctypes.windll.dbghelp.UnDecorateSymbolNameW
        function.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint, ctypes.c_uint]
        function.restype = ctypes.c_uint
        buffer = ctypes.create_unicode_buffer(4096)
        if function(name, buffer, len(buffer), flags):
            return buffer.value
    except (AttributeError, OSError):
        pass
    return name


def read_map(path):
    text = Path(path).read_text(encoding="utf-8", errors="backslashreplace")
    stamp = re.search(r"Timestamp is ([0-9a-fA-F]+)", text)
    pattern = re.compile(r"^\s+([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{8})\s+f\s+(.*)$")
    rows = []
    for line in text.splitlines():
        match = pattern.match(line)
        if not match:
            continue
        segment, offset, name, address, origin = match.groups()
        # 'i' means inline/public annotation in this MAP, not part of object name.
        origin = re.sub(r"^i\s+", "", origin.strip())
        group = origin.split(":", 1)[0] if ":" in origin else "game_objects"
        rows.append(dict(address=f"0x{int(address, 16):08X}",
                         segment=int(segment, 16), offset=int(offset, 16),
                         map_name=name, object=origin, group=group))
    if not rows:
        raise ValueError("No MAP function symbols found")
    return (f"0x{int(stamp[1], 16):08X}" if stamp else None), rows


def generated_registrations(directory):
    """The generated registrar is the address authority, including renamed functions."""
    registrations = {}
    pattern = re.compile(r"registrar->SetFunction\(\s*(0x[0-9A-Fa-f]+)\s*,\s*(\w+)\s*\)")
    for file in sorted(Path(directory).glob("*_register.cpp")):
        for address, name in pattern.findall(file.read_text(encoding="utf-8")):
            key = f"0x{int(address, 16):08X}"
            if key in registrations and registrations[key] != name:
                raise ValueError(f"Conflicting generated registration for {key}")
            registrations[key] = name
    return registrations


def generated_functions(directory):
    registrations = generated_registrations(directory)
    definitions = {}
    pattern = re.compile(r"DEFINE_REX_FUNC\((\w+)\)")
    for file in sorted(Path(directory).glob("*_recomp*.cpp")):
        for line_number, line in enumerate(file.read_text(encoding="utf-8").splitlines(), 1):
            match = pattern.search(line)
            if match:
                name = match[1]
                if name in definitions:
                    raise ValueError(f"Duplicate generated definition: {name}")
                definitions[name] = (name, file.name, line_number)
    functions = {address: definitions[name] for address, name in registrations.items()
                 if name in definitions}
    # Legacy generated fixtures may omit the registrar. Do not infer addresses
    # from suffixes of readable names: only the old sub_ADDRESS form is explicit.
    for name, location in definitions.items():
        if re.fullmatch(r"sub_[0-9A-Fa-f]{8}", name):
            address = f"0x{int(name[4:], 16):08X}"
            if registrations and registrations.get(address) != name:
                raise ValueError(f"Generated definition/registration mismatch for {address}")
            functions.setdefault(address, location)
    return functions


def build(args):
    metadata, pdb_symbols, private_symbols = read_pdb(args.pdb)
    stamp, rows = read_map(args.map)
    generated = generated_functions(args.generated)
    counts = collections.Counter(row["address"] for row in rows)
    statuses = collections.Counter()
    private_statuses = collections.Counter()
    missing_examples = []
    for row in rows:
        names = pdb_symbols.get((row["segment"], row["offset"]), set())
        status = "exact_name_and_location" if row["map_name"] in names else (
            "location_only" if names else "absent_public_location")
        statuses[status] += 1
        private = sorted(private_symbols.get((row["segment"], row["offset"]), set()))
        private_names = sorted({entry[0] for entry in private})
        private_status = "exact_raw_name_and_location" if row["map_name"] in private_names else (
            "location_only" if private else "absent_module_location")
        private_statuses[private_status] += 1
        row.update(readable_map_name=demangle(row["map_name"]),
                   pdb_status=status, pdb_names=" | ".join(sorted(names)[:3]),
                   pdb_public_alias_count=len(names),
                   pdb_module_status=private_status,
                   pdb_module_names=" | ".join(private_names[:3]),
                   pdb_module_name_count=len(private_names),
                   alias_count=counts[row["address"]])
        symbol, file, line = generated.get(row["address"], ("", "", ""))
        row.update(generated_name=symbol, generated_file=file, generated_line=line)
        if status != "exact_name_and_location" and len(missing_examples) < 12:
            missing_examples.append({k: row[k] for k in ("address", "map_name", "pdb_status")})
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="", encoding="utf-8-sig") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    summary = dict(
        pdb=metadata, map_timestamp=stamp,
        map_timestamp_matches_pdb_signature=stamp == metadata["signature"],
        map_function_rows=len(rows), map_unique_function_addresses=len(counts),
        addresses_with_aliases=sum(count > 1 for count in counts.values()),
        map_pdb_public_comparison=dict(statuses),
        map_pdb_module_comparison=dict(private_statuses),
        map_rows_without_public_or_module_location=sum(
            row["pdb_status"] == "absent_public_location" and
            row["pdb_module_status"] == "absent_module_location" for row in rows),
        generated_sub_function_count=sum(bool(re.fullmatch(r"sub_[0-9A-Fa-f]{8}", value[0]))
                                         for value in generated.values()),
        generated_function_count=len(generated),
        map_unique_addresses_with_generated_entry=len(set(counts) & set(generated)),
        map_unique_addresses_without_generated_entry=len(set(counts) - set(generated)),
        generated_entries_without_map_function=len(set(generated) - set(counts)),
        groups=dict(collections.Counter(row["group"] for row in rows)),
        unmatched_examples=missing_examples,
        input_sha256={str(path.name): hashlib.sha256(path.read_bytes()).hexdigest()
                      for path in (Path(args.map), Path(args.pdb))},
        limits=["PDB public/procedure/label records only; no full local/type enumeration.",
                "Readable names use Windows DbgHelp when available; raw names remain authoritative.",
                "CSV alias previews are limited to three names; query lists MAP aliases separately.",
                "Same location alone does not prove that two different names are identical functions.",
                "A PDB signature timestamp need not equal the final linker timestamp.",
                "Segment:offset equality is checked; no PDB section remapping applied.",
                "XEX CodeView GUID/age has not been verified.",
                "MAP function names are not reliable function lengths or subsystem boundaries.",
                "Generated addresses use registrar SetFunction entries and emitted definitions; "
                "legacy sub_ADDRESS definitions can be read without a registrar."])
    output.with_suffix(".summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2))


def query(args):
    needle = args.term.casefold()
    with Path(args.index).open(encoding="utf-8-sig", newline="") as file:
        rows = [row for row in csv.DictReader(file) if any(
            needle in row[key].casefold() for key in
            ("address", "map_name", "readable_map_name", "pdb_names", "pdb_module_names", "object", "group", "generated_name"))]
    print(f"{len(rows)} matches; showing up to {args.limit}")
    for row in rows[:args.limit]:
        location = f"{row['generated_file']}:{row['generated_line']}" if row['generated_file'] else "no generated entry"
        print(f"{row['address']} {row['readable_map_name']}\n  raw: {row['map_name']}\n  {row['group']} / {row['object']} / {row['pdb_status']}\n  module: {row['pdb_module_status']} {row['pdb_module_names'][:200]}\n  {row['generated_name']} {location}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    build_parser = commands.add_parser("build")
    build_parser.add_argument("--map", type=Path, default=ROOT / "assets/Xacalite_ScriptTeam.map")
    build_parser.add_argument("--pdb", type=Path, default=ROOT / "assets/Xacalite_ScriptTeam.pdb")
    build_parser.add_argument("--generated", type=Path, default=ROOT / "generated/xacalite_scriptteam")
    build_parser.add_argument("--output", type=Path, default=DEFAULT_INDEX)
    build_parser.set_defaults(action=build)
    query_parser = commands.add_parser("query")
    query_parser.add_argument("term")
    query_parser.add_argument("--index", type=Path, default=DEFAULT_INDEX)
    query_parser.add_argument("--limit", type=int, default=20)
    query_parser.set_defaults(action=query)
    args = parser.parse_args()
    args.action(args)


if __name__ == "__main__":
    main()
