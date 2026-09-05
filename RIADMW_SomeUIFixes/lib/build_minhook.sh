#!/usr/bin/env bash
# Build MinHook as libMinHook.x64.lib (mingw clang, cross from Linux)
set -e
LLVM_MINGW="${LLVM_MINGW:-/opt/llvm-mingw}"
LIBDIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$LIBDIR/minhook-1.3.3"
OUT="$LIBDIR/libMinHook.x64.lib"
CC="$LLVM_MINGW/bin/x86_64-w64-mingw32-clang"
AR="$LLVM_MINGW/bin/llvm-ar"

SOURCES=(
  "$SRC/src/buffer.c"
  "$SRC/src/hook.c"
  "$SRC/src/trampoline.c"
  "$SRC/src/hde/hde64.c"
  "$SRC/src/hde/hde32.c"
)

mkdir -p "$SRC/_cross"
OBJS=()
for s in "${SOURCES[@]}"; do
  o="$SRC/_cross/$(basename "${s%.c}").o"
  "$CC" -c -O2 -DNDEBUG -D_CRT_SECURE_NO_WARNINGS \
    -I"$SRC/include" -I"$SRC/src" -I"$SRC/src/hde" \
    -o "$o" "$s"
  OBJS+=("$o")
done

"$AR" rcs "$OUT" "${OBJS[@]}"
ls -la "$OUT"
