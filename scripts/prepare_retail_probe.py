"""Create an isolated ReXGlue 0.10.0 retail probe, without changing the main host."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

REPO = Path(__file__).resolve().parents[1]
RETAIL_SHA256 = "8935855e0fbd2461c0aba73163dd023e110d099f01816d6286d3a52211bc4286"


def prepare(rexglue, image, output):
    image = image.resolve(strict=True)
    output = output.resolve()
    if output.is_relative_to(REPO) or output.is_relative_to(image.parent):
        raise ValueError("Keep the probe outside the repository and game resource directory")
    if output.exists():
        raise ValueError("Probe directory already exists; choose a new directory")
    with image.open("rb") as source:
        actual = hashlib.file_digest(source, "sha256").hexdigest()
    if actual != RETAIL_SHA256:
        raise ValueError(f"Retail image mismatch: {actual}; address configs apply only to {RETAIL_SHA256}")
    version = subprocess.run([str(rexglue), "--version"], check=True, capture_output=True, text=True).stdout
    if version.strip() != "0.10.0":
        raise ValueError(f"Expected ReXGlue v0.10.0, received {version.strip()}")
    subprocess.run([str(rexglue), "init", "--project-name", "sylpheed_retail_probe",
                    "--xex-path", str(image), "--game-root", str(image.parent),
                    "--project-root", str(output)], check=True)
    config = output / "config"
    config.mkdir(exist_ok=True)
    for name in ("verified-entry-seeds.toml", "terminal-rethrow-boundaries.toml"):
        shutil.copyfile(REPO / "config/retail" / name, config / name)
    # JSON string escaping is compatible with TOML basic strings for paths.
    manifest = '\n'.join([
        '[project]', 'name = "sylpheed_retail_probe"', 'sdk_version = "0.10.0"',
        f'game_root = {json.dumps(image.parent.as_posix())}', '', '[entrypoint]',
        f'file_path = {json.dumps(image.as_posix())}', 'out_directory_path = "generated/default"',
        'includes = ["config/verified-entry-seeds.toml", "config/terminal-rethrow-boundaries.toml"]', '',
    ])
    (output / "sylpheed_retail_probe_manifest.toml").write_text(manifest, encoding="utf-8")
    cmake_path = output / "CMakeLists.txt"
    cmake = cmake_path.read_text(encoding="utf-8")
    call = "rexglue_setup_target(sylpheed_retail_probe)"
    if cmake.count(call) != 1:
        raise ValueError("Unexpected SDK init template; probe preserved for inspection")
    cmake = cmake.replace(call, "rexglue_setup_target(sylpheed_retail_probe GPU_PLUGINS xenos)")
    cmake += '\ntarget_sources(sylpheed_retail_probe PRIVATE src/round_even.cpp)\n'
    cmake_path.write_text(cmake, encoding="utf-8")
    shutil.copyfile(REPO / "src/platform/windows/round_even.cpp", output / "src/round_even.cpp")
    (output / "input-identity.json").write_text(json.dumps({
        "image": str(image), "sha256": actual, "sdk": version.strip(),
        "status": "prepared only; codegen, configure, build and run remain separate steps",
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {output}; no game execution or development-host changes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rexglue", type=Path, required=True)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        prepare(args.rexglue.resolve(strict=True), args.image, args.output_dir)
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Probe preparation failed: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
