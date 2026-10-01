"""Live, read-only project dashboard. Python 3.11+, standard library only."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import socket
import subprocess
import sys
from urllib.error import URLError
from urllib.parse import urlsplit
from urllib.request import ProxyHandler, build_opener
import webbrowser


REPO = Path(__file__).resolve().parents[1]
APP = "sylpheed-progress-v1"
STATUSES = {"done", "in_progress", "pending", "blocked"}
STATIC = {"/": ("index.html", "text/html; charset=utf-8"),
          "/app.js": ("app.js", "text/javascript; charset=utf-8"),
          "/style.css": ("style.css", "text/css; charset=utf-8")}


def timestamp(path):
    return datetime.fromtimestamp(path.stat().st_mtime, timezone.utc).isoformat()


def bounded_path(base, relative):
    if not isinstance(relative, str) or not relative or Path(relative).is_absolute():
        raise ValueError("Evidence path must be relative")
    candidate = (base / relative).resolve()
    if not candidate.is_relative_to(base.resolve()):
        raise ValueError("Evidence path leaves its declared folder")
    return candidate


def load_state(path, repo, workspace):
    raw = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(raw, dict) or raw.get("schema_version") != 1:
        raise ValueError("Unsupported milestone schema")
    for field in ("project", "headline", "scope", "updated_at", "current_task"):
        if not isinstance(raw.get(field), str):
            raise ValueError(f"Missing text field: {field}")
    if not isinstance(raw.get("stages"), list) or not raw["stages"]:
        raise ValueError("No milestone stages")
    if not isinstance(raw.get("versions"), list):
        raise ValueError("No version roles")
    for version in raw["versions"]:
        if not isinstance(version, dict) or any(not isinstance(version.get(k), str)
                                               for k in ("name", "role", "note")):
            raise ValueError("Invalid version role")
    bases = {"repo": repo, "workspace": workspace}
    tasks, stage_ids = {}, set()
    for stage in raw["stages"]:
        if not isinstance(stage, dict) or not isinstance(stage.get("id"), str):
            raise ValueError("Invalid stage")
        if stage["id"] in stage_ids:
            raise ValueError("Duplicate stage ID")
        stage_ids.add(stage["id"])
        if any(not isinstance(stage.get(k), str) for k in ("title", "description")):
            raise ValueError("Missing stage text")
        if not isinstance(stage.get("tasks"), list) or not stage["tasks"]:
            raise ValueError("Empty stage")
        for task in stage["tasks"]:
            if not isinstance(task, dict) or not isinstance(task.get("id"), str):
                raise ValueError("Invalid task")
            if task["id"] in tasks:
                raise ValueError("Duplicate task ID")
            if task.get("status") not in STATUSES:
                raise ValueError(f"Invalid task status: {task['id']}")
            if any(not isinstance(task.get(k), str) for k in ("title", "note")):
                raise ValueError("Missing task text")
            evidence = task.get("evidence")
            if not isinstance(evidence, list):
                raise ValueError("Task evidence must be a list")
            for item in evidence:
                if not isinstance(item, dict) or item.get("base") not in bases:
                    raise ValueError("Invalid evidence base")
                target = bounded_path(bases[item["base"]], item.get("path"))
                item["present"] = target.is_file()
                item["modified_at"] = timestamp(target) if item["present"] else None
            recorded = task["status"]
            task["recorded_status"] = recorded
            # Existence verifies that the recorded evidence is available, not
            # the semantic correctness of a build or run. Those need review.
            if recorded == "done" and (not isinstance(task.get("verified_at"), str)
                                       or not task["verified_at"].strip() or not evidence
                                       or not all(e["present"] for e in evidence)):
                task["status"] = "unverified"
            task["stage_id"] = stage["id"]
            tasks[task["id"]] = task
        stage["completed"] = sum(t["status"] == "done" for t in stage["tasks"])
        stage["total"] = len(stage["tasks"])
        stage["percent"] = round(100 * stage["completed"] / stage["total"], 1)
    current = raw["current_task"]
    if current not in tasks:
        raise ValueError("Current task is not in milestones")
    upcoming = raw.get("next_tasks")
    if not isinstance(upcoming, list) or any(not isinstance(t, str) or t not in tasks for t in upcoming):
        raise ValueError("Next tasks must reference milestone IDs")
    completed = sum(t["status"] == "done" for t in tasks.values())
    raw["summary"] = {"completed": completed, "total": len(tasks),
                      "percent": round(100 * completed / len(tasks), 1),
                      "unverified": sum(t["status"] == "unverified" for t in tasks.values())}
    raw["current"] = tasks[current]
    raw["upcoming"] = [tasks[t] for t in upcoming if tasks[t]["status"] != "done"]
    raw["source"] = "progress/milestones.json"
    raw["modified_at"] = timestamp(path)
    return raw


def git_command(repo, *args):
    result = subprocess.run(
        ["git", "-c", "core.quotepath=false", "-C", str(repo), *args],
        capture_output=True, encoding="utf-8", errors="replace", timeout=8,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    if result.returncode:
        raise ValueError(result.stderr.strip()[:300] or "Git command failed")
    return result.stdout.rstrip("\r\n")


def git_state(repo):
    commands = {
        "branch": ("symbolic-ref", "--short", "HEAD"),
        "head": ("rev-parse", "HEAD"),
        "changes": ("status", "--porcelain=v1", "--untracked-files=normal"),
        "log": ("log", "-20", "--format=%H%x00%h%x00%aI%x00%s%x00%b%x1e"),
        "count": ("rev-list", "--count", "HEAD"),
        "tracking": ("rev-list", "--left-right", "--count", "HEAD...@{upstream}"),
    }
    values, errors = {}, {}
    with ThreadPoolExecutor(max_workers=len(commands)) as pool:
        futures = {k: pool.submit(git_command, repo, *args) for k, args in commands.items()}
        for key, future in futures.items():
            try:
                values[key] = future.result()
            except (OSError, ValueError, subprocess.TimeoutExpired) as error:
                errors[key] = str(error)
    commits = []
    for record in values.get("log", "").split("\x1e"):
        fields = record.strip().split("\0", 4)
        if len(fields) == 5:
            commits.append(dict(zip(("hash", "short", "date", "subject", "body"), fields)))
    tracking = values.get("tracking", "").split()
    return {
        "available": "head" in values,
        "branch": values.get("branch", "detached" if "head" in values else "unknown"),
        "head": values.get("head", ""), "commits": commits,
        "commit_count": int(values["count"]) if "count" in values else None,
        "changes": values.get("changes", "").splitlines(),
        "ahead": int(tracking[0]) if len(tracking) == 2 else None,
        "behind": int(tracking[1]) if len(tracking) == 2 else None,
        "tracking_note": "与本机 upstream 引用比较，不自动联网确认 GitHub。",
        "errors": errors,
    }


def read_note(path, limit=16000):
    with path.open("r", encoding="utf-8-sig", errors="replace") as stream:
        text = stream.read(limit + 1)
    return {"name": path.name, "text": text[:limit], "truncated": len(text) > limit,
            "modified_at": timestamp(path)}


def local_notes(workspace):
    result = {"plans": [], "handoff": None, "errors": []}
    try:
        plans_dir = bounded_path(workspace, "plans")
        candidates = sorted(plans_dir.glob("*.md"), key=lambda p: (p.stat().st_mtime_ns, p.name), reverse=True)
        for candidate in candidates[:8]:
            if not candidate.resolve().is_relative_to(workspace.resolve()):
                raise ValueError("Plan leaves workspace")
            result["plans"].append(read_note(candidate))
        handoff = bounded_path(workspace, "HANDOFF.md")
        if handoff.is_file():
            # Last handoff sections carry the newest status.
            text = handoff.read_text(encoding="utf-8-sig", errors="replace")
            result["handoff"] = {"name": handoff.name, "text": text[-16000:],
                                 "truncated": len(text) > 16000, "modified_at": timestamp(handoff)}
    except (OSError, ValueError) as error:
        result["errors"].append(str(error))
    return result


def snapshot(repo, workspace):
    errors = []
    try:
        state = load_state(repo / "progress" / "milestones.json", repo, workspace)
    except (OSError, ValueError, TypeError) as error:
        state = None
        errors.append(f"progress/milestones.json: {error}")
    return {"app": APP, "observed_at": datetime.now(timezone.utc).isoformat(),
            "state": state, "git": git_state(repo), "notes": local_notes(workspace), "errors": errors}


class Dashboard(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = False

    def server_bind(self):
        # Windows SO_REUSEADDR can bind twice to one live listener. Exclusive
        # binding makes repeated BAT launches reuse the existing app reliably.
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()

    def __init__(self, address, repo, workspace):
        super().__init__(address, Handler)
        self.repo, self.workspace = repo, workspace
        self.identity = {"app": APP, "project_key": project_key(repo, workspace)}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        # Only local hostnames, exact routes, and same-origin read requests.
        host = urlsplit("http://" + self.headers.get("Host", "")).hostname
        if host not in ("127.0.0.1", "localhost") or self.headers.get("Sec-Fetch-Site") == "cross-site":
            self.send_error(403)
            return
        route = urlsplit(self.path).path
        try:
            if route == "/api/identity":
                body, kind = json.dumps(self.server.identity).encode(), "application/json"
            elif route == "/api/status":
                body = json.dumps(snapshot(self.server.repo, self.server.workspace), ensure_ascii=False).encode("utf-8")
                kind = "application/json; charset=utf-8"
            elif route in STATIC:
                name, kind = STATIC[route]
                body = (self.server.repo / "scripts" / "progress_web" / name).read_bytes()
            else:
                self.send_error(404)
                return
        except OSError:
            self.send_error(500, "Dashboard source unavailable")
            return
        self.send_response(200)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self'; object-src 'none'; frame-ancestors 'none'")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass


def project_key(repo, workspace):
    return hashlib.sha256(f"{repo.resolve()}\0{workspace.resolve()}".encode()).hexdigest()


def select_server(repo, workspace, port):
    opener = build_opener(ProxyHandler({}))
    for candidate in range(port, min(port + 20, 65536)):
        try:
            server = Dashboard(("127.0.0.1", candidate), repo, workspace)
            return server, f"http://127.0.0.1:{server.server_port}/"
        except OSError as error:
            if error.errno not in (48, 98, 10048, 10013) and getattr(error, "winerror", None) not in (10048, 10013):
                raise
            try:
                with opener.open(f"http://127.0.0.1:{candidate}/api/identity", timeout=0.5) as response:
                    identity = json.loads(response.read(4096))
                if identity == {"app": APP, "project_key": project_key(repo, workspace)}:
                    return None, f"http://127.0.0.1:{candidate}/"
            except (OSError, URLError, ValueError, socket.timeout):
                pass
    raise OSError("No available dashboard port in requested range")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=REPO)
    parser.add_argument("--workspace", type=Path, help="Defaults to repository parent")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--open", action="store_true", help="Open system browser")
    parser.add_argument("--snapshot", action="store_true", help="Print live JSON without starting server")
    args = parser.parse_args()
    if not 0 <= args.port <= 65535:
        parser.error("Port must be 0..65535 (0 selects a free port)")
    repo = args.repo.resolve()
    workspace = (args.workspace or repo.parent).resolve()
    if not repo.is_dir() or not workspace.is_dir():
        parser.error("Repository/workspace directory does not exist")
    if args.snapshot:
        print(json.dumps(snapshot(repo, workspace), ensure_ascii=False, indent=2))
        return
    try:
        server, url = select_server(repo, workspace, args.port)
    except OSError as error:
        parser.exit(1, f"Dashboard start failed: {error}\n")
    print(f"Dashboard: {url}", flush=True)
    if args.open:
        if not webbrowser.open(url):
            print("Browser could not be opened automatically. Open the URL above.", flush=True)
    if server is None:
        print("Existing dashboard reused; no second server started.", flush=True)
        return
    print("Reads files and Git on every refresh. Ctrl+C closes this server.", flush=True)
    try:
        server.serve_forever(poll_interval=0.3)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    if sys.version_info < (3, 11):
        raise SystemExit("Python 3.11 or newer is required.")
    main()
