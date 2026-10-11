#!/usr/bin/env python3
"""Build a per-user installation and migrate the development profile once."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import uuid
import xml.etree.ElementTree as ET

MARKER = ".alamios-user-install.json"


def absolute_path(value):
    return Path(value).expanduser().absolute()


def desktop_string(value):
    """Escape a desktop-entry string value (including significant spaces)."""
    return str(value).replace("\\", "\\\\").replace("\n", "\\n").replace(
        "\r", "\\r").replace("\t", "\\t").replace(" ", "\\s")


def desktop_exec(path):
    # Exec is parsed first as a desktop string, then as a quoted argument.
    value = str(path).replace("%", "%%")
    for char in ('\\', '"', '`', '$'):
        value = value.replace(char, "\\" + char)
    return desktop_string('"' + value + '"')


def launcher(template, executable):
    lines = []
    for line in template.splitlines():
        if line.startswith("Exec=notepadqq"):
            line = "Exec=" + desktop_exec(executable) + line[len("Exec=notepadqq"):]
        elif line.startswith("TryExec="):
            line = "TryExec=" + desktop_string(executable)
        lines.append(line)
    return "\n".join(lines) + "\n"


def rewrite_sessions(staged, source, target):
    """Rewrite only cache references; actual document filenames stay intact."""
    sessions = [staged / "session.xml"]
    sessions.extend((staged / "backupCache").glob("window_*/window.xml"))
    for session in sessions:
        if not session.exists():
            continue
        tree = ET.parse(session)  # Invalid sessions abort before replacing data.
        if tree.getroot().tag != "Notepadqq":
            raise ValueError(f"Invalid Notepadqq session: {session.name}")
        changed = False
        for tab in tree.iter("Tab"):
            cached = tab.get("cacheFilePath")
            if not cached:
                continue
            try:
                relative = Path(cached).relative_to(source)
            except ValueError:
                continue
            if ".." in relative.parts:
                raise ValueError("A session cache path escapes the source profile")
            tab.set("cacheFilePath", str(target / relative))
            changed = True
        if changed:
            tree.write(session, encoding="utf-8", xml_declaration=True)


def prepare_profile(source, target, keep_existing, staging):
    if (target / MARKER).exists():
        return None
    if keep_existing or not source.is_dir():
        return None
    if source.resolve() == target.resolve():
        return None
    # Copy only Notepadqq data, never KDE settings, recent files or GPU caches.
    if source.is_symlink():
        raise ValueError("Profile migration does not support symbolic links")
    for path in source.rglob("*"):
        if path.is_symlink():
            raise ValueError("Profile migration does not support symbolic links")
    staged = staging / "profile"
    shutil.copytree(source, staged)
    rewrite_sessions(staged, source, target)
    return staged


def publish_profile(staged, target, source):
    target.parent.mkdir(parents=True, exist_ok=True)
    backup = None
    if staged is not None:
        # Copy on the destination filesystem before renaming, for safe rollback.
        with tempfile.TemporaryDirectory(prefix=".notepadqq-migration-", dir=target.parent) as tmp:
            ready = Path(tmp) / "profile"
            shutil.copytree(staged, ready)
            (ready / MARKER).write_text(json.dumps({"source": str(source)}) + "\n")
            if target.exists():
                backup = target.with_name("Notepadqq.before-fork-" + uuid.uuid4().hex)
                target.rename(backup)
            try:
                ready.rename(target)
            except OSError:
                if backup is not None:
                    backup.rename(target)
                raise
    else:
        target.mkdir(parents=True, exist_ok=True)
        if not (target / MARKER).exists():
            (target / MARKER).write_text(json.dumps({"source": None}) + "\n")
    return backup


def run(args, repo):
    prefix = absolute_path(args.prefix)
    config = absolute_path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config")
    data = absolute_path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share")
    source = absolute_path(args.profile_source or repo / "build/profile") / "config/Notepadqq"
    target = config / "Notepadqq"
    executable = prefix / "bin/notepadqq"
    desktop = data / "applications/notepadqq.desktop"
    if args.jobs < 1:
        raise ValueError("--jobs must be positive")
    if target.is_symlink():
        raise ValueError("The destination profile must not be a symbolic link")
    print(f"Executable: {executable}\nLauncher: {desktop}\nProfile: {target}", flush=True)
    if args.dry_run:
        print(f"Build preset: {args.preset}; profile source: {source}")
        print("Profile import: " + ("disabled/already installed" if args.keep_profile or
              (target / MARKER).exists() else "once, if source exists; back up existing destination"))
        return
    running = subprocess.run(["pgrep", "-u", str(os.getuid()), "-x", "notepadqq"],
                             stdout=subprocess.DEVNULL, check=False)
    if running.returncode == 0:
        raise ValueError("Close all Notepadqq instances before installing (including other editions)")
    if running.returncode != 1:
        raise ValueError("Unable to check running Notepadqq processes")
    with tempfile.TemporaryDirectory(prefix="notepadqq-install-") as tmp:
        staging = Path(tmp)
        profile = prepare_profile(source, target, args.keep_profile, staging)
        subprocess.run(["cmake", "--preset", args.preset], cwd=repo, check=True)
        subprocess.run(["cmake", "--build", "--preset", args.preset,
                        "--parallel", str(args.jobs)], cwd=repo, check=True)
        payload = staging / "payload"
        subprocess.run(["cmake", "--install", str(repo / "build" / args.preset),
                        "--prefix", str(payload)], cwd=repo, check=True)
        template = (payload / "share/applications/notepadqq.desktop").read_text()
        entry = launcher(template, executable)
        (payload / "share/applications/notepadqq.desktop").write_text(entry)
        shutil.copytree(payload, prefix, dirs_exist_ok=True)
        # XDG_DATA_HOME can differ from the installation prefix.
        if data != prefix / "share":
            shutil.copytree(payload / "share/icons", data / "icons", dirs_exist_ok=True)
        desktop.parent.mkdir(parents=True, exist_ok=True)
        desktop.write_text(entry)
        backup = publish_profile(profile, target, source)
        if backup:
            print(f"Previous profile backup: {backup}")
    for program, options in (("update-desktop-database", [str(desktop.parent)]),
                             ("kbuildsycoca6", ["--noincremental"])):
        if shutil.which(program):
            result = subprocess.run([program, *options], check=False)
            if result.returncode:
                print(f"Menu cache refresh failed: {program}; log out and back in if needed.")
    print("Installed. Open Notepadqq from the application menu.")
    print(f"For terminal use, put {prefix / 'bin'} before /usr/bin in PATH.")


def main():
    repo = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", default=str(Path.home() / ".local"))
    parser.add_argument("--profile-source", help="Development XDG profile root (default: build/profile)")
    parser.add_argument("--keep-profile", action="store_true", help="Keep standard settings instead of importing")
    parser.add_argument("--preset", choices=("release", "dev"), default="release")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--dry-run", action="store_true", help="Print destinations without building or writing")
    try:
        run(parser.parse_args(), repo)
    except (OSError, ValueError, ET.ParseError, subprocess.CalledProcessError) as error:
        print(f"Installation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
