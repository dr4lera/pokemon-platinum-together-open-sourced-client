# SPDX-License-Identifier: GPL-3.0-or-later
"""Read actual native SDK include paths/defines for isolated engine fixtures."""
import json
from pathlib import Path
import shlex
import sys


def native_flags(build, target):
    build = Path(build).resolve()
    commands = json.loads((build / "compile_commands.json").read_text())
    entry = next(item for item in commands if item["file"].replace("\\", "/").endswith(target))
    flags = []
    for word in shlex.split(entry["command"]):
        if word.startswith("-I"):
            path = Path(word[2:])
            resolved = path.resolve() if path.is_absolute() else (Path(entry["directory"]) / path).resolve()
            # Preserve strict diagnostics for our fixture and source while using
            # the real SDK's inherited C headers as system headers.
            if "/subprojects/" in resolved.as_posix():
                flags.extend(["-isystem", str(resolved)])
            else:
                flags.append("-I" + str(resolved))
        elif word.startswith("-D"):
            flags.append(word)
    return flags


if __name__ == "__main__":
    print(json.dumps(native_flags(sys.argv[1], sys.argv[2])))
