#!/bin/sh

rm -rf dist >/dev/null # Shush.
mkdir dist

# ----------------------------------------- #
#                  Windows                  #
# ----------------------------------------- #

zig build-exe -target x86-windows -lshlwapi --subsystem windows -lc --name intermediate __windows.c tiny-json.c
mv intermediate.exe dist/Windows-x86.exe
mv intermediate.pdb dist/Windows-x86.pdb

zig build-exe -target x86_64-windows -lshlwapi --subsystem windows -lc --name intermediate __windows.c tiny-json.c
mv intermediate.exe dist/Windows-x86_64.exe
mv intermediate.pdb dist/Windows-x86_64.pdb

# zig build-exe -target arm-windows -lshlwapi --subsystem windows -lc --name intermediate __windows.c tiny-json.c
# mv intermediate.exe dist/Windows-arm.exe
# mv intermediate.pdb dist/Windows-arm.pdb

zig build-exe -target aarch64-windows -lshlwapi --subsystem windows -lc --name intermediate __windows.c tiny-json.c
mv intermediate.exe dist/Windows-aarch64.exe
mv intermediate.pdb dist/Windows-aarch64.pdb

# ----------------------------------------- #
#                   Linux                   #
# ----------------------------------------- #

zig cc -target x86_64-linux -o intermediate __unix.c tiny-json.c
mv intermediate dist/Linux-x86_64

zig cc -target x86-linux -o intermediate __unix.c tiny-json.c
mv intermediate dist/Linux-x86

zig cc -target arm-linux-gnueabi -o intermediate __unix.c tiny-json.c
mv intermediate dist/Linux-arm

zig cc -target aarch64-linux -o intermediate __unix.c tiny-json.c
mv intermediate dist/Linux-aarch64

# ----------------------------------------- #
#                   macOS                   #
# ----------------------------------------- #
