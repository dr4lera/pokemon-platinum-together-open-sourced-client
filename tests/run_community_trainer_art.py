"""Build only the original portable decoder in a disposable folder."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
compiler=os.environ.get("CXX") or shutil.which("g++") or "C:/msys64/ucrt64/bin/g++.exe"
scratch=root/".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="trainer-art-decoder-",dir=scratch) as directory:
    binary=Path(directory)/"decoder.exe"
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-O1","-I"+str(root/"include"),
        str(root/"src/port/community_trainer_art.cpp"),str(root/"tests/community_trainer_art_test.cpp"),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
