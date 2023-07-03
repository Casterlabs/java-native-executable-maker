#!/bin/sh

rm -rf dist >/dev/null # Shush.
mkdir dist

# ----------------------------------------- #
#                  Windows                  #
# ----------------------------------------- #
zig build-exe -target x86_64-windows-gnu -lshlwapi --subsystem windows -lc --name intermediate __windows.c tiny-json.c
mv intermediate.exe dist/Windows-x86_64.exe
mv intermediate.pdb dist/Windows-x86_64.pdb

zig build-exe -target aarch64-windows-gnu -lshlwapi --subsystem windows -lc --name intermediate __windows.c tiny-json.c
mv intermediate.exe dist/Windows-aarch64.exe
mv intermediate.pdb dist/Windows-aarch64.pdb

# ----------------------------------------- #
#                   Linux                   #
# ----------------------------------------- #

# ----------------------------------------- #
#                   macOS                   #
# ----------------------------------------- #
