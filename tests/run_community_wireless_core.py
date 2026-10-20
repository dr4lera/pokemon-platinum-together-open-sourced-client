"""Compile and exercise the actual portable WM semantic state machine."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CXX") or shutil.which("g++")
if not compiler and Path("C:/msys64/ucrt64/bin/g++.exe").exists():
    compiler = "C:/msys64/ucrt64/bin/g++.exe"
if not compiler:
    raise SystemExit("Set CXX to a C++17 compiler")
env = os.environ.copy()
env["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + env.get("PATH", "")
scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="wireless-core-", dir=scratch) as directory:
    executable = Path(directory) / ("core.exe" if os.name == "nt" else "core")
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(root / "include"), str(root / "src/port/community_wireless_core.cpp"),
                    str(root / "tests/community_wireless_core_test.cpp"), "-o", str(executable)], check=True, env=env)
    subprocess.run([str(executable)], check=True, env=env)
