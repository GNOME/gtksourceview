#!/usr/bin/env python3
# Copyright 2026 Christian Hergert
# SPDX-License-Identifier: LGPL-2.1-or-later

from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


xmllint, manifest, schema, output, depfile = sys.argv[1:]
manifest = Path(manifest)
specs = []

for entry in ET.parse(manifest).iter("file"):
    path = manifest.parent / entry.text
    # testv1.lang uses the old format, which we reject at runtime.
    if path.suffix == ".lang" and path.name != "testv1.lang":
        specs.append(path)

result = subprocess.run(
    [xmllint, "--nonet", "--noout", "--relaxng", schema, *map(str, specs)],
    capture_output=True,
    text=True,
)
if result.returncode:
    sys.stderr.write(result.stderr)
    sys.exit(result.returncode)


def escape(path):
    return str(path).replace("\\", "\\\\").replace(" ", "\\ ").replace("#", "\\#").replace("$", "$$")


dependencies = " ".join(map(escape, [manifest, schema, *specs]))
Path(depfile).write_text(f"{escape(output)}: {dependencies}\n")
Path(output).write_text("Bundled language specifications validated.\n")
