#!/usr/bin/env python3
# Copyright 2026 Christian Hergert
# SPDX-License-Identifier: LGPL-2.1-or-later

from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


xmllint, manifest, schema, output, depfile = sys.argv[1:]
manifest = Path(manifest)
styles = []

for resource in ET.parse(manifest).iter("gresource"):
    if resource.get("prefix") == "/org/gnome/gtksourceview/styles":
        for entry in resource.iter("file"):
            styles.append(manifest.parent / entry.text)

result = subprocess.run(
    [xmllint, "--nonet", "--noout", "--relaxng", schema, *map(str, styles)],
    capture_output=True,
    text=True,
)
if result.returncode:
    sys.stderr.write(result.stderr)
    sys.exit(result.returncode)


def escape(path):
    return str(path).replace("\\", "\\\\").replace(" ", "\\ ").replace("#", "\\#").replace("$", "$$")


dependencies = " ".join(map(escape, [manifest, schema, *styles]))
Path(depfile).write_text(f"{escape(output)}: {dependencies}\n")
Path(output).write_text("Bundled style schemes validated.\n")
