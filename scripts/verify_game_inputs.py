"""Verify recorded development inputs without modifying them; Python 3.11+."""

import argparse
import hashlib
import json
from pathlib import Path
import sys


REPO = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, default=REPO / "assets")
    parser.add_argument("--image-only", action="store_true", help="Do not require MAP/PDB")
    args = parser.parse_args()
    try:
        manifest = json.loads((REPO / "config/game-inputs.json").read_text(encoding="utf-8"))
        files = manifest["files"]
        if args.image_only:
            files = [entry for entry in files if entry["role"] == "image"]
        failed = False
        for entry in files:
            name = entry["name"]
            path = args.input_dir / name
            try:
                with path.open("rb") as stream:
                    digest = hashlib.file_digest(stream, "sha256").hexdigest()
                if path.stat().st_size != entry["size"] or digest != entry["sha256"]:
                    print(f"MISMATCH {name}: differs from the recorded development input", file=sys.stderr)
                    failed = True
                else:
                    print(f"OK {name}")
            except OSError as error:
                print(f"UNAVAILABLE {name}: {error.strerror}", file=sys.stderr)
                failed = True
        if failed:
            return 1
        print("Input hashes match. This does not authenticate the image/PDB CodeView pairing.")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Cannot read input manifest: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
