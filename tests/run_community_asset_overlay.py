"""Compile the actual portable resolver and FS_OpenFile body, without SDK/assets."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("gcc")
if not compiler and Path("C:/msys64/ucrt64/bin/gcc.exe").exists():
    compiler = "C:/msys64/ucrt64/bin/gcc.exe"
if not compiler:
    raise SystemExit("Set CC to a C compiler")
fs = root / "subprojects/libntr/libraries/fs"
source = (fs / "src/fs_file.c").read_text()
start = source.index("BOOL FS_OpenFile (FSFile *p_file, const char *path)")
end = source.index("\n#if SDK_VERSION_MAJOR == 5\nBOOL FS_OpenFileEx", start)
scratch = root / ".build-tools"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="asset-overlay-tests-", dir=scratch) as directory:
    temp = Path(directory)
    runtime, cwd, outside, escape = [temp / name for name in ("runtime", "cwd", "outside", "escape")]
    for folder in (runtime, cwd, outside, escape):
        folder.mkdir()
    fixtures = {
        runtime / "patch/poketool/personal/pl_personal.narc": "patch",
        runtime / "poketool/personal/pl_personal.narc": "runtime-personal",
        cwd / "poketool/personal/pl_personal.narc": "cwd-personal",
        runtime / "patch/msgdata/pl_msg.narc": "patch-lowercase",
        cwd / "msgdata/pl_msg.narc": "cwd-msg",
        runtime / "poketool/personal/wotbl.narc": "runtime-base",
        cwd / "poketool/personal/wotbl.narc": "cwd-learnset",
        runtime / "patch/battle/skill/sub_seq.narc": "must-not-load",
        runtime / "battle/skill/sub_seq.narc": "runtime-unpatched",
        runtime / "patch/wazaeffect/we.arc": "must-not-load",
        cwd / "wazaeffect/we.arc": "cwd-unpatched",
        cwd / "battle/graphic/pl_batt_obj.narc": "cwd-base",
        cwd / "local/lower.narc": "cwd-lowercase",
        outside / "pl_msg.narc": "must-not-escape",
        outside / "wotbl.narc": "must-not-escape",
    }
    for path, contents in fixtures.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)
    env = os.environ.copy()
    env["PLATINUM_COMMUNITY_ASSET_ROOT"] = str(runtime)
    try:
        (escape / "patch").mkdir()
        (escape / "patch/msgdata").symlink_to(outside, target_is_directory=True)
        (escape / "poketool/personal").mkdir(parents=True)
        (escape / "poketool/personal/wotbl.narc").symlink_to(outside / "wotbl.narc")
    except OSError:
        print("Symlink fixtures unavailable on this host; confinement branches remain compile-checked")
    else:
        env["OVERLAY_TEST_LINKS"] = "1"
        env["OVERLAY_TEST_ESCAPE_ROOT"] = str(escape)
    (temp / "actual_fs_open.inc").write_text(source[start:end])
    executable = temp / ("overlay.exe" if os.name == "nt" else "overlay")
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable",
                    "-I" + str(temp), "-I" + str(fs / "include"),
                    str(fs / "src/community_asset_overlay.c"),
                    str(root / "tests/community_asset_overlay_test.c"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable), str(runtime)], cwd=cwd, env=env, check=True)
