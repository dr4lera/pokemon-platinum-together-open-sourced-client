"""Prepare a separate private runtime from this source build, never a release.

Copies only the declared NitroFS files, fresh foundation firmware/header and
the independently built executable plus its open-source runtime dependencies.
Does not copy an installed game, accounts, saves, settings or private references.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--build", type=Path, default=Path("build_win64"))
    parser.add_argument("--runtime-bin", type=Path, default=Path("C:/msys64/ucrt64/bin"))
    parser.add_argument("--objdump", default="C:/msys64/ucrt64/bin/objdump.exe")
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    output, build, runtime = args.output.resolve(), args.build.resolve(), args.runtime_bin.resolve()
    if output == source or source in output.parents:
        raise SystemExit("Private runtime must remain outside the public source checkout")
    if output.exists():
        raise SystemExit("Refusing to replace an existing runtime/profile directory")
    inputs = []
    targets = set()
    with (source / "platinum.us/filesys.csv").open(newline="", encoding="utf-8") as stream:
        for record in csv.DictReader(stream):
            src, target = PurePosixPath(record["Source File"]), PurePosixPath(record["Target File"].lstrip("/"))
            if any(path.is_absolute() or ".." in path.parts or "\\" in str(path) or ":" in str(path) for path in (src, target)):
                raise SystemExit("Invalid declared resource path")
            if str(target) in targets:
                raise SystemExit("Duplicate runtime resource")
            targets.add(str(target))
            incoming = build / src
            if not incoming.is_file():
                incoming = source / src
            if not incoming.is_file():
                raise SystemExit("Build the missing resource before preparing runtime: " + str(src))
            inputs.append((incoming, target))
    inputs += [(source / "res/pc_port/header.bin", PurePosixPath("header.bin")),
               (source / "res/pc_port/dwc_nv.bin", PurePosixPath("firmware.bin")),
               (build / "main.exe", PurePosixPath("CommunityClient.exe"))]
    for incoming, _ in inputs:
        if not incoming.is_file():
            raise SystemExit("Required source build input missing: " + str(incoming))

    # Resolve imports without executing the built program or any DLL.
    libraries = {path.name.lower(): path for path in runtime.glob("*.dll")}
    systems = Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32"
    queue = [build / "main.exe"]
    copied = set()
    while queue:
        binary = queue.pop()
        imported = subprocess.check_output([args.objdump, "-p", str(binary)], text=True, encoding="utf-8")
        for name in re.findall(r"DLL Name:\s*(\S+)", imported):
            lower = name.lower()
            if lower in libraries:
                if lower not in copied:
                    copied.add(lower)
                    queue.append(libraries[lower])
                    inputs.append((libraries[lower], PurePosixPath(libraries[lower].name)))
            elif not (systems / name).is_file() and not lower.startswith(("api-ms-win-", "ext-ms-win-")):
                raise SystemExit("Unresolved runtime import: " + name)
    output.mkdir(parents=True)
    records = []
    for incoming, target in inputs:
        destination = output / target
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(incoming, destination)
        records.append({"path": target.as_posix(), "size": destination.stat().st_size,
                        "sha256": hashlib.sha256(destination.read_bytes()).hexdigest()})
    (output / "sim_config.ini").write_text("[General]\nInternalResolutionScale=2\nWindowWidth=768\nWindowHeight=576\nFullScreen=false\nMasterVolume=0\nCapFrameRate=true\n", encoding="ascii")
    (output / "PRIVATE-RUNTIME.json").write_text(json.dumps({
        "purpose": "separate own-client development runtime; not a release", "files": records,
        "original_game_or_player_data_copied": False,
        "runtime_dlls": sorted(copied), "gameplay_verified": False,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared separate source runtime: {len(records)} files, {len(copied)} runtime DLLs.")
    print("Launch CommunityClient.exe with this directory as its working directory.")
    print("This preparation does not establish gameplay, animation, save or multiplayer parity.")


if __name__ == "__main__":
    main()
