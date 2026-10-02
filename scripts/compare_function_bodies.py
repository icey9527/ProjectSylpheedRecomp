"""Compare PDATA-delimited Xbox PE function bodies from SDK-loaded image dumps.

Exact and relocation-normalized unique matches are correspondence candidates,
not a measure of game feature parity. Python 3.11+, standard library only.
"""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import struct


def load_image(path):
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise ValueError("Expected an SDK-loaded PE image dump, not the original XEX")
    pe = struct.unpack_from("<I", data, 60)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Invalid PE signature")
    sections_count = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x10B:
        raise ValueError("Expected Xbox PE32 optional header")
    base = struct.unpack_from("<I", data, opt + 28)[0]
    rva, size = struct.unpack_from("<II", data, opt + 120)
    if size % 8 or rva + size > len(data):
        raise ValueError("Invalid exception directory")
    sections = []
    for i in range(sections_count):
        pos = opt + opt_size + 40 * i
        name = data[pos:pos + 8].rstrip(b"\0").decode("ascii")
        length, start = struct.unpack_from("<II", data, pos + 8)
        flags = struct.unpack_from("<I", data, pos + 36)[0]
        if start + length > len(data):
            # XEX loader dumps may omit the PE relocation tail. No function
            # may use it; the SDK BinaryView likewise skips unmapped sections.
            if name == ".reloc" and not flags & 0x20000000:
                continue
            raise ValueError("Section exceeds loaded image")
        sections.append((name, base + start, base + start + length, bool(flags & 0x20000000)))
    functions = {}
    for addr, bits in struct.iter_unpack(">II", data[rva:rva + size]):
        length = max(4, ((bits >> 8) & 0x3FFFFF) * 4)
        if not any(executable and low <= addr and addr + length <= high
                   for _, low, high, executable in sections):
            raise ValueError(f"PDATA function outside executable sections: {addr:#x}")
        functions[addr] = data[addr - base:addr - base + length]
    if not functions:
        raise ValueError("No PDATA functions")
    return dict(data=data, base=base, sections=sections, functions=functions)


def normalized(body, start, image):
    words = list(struct.unpack(">" + "I" * (len(body) // 4), body))
    tokens = [(w,) for w in words]
    for i, word in enumerate(words):
        opcode = word >> 26
        if opcode in (18, 16) and not word & 2:
            mask, sign = (0x3FFFFFC, 0x2000000) if opcode == 18 else (0xFFFC, 0x8000)
            delta = word & mask
            if delta & sign:
                delta -= sign * 2
            target = start + i * 4 + delta
            # Keep intra-function control flow. External branch destinations
            # change on relink; retain the opcode/condition/link bits.
            if not start <= target < start + len(body):
                tokens[i] = (word & ~mask, "external_branch")
        if opcode == 15 and ((word >> 16) & 31) == 0:
            register = (word >> 21) & 31
            high = (word & 0xFFFF) << 16
            for j in range(i + 1, min(i + 5, len(words))):
                low_word = words[j]
                low_op = low_word >> 26
                if low_op not in (14, 32, 34, 36, 38, 48, 50, 52, 54):
                    continue
                if (low_word >> 16) & 31 != register:
                    continue
                low = low_word & 0xFFFF
                if low & 0x8000:
                    low -= 0x10000
                target = (high + low) & 0xFFFFFFFF
                section = next((s for s in image["sections"] if s[1] <= target < s[2]), None)
                if section:
                    tokens[i] = (word & 0xFFFF0000, "address_high", section[0])
                    tokens[j] = (low_word & 0xFFFF0000, "address_low", section[0])
    return json.dumps(tokens, separators=(",", ":")).encode()


def compare(dev, retail, names=None):
    exact = collections.defaultdict(list)
    shape = collections.defaultdict(list)
    for addr, body in retail["functions"].items():
        if len(body) >= 64:
            exact[hashlib.sha256(body).digest()].append(addr)
            shape[hashlib.sha256(normalized(body, addr, retail)).digest()].append(addr)
    rows, counts = [], collections.Counter()
    dev_hashes = collections.Counter(hashlib.sha256(body).digest()
                                    for body in dev["functions"].values() if len(body) >= 64)
    dev_shapes = collections.Counter(hashlib.sha256(normalized(body, addr, dev)).digest()
                                    for addr, body in dev["functions"].items() if len(body) >= 64)
    for addr, body in dev["functions"].items():
        if len(body) < 64:
            counts["excluded_under_64_bytes"] += 1
            continue
        key = hashlib.sha256(body).digest()
        skey = hashlib.sha256(normalized(body, addr, dev)).digest()
        if len(exact[key]) == 1 and dev_hashes[key] == 1:
            mode, other = "unique_exact", exact[key][0]
        elif len(shape[skey]) == 1 and dev_shapes[skey] == 1:
            mode, other = "unique_normalized_candidate", shape[skey][0]
        else:
            counts["unmatched_or_ambiguous"] += 1
            continue
        counts[mode] += 1
        rows.append(dict(development_address=hex(addr), retail_address=hex(other),
                         bytes=len(body), match=mode, **(names or {}).get(addr, {})))
    return dict(scope="PDATA function bodies >=64 bytes; exact unique matches and heuristic relocation-normalized candidates",
                limitations="Not all functions have PDATA. Optimization, inlining and changed boundaries affect matching. Candidates require call/data/behavior validation; unmatched does not mean added/removed features.",
                development_pdata_functions=len(dev["functions"]),
                retail_pdata_functions=len(retail["functions"]),
                development_text_bytes=sum(hi-lo for name,lo,hi,_ in dev['sections'] if name=='.text'),
                retail_text_bytes=sum(hi-lo for name,lo,hi,_ in retail['sections'] if name=='.text'),
                counts=dict(counts), matches=rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--development", type=Path, required=True)
    parser.add_argument("--retail", type=Path, required=True)
    parser.add_argument("--symbols", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        inputs = [args.development.resolve(), args.retail.resolve()]
        out = args.output.resolve()
        repo = Path(__file__).resolve().parents[1]
        if out in inputs or out.is_relative_to(repo):
            raise ValueError("Keep full address/name reports outside the repository and do not overwrite inputs")
        names = {}
        if args.symbols:
            with args.symbols.open(encoding="utf-8-sig", newline="") as source:
                for row in csv.DictReader(source):
                    names.setdefault(int(row['address'], 16), dict(name=row['readable_map_name'], object=row['object']))
        result = compare(load_image(inputs[0]), load_image(inputs[1]), names)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({k:v for k,v in result.items() if k!='matches'}, ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError, struct.error, KeyError) as error:
        print(f"Comparison failed: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
