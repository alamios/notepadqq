"""Installer regressions; all writes use temporary profiles, never user data."""

import argparse
from contextlib import redirect_stdout
import io
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

import install_user as installer


class UserInstallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="notepadqq-installer-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = Path(__file__).resolve().parent.parent
        self.source_root = self.root / "development profile"
        self.source = self.source_root / "config/Notepadqq"
        self.config = self.root / "config"
        self.data = self.root / "data"
        self.target = self.config / "Notepadqq"
        self.prefix = self.root / 'install space $cash `literal` %quote"'
        self.args = argparse.Namespace(prefix=str(self.prefix), profile_source=str(self.source_root),
                                       keep_profile=False, preset="release", jobs=4, dry_run=False)
        self.calls = []

    def fixture(self):
        cached = self.source / "tabCache/unsaved.123"
        cached.parent.mkdir(parents=True)
        cached.write_text("unsaved contents\n")
        (self.source / "Notepadqq.ini").write_text("[General]\nTest=true\n")
        tree = ET.Element("Notepadqq")
        view = ET.SubElement(tree, "View")
        ET.SubElement(view, "Tab", cacheFilePath=str(cached), filePath="",
                      unsavedTitle="My notes", tabColor="green")
        ET.SubElement(view, "Tab", cacheFilePath="/external/cache", filePath="/real/document")
        ET.ElementTree(tree).write(self.source / "session.xml", encoding="utf-8")
        backup = self.source / "backupCache/window_42"
        backup.mkdir(parents=True)
        (backup / "cached.456").write_text("recovered text")
        root = ET.Element("Notepadqq")
        ET.SubElement(ET.SubElement(root, "View"), "Tab", cacheFilePath=str(backup / "cached.456"))
        ET.ElementTree(root).write(backup / "window.xml", encoding="utf-8")
        # User documents with XML extensions must never be parsed or rewritten.
        (cached.parent / "invalid.xml").write_text("unfinished <document")
        (self.source_root / "config/kdeglobals").write_text("unrelated")

    def subprocess(self, command, **kwargs):
        self.calls.append(command)
        if command[0] == "pgrep":
            return subprocess.CompletedProcess(command, 1)
        if command[:2] == ["cmake", "--install"]:
            payload = Path(command[-1])
            binary = payload / "bin/notepadqq"
            binary.parent.mkdir(parents=True)
            binary.write_text("#!/bin/sh\nexit 0\n")
            binary.chmod(0o755)
            desktop = payload / "share/applications/notepadqq.desktop"
            desktop.parent.mkdir(parents=True)
            shutil.copyfile(self.repo / "support_files/shortcuts/notepadqq.desktop", desktop)
            (payload / "share/icons").mkdir()
            (payload / "share/notepadqq/editor").mkdir(parents=True)
        return subprocess.CompletedProcess(command, 0)

    def run_install(self):
        with patch.dict(os.environ, {"XDG_CONFIG_HOME": str(self.config), "XDG_DATA_HOME": str(self.data)}), \
                patch.object(installer.subprocess, "run", side_effect=self.subprocess), \
                patch.object(installer.shutil, "which", return_value=None), redirect_stdout(io.StringIO()):
            installer.run(self.args, self.repo)

    def test_migration_backup_and_subsequent_update(self):
        self.fixture()
        self.target.mkdir(parents=True)
        (self.target / "old-session").write_text("old profile")
        self.run_install()
        tabs = list(ET.parse(self.target / "session.xml").iter("Tab"))
        self.assertEqual(tabs[0].get("cacheFilePath"), str(self.target / "tabCache/unsaved.123"))
        self.assertEqual(Path(tabs[0].get("cacheFilePath")).read_text(), "unsaved contents\n")
        self.assertEqual(tabs[0].get("unsavedTitle"), "My notes")
        self.assertEqual(tabs[0].get("tabColor"), "green")
        self.assertEqual(tabs[1].get("filePath"), "/real/document")
        self.assertEqual(tabs[1].get("cacheFilePath"), "/external/cache")
        backup_tab = next(ET.parse(self.target / "backupCache/window_42/window.xml").iter("Tab"))
        self.assertEqual(Path(backup_tab.get("cacheFilePath")).read_text(), "recovered text")
        backups = list(self.config.glob("Notepadqq.before-fork-*"))
        self.assertEqual(len(backups), 1)
        self.assertEqual((backups[0] / "old-session").read_text(), "old profile")
        self.assertEqual((self.target / "tabCache/invalid.xml").read_text(), "unfinished <document")
        self.assertFalse((self.config / "kdeglobals").exists())
        self.assertTrue((self.prefix / "bin/notepadqq").stat().st_mode & 0o111)
        launcher = (self.data / "applications/notepadqq.desktop").read_text()
        self.assertIn("Name=Notepadqq\n", launcher)
        self.assertIn("Terminal=false\n", launcher)
        self.assertIn("Exec=" + installer.desktop_exec(self.prefix / "bin/notepadqq") + " %U", launcher)
        self.assertIn(" --new-window", launcher)
        (self.target / "session.xml").write_text("new live session")
        self.run_install()
        self.assertEqual((self.target / "session.xml").read_text(), "new live session")
        self.assertEqual(len(list(self.config.glob("Notepadqq.before-fork-*"))), 1)
        self.assertTrue((self.source / "session.xml").exists())

    def test_invalid_session_leaves_destination_and_binary_untouched(self):
        self.fixture()
        (self.source / "session.xml").write_text("broken <xml")
        self.target.mkdir(parents=True)
        (self.target / "keep").write_text("keep")
        with self.assertRaises(ET.ParseError):
            self.run_install()
        self.assertEqual((self.target / "keep").read_text(), "keep")
        self.assertFalse(self.prefix.exists())
        self.assertEqual(len(self.calls), 1)  # Only the process check, no build.

    def test_keep_profile_then_update_never_imports(self):
        self.fixture()
        self.target.mkdir(parents=True)
        (self.target / "Notepadqq.ini").write_text("existing settings")
        self.args.keep_profile = True
        self.run_install()
        self.args.keep_profile = False
        self.run_install()
        self.assertEqual((self.target / "Notepadqq.ini").read_text(), "existing settings")
        self.assertFalse((self.target / "session.xml").exists())

    def test_fresh_install_without_development_profile(self):
        self.run_install()
        self.assertTrue((self.target / installer.MARKER).exists())
        self.assertTrue((self.data / "applications/notepadqq.desktop").exists())

    def test_dry_run_writes_nothing_and_starts_no_process(self):
        self.args.dry_run = True
        self.run_install()
        self.assertFalse(self.prefix.exists())
        self.assertFalse(self.config.exists())
        self.assertEqual(self.calls, [])

    def test_running_editor_aborts_before_any_writes(self):
        with patch.object(installer.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)), \
                redirect_stdout(io.StringIO()), self.assertRaisesRegex(ValueError, "Close all"):
            installer.run(self.args, self.repo)
        self.assertFalse(self.prefix.exists())

    def test_build_failure_leaves_existing_profile_untouched(self):
        self.fixture()
        self.target.mkdir(parents=True)
        (self.target / "keep").write_text("original")
        original = self.subprocess

        def fail(command, **kwargs):
            if command[:2] == ["cmake", "--build"]:
                raise subprocess.CalledProcessError(1, command)
            return original(command, **kwargs)

        with patch.object(self, "subprocess", side_effect=fail), self.assertRaises(subprocess.CalledProcessError):
            self.run_install()
        self.assertEqual((self.target / "keep").read_text(), "original")
        self.assertFalse(self.prefix.exists())

    def test_failed_profile_publish_restores_original(self):
        self.fixture()
        self.target.mkdir(parents=True)
        (self.target / "keep").write_text("original")
        staging = self.root / "staging"
        staging.mkdir()
        staged = installer.prepare_profile(self.source, self.target, False, staging)
        rename = Path.rename

        def fail_final(path, destination):
            if path.name == "profile" and path.parent.name.startswith(".notepadqq-migration-"):
                raise OSError("simulated final rename failure")
            return rename(path, destination)

        with patch.object(Path, "rename", fail_final), self.assertRaises(OSError):
            installer.publish_profile(staged, self.target, self.source)
        self.assertEqual((self.target / "keep").read_text(), "original")
        self.assertFalse((self.target / installer.MARKER).exists())

    def test_symlink_in_source_is_rejected(self):
        self.fixture()
        (self.source / "linked-cache").symlink_to(self.source / "tabCache")
        with self.assertRaisesRegex(ValueError, "symbolic links"):
            self.run_install()
        self.assertFalse(self.prefix.exists())

    def test_escaping_cache_path_is_rejected(self):
        self.fixture()
        tree = ET.parse(self.source / "session.xml")
        next(tree.iter("Tab")).set("cacheFilePath", str(self.source / "../outside"))
        tree.write(self.source / "session.xml")
        with self.assertRaisesRegex(ValueError, "escapes"):
            self.run_install()
        self.assertFalse(self.prefix.exists())


if __name__ == "__main__":
    unittest.main()
