"""Dashboard outcomes: live files/Git, honest progress and launcher reuse.

Run: python -m unittest discover -s tests -v
"""

import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import ProxyHandler, Request, build_opener


spec = importlib.util.spec_from_file_location("dashboard", Path(__file__).resolve().parents[1] / "scripts/progress_server.py")
dashboard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dashboard)


class DashboardTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="dashboard test ")
        self.addCleanup(self.temporary.cleanup)
        self.workspace = Path(self.temporary.name)
        self.repo = self.workspace / "repo with spaces"
        (self.repo / "progress").mkdir(parents=True)
        (self.workspace / "logs").mkdir()
        (self.workspace / "logs/verified.txt").write_text("reviewed evidence", encoding="utf-8")
        self.path = self.repo / "progress/milestones.json"
        self.state = {
            "schema_version": 1, "project": "Fixture", "headline": "First load pending",
            "scope": "Acceptance checklist", "updated_at": "2026-10-01",
            "current_task": "load", "next_tasks": ["load"], "versions": [],
            "stages": [{"id": "runtime", "title": "Runtime", "description": "Actual game evidence",
                        "tasks": [
                            {"id": "build", "title": "Build", "status": "done", "note": "Reviewed",
                             "verified_at": "2026-10-01", "evidence": [{"base": "workspace", "path": "logs/verified.txt"}]},
                            {"id": "load", "title": "Game loaded", "status": "in_progress", "note": "No success yet", "evidence": []}
                        ]}]
        }
        self.write()
        self.opener = build_opener(ProxyHandler({}))

    def write(self):
        self.path.write_text(json.dumps(self.state), encoding="utf-8")

    def start(self):
        server, url = dashboard.select_server(self.repo, self.workspace, 0)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()

        def close():
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)
        self.addCleanup(close)
        return server, url

    def get(self, url):
        with self.opener.open(url, timeout=10) as response:
            self.assertEqual(response.headers["Cache-Control"], "no-store")
            return json.loads(response.read())

    def test_progress_changes_when_file_changes(self):
        first = dashboard.load_state(self.path, self.repo, self.workspace)
        self.assertEqual(first["summary"]["percent"], 50)
        task = self.state["stages"][0]["tasks"][1]
        task.update(status="done", verified_at="2026-10-01", evidence=copy.deepcopy(self.state["stages"][0]["tasks"][0]["evidence"]))
        self.write()
        second = dashboard.load_state(self.path, self.repo, self.workspace)
        self.assertEqual(second["summary"]["percent"], 100)
        self.assertEqual(second["upcoming"], [])

    def test_missing_evidence_and_date_never_count_as_done(self):
        (self.workspace / "logs/verified.txt").unlink()
        result = dashboard.load_state(self.path, self.repo, self.workspace)
        self.assertEqual(result["summary"]["completed"], 0)
        self.assertEqual(result["summary"]["unverified"], 1)
        self.state["stages"][0]["tasks"][0]["evidence"] = []
        self.write()
        self.assertEqual(dashboard.load_state(self.path, self.repo, self.workspace)["summary"]["percent"], 0)
        (self.workspace / "logs/verified.txt").write_text("restored", encoding="utf-8")
        task = self.state["stages"][0]["tasks"][0]
        task["evidence"] = [{"base": "workspace", "path": "logs/verified.txt"}]
        del task["verified_at"]
        self.write()
        self.assertEqual(dashboard.load_state(self.path, self.repo, self.workspace)["summary"]["unverified"], 1)

    def test_bad_evidence_path_rejected(self):
        self.state["stages"][0]["tasks"][0]["evidence"][0]["path"] = "../outside.txt"
        self.write()
        with self.assertRaisesRegex(ValueError, "leaves"):
            dashboard.load_state(self.path, self.repo, self.workspace)

    def test_invalid_json_does_not_show_old_percentage(self):
        self.path.write_text("{broken", encoding="utf-8")
        result = dashboard.snapshot(self.repo, self.workspace)
        self.assertIsNone(result["state"])
        self.assertTrue(result["errors"])

    def test_live_api_and_plan_updates_without_restart(self):
        _, url = self.start()
        first = self.get(url + "api/status")
        self.assertEqual(first["state"]["headline"], "First load pending")
        self.state["headline"] = "Changed on disk"
        self.write()
        (self.workspace / "plans").mkdir()
        (self.workspace / "plans/new.md").write_text("next real task", encoding="utf-8")
        second = self.get(url + "api/status")
        self.assertEqual(second["state"]["headline"], "Changed on disk")
        self.assertEqual(second["notes"]["plans"][0]["text"], "next real task")

    def test_unknown_routes_and_cross_site_requests_rejected(self):
        _, url = self.start()
        for route in ("HANDOFF.md", "../progress/milestones.json", "api/anything"):
            with self.assertRaises(HTTPError) as error:
                self.opener.open(url + route, timeout=10)
            self.assertEqual(error.exception.code, 404)
            error.exception.close()
        request = Request(url + "api/status", headers={"Sec-Fetch-Site": "cross-site"})
        with self.assertRaises(HTTPError) as error:
            self.opener.open(request, timeout=10)
        self.assertEqual(error.exception.code, 403)
        error.exception.close()

    def test_repeated_launch_reuses_server_other_workspace_uses_new_port(self):
        server, url = self.start()
        reused, same_url = dashboard.select_server(self.repo, self.workspace, server.server_port)
        self.assertIsNone(reused)
        self.assertEqual(same_url, url)
        other, other_url = dashboard.select_server(self.repo, self.workspace / "other", server.server_port)
        self.addCleanup(other.server_close)
        self.assertNotEqual(other_url, url)

    def test_git_commit_and_dirty_state_are_live(self):
        def git(*args):
            result = subprocess.run(["git", "-C", str(self.repo), *args], capture_output=True,
                                    encoding="utf-8", errors="replace", timeout=10,
                                    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            self.assertEqual(result.returncode, 0, result.stderr)
        git("init", "-b", "main")
        git("add", "progress/milestones.json")
        git("-c", "user.name=Dashboard Test", "-c", "user.email=test@example.invalid", "commit", "-m", "First acceptance record")
        first = dashboard.git_state(self.repo)
        self.assertEqual(first["commits"][0]["subject"], "First acceptance record")
        self.assertEqual(first["changes"], [])
        self.state["headline"] = "Modified acceptance record"
        self.write()
        second = dashboard.git_state(self.repo)
        self.assertEqual(second["changes"], [" M progress/milestones.json"])
        git("add", "progress/milestones.json")
        git("-c", "user.name=Dashboard Test", "-c", "user.email=test@example.invalid", "commit", "-m", "Second acceptance record")
        third = dashboard.git_state(self.repo)
        self.assertEqual(third["commit_count"], 2)
        self.assertEqual(third["commits"][0]["subject"], "Second acceptance record")


if __name__ == "__main__":
    unittest.main()
