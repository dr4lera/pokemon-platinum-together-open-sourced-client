# Build the community source client

The independent client uses the public PC-port engine and libntr replacements.
The Windows native build does not require the original Together executable,
Metrowerks, an original Nintendo SDK, or changes to the installed game. Native
Together behavior is a reconstruction target, not a completed feature claim.

Install a normal UCRT64 GNU development environment with Python 3, Git, GCC/G++,
NASM, Ninja, Flex, Bison, GNU ARM GCC/binutils, SDL2, ENet, zlib, libpng and
pkg-config. ARM tools compile data scripts, not a proprietary SDK. Add their bin
directories to PATH. The supplied Windows build script uses Meson 1.12.0 at the
recorded source revision and checks out public dependencies at pinned commits.

```powershell
.\tools\community\build-win64.ps1 -Jobs 8
```

The native executable is `build_win64/main.exe`. The first source build also
generates resource headers. `PYTHONUTF8=1` is required on Windows and is set by
the script. The old upstream `make win64` route downloads an ARM matching
toolchain that this native build does not require; use the community script.

Run rule checks separately, without a game installation or ROM:

```powershell
python tests/run_community_rules.py
gcc -std=c99 -Wall -Wextra -Werror -Iinclude tests/capture_rules_core_test.c -o build_win64/capture_rules_core_test.exe
.\build_win64\capture_rules_core_test.exe
```

For actual play, supply your own compatible US Platinum ROM and use a separate
runtime folder/profile. Do not copy your original saves into this development
client. Base-engine startup, assets, save handling and every Together-specific
feature require their acceptance checks. An executable build alone is not proof
that native co-op, custom trainer art, trading, modern content or 1:1 parity work.

Do not publish generated resources, original game binaries, ROMs, account keys,
saves, or private reverse-engineering references. Build and binary release
packaging are separate; the inherited asset-copy packaging scripts are not a
community release allowlist. Release publication remains paused during migration.
