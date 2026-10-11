#!/usr/bin/env python3
"""Refresh desktop text MIME declarations from a shared-mime-info XML source."""

import argparse
import hashlib
from pathlib import Path
import xml.etree.ElementTree as ET

NAMESPACE = "{http://www.freedesktop.org/standards/shared-mime-info}"


def text_types(database):
    root = ET.fromstring(database)
    if root.tag != NAMESPACE + "mime-info":
        raise ValueError("Expected a shared-mime-info XML database")
    nodes = {node.attrib["type"]: node for node in root.findall(NAMESPACE + "mime-type")}
    aliases = {alias.attrib["type"]: name for name, node in nodes.items()
               for alias in node.findall(NAMESPACE + "alias")}
    parents = {name: {parent.attrib["type"] for parent in node.findall(NAMESPACE + "sub-class-of")}
               for name, node in nodes.items()}
    selected = {"text/plain"} | {name for name in nodes if name.startswith("text/")}
    while True:
        expanded = selected | {name for name, bases in parents.items()
                               if any(aliases.get(base, base) in selected for base in bases)}
        if expanded == selected:
            break
        selected = expanded
    # Empty regular files can be opened even before they contain detectable text.
    if "application/x-zerosize" in nodes:
        selected.add("application/x-zerosize")
    # Private Windows association identifiers are not portable MIME names.
    selected |= {alias for alias, canonical in aliases.items()
                 if canonical in selected and not alias.startswith("zz-")}
    return sorted(selected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path, help="Downloaded freedesktop.org.xml.in or compiled XML source")
    args = parser.parse_args()
    database = args.database.read_bytes()
    types = text_types(database)
    desktop = Path(__file__).resolve().parent.parent / "support_files/shortcuts/notepadqq.desktop"
    lines = desktop.read_text().splitlines()
    replacements = sum(line.startswith("MimeType=") for line in lines)
    if replacements != 1:
        raise ValueError("Expected exactly one MimeType declaration")
    lines = [line for line in lines if not line.startswith("# Text MIME registry")]
    output = []
    for line in lines:
        if line.startswith("MimeType="):
            output.append("# Text MIME registry: shared-mime-info; refresh with scripts/update_text_mime_types.py")
            output.append("# Text MIME registry SHA-256: " + hashlib.sha256(database).hexdigest())
            line = "MimeType=" + ";".join(types) + ";"
        output.append(line)
    desktop.write_text("\n".join(output) + "\n")
    print(f"Declared {len(types)} text and empty-file MIME types (including aliases).")


if __name__ == "__main__":
    main()
