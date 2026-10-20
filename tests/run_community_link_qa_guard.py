#!/usr/bin/env python3
"""Compile the actual C++17 preflight guard; all files are disposable fixtures."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument("--cxx", default=os.environ.get("CXX", "C:/msys64/ucrt64/bin/g++.exe" if os.name == "nt" else "c++"))
args = parser.parse_args()
parent = root / ".build-tools"
parent.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="community-link-qa-guard-", dir=parent) as temporary:
    scratch = Path(temporary).resolve()
    executable = scratch / ("guard.exe" if os.name == "nt" else "guard")
    subprocess.run([args.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I" + str(root / "include"), str(root / "src/port/community_link_qa_guard.cpp"), str(root / "tests/community_link_qa_guard_test.cpp"), "-o", str(executable)], check=True)
    subprocess.run([str(executable), str(scratch)], check=True)
    if os.name == "nt":
        target = scratch / ("community-link-qa-" + "a" * 32 + "-host")
        junction = scratch / "junction"
        quote = lambda text: "'" + str(text).replace("'", "''") + "'"
        script = "New-Item -ItemType Junction -Path " + quote(junction) + " -Target " + quote(target) + " -ErrorAction Stop | Out-Null"
        subprocess.run(["powershell.exe", "-NoProfile", "-Command", script], check=True)
        subprocess.run([str(executable), "--reject-reparse", str(junction)], check=True)
