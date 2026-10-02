"""Run the matching development image with isolated user data and a time limit.

Python 3.11+, standard library only. Local paths are arguments or read from
../assets/runtime.local.json; local configuration and run evidence stay outside Git.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time


REPO = Path(__file__).resolve().parents[1]
DEVELOPMENT_SHA256 = "a8fd9273e37a3930f3d1d4e8614f3b487f583d7d3335347b47fbf67f80972699"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-data-root", type=Path)
    parser.add_argument("--seconds", type=int, default=45)
    parser.add_argument("--config", type=Path, default=REPO.parent / "assets/runtime.local.json")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 600:
        parser.error("Time limit must be 1..600 seconds")
    try:
        root = args.game_data_root
        if root is None:
            local = json.loads(args.config.read_text(encoding="utf-8-sig"))
            root = Path(local["game_data_root"])
        root = root.resolve(strict=True)
        binary = REPO / "out/build/win-amd64-debug/project_sylpheed.exe"
        if not binary.is_file():
            raise ValueError("Debug host is missing; run scripts/Build.ps1 first")
        image = binary.parent / "BaseLib.dll"
        with image.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        if digest != DEVELOPMENT_SHA256:
            raise ValueError("Image SHA256 does not match this development host; refusing to run")
        if not (root / "config.ini").is_file() or not (root / "dat").is_dir():
            raise ValueError("Expected config.ini and dat/ in the selected resource root")
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"Preflight failed: {error}\n")

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    run_dir = REPO.parent / "logs" / f"run-development-{stamp}"
    run_dir.mkdir(parents=True)
    user_dir = REPO.parent / "logs/runtime-user-data/development"
    user_dir.mkdir(parents=True, exist_ok=True)
    log_file = run_dir / "runtime.log"
    command = [str(binary), f"--game_data_root={root}", f"--user_data_root={user_dir}",
               "--gpu_plugin=xenos", f"--log_file={log_file}", "--log_level=debug",
               "--log_flush_interval=1", "--allow_game_relative_writes=false"]
    record = {"started_at": datetime.now(timezone.utc).isoformat(), "image_sha256": digest,
              "game_data_root": str(root), "command": command, "time_limit_seconds": args.seconds}
    print(f"Run evidence: {run_dir}", flush=True)
    started = time.monotonic()
    process = None
    try:
        with (run_dir / "stdout.txt").open("wb") as out, (run_dir / "stderr.txt").open("wb") as err:
            process = subprocess.Popen(command, cwd=binary.parent, stdout=out, stderr=err)
            record["pid"] = process.pid
            while True:
                remaining = args.seconds - (time.monotonic() - started)
                if remaining <= 0:
                    record["outcome"] = "time_limit"
                    process.terminate()
                    process.wait(timeout=10)
                    break
                try:
                    process.wait(timeout=min(1, remaining))
                    record["outcome"] = "exited"
                    break
                except subprocess.TimeoutExpired:
                    if log_file.exists():
                        with log_file.open("rb") as stream:
                            stream.seek(max(0, log_file.stat().st_size - 65536))
                            tail = stream.read().decode("utf-8", errors="replace")
                        fatal = next((line for line in tail.splitlines() if "[FATAL]" in line), None)
                        if fatal:
                            record.update(outcome="runtime_fatal", fatal_line=fatal)
                            process.terminate()
                            process.wait(timeout=10)
                            break
    except KeyboardInterrupt:
        record["outcome"] = "interrupted"
    except OSError as error:
        record.update(outcome="launch_failed", error=str(error))
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.wait()
        record["exit_code"] = process.returncode if process else None
        record["elapsed_seconds"] = round(time.monotonic() - started, 2)
        record["finished_at"] = datetime.now(timezone.utc).isoformat()
        text = "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in sorted(run_dir.glob("runtime*.log")))
        record["observed_stages"] = list(dict.fromkeys(re.findall(r"SYLPHEED_STAGE ([a-z_]+)", text)))
        record["run_directory"] = str(run_dir)
        (run_dir / "result.json").write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        (REPO.parent / "logs/development-last-run.json").write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Outcome: {record['outcome']}; process exit: {record['exit_code']}; "
          f"elapsed: {record['elapsed_seconds']}s", flush=True)
    print("Read runtime.log for load/guest stages. A living process or exit 0 is not proof of gameplay.")
    return 0 if record["outcome"] == "exited" and record["exit_code"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
