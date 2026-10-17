#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the Dock emulator core.
#   emu/build.sh          -> build/emu/dock.wasm (+ dock.mjs)
#   emu/build.sh test     -> native and WebAssembly self-tests, compared byte for byte
# Needs emsdk (EMSDK_PYTHON pointing at Python >= 3.10 if the system one is older).
set -e
cd "$(dirname "$0")/.."
OUT=build/emu
mkdir -p "$OUT" build/gen

# the generated tables the DSP needs (pure Python; ./build.sh makes these too)
[ -f build/gen/melodee_tables.h ] || {
    python3 tools/gen_tables.py build/gen/melodee_tables.h
    python3 tools/gen_fm6_patches.py build/gen/melodee_fm6.h
    python3 tools/gen_cz1_factory.py build/gen/melodee_cz1.h
    python3 tools/gen_prophet_factory.py build/gen/melodee_prophet_factory.h
    python3 tools/gen_909_tables.py build/gen/x0x_drum_tables.h
}

if ! command -v emcc >/dev/null 2>&1; then
    : "${EMSDK_PYTHON:=$HOME/.local/share/uv/python/cpython-3.12-macos-aarch64-none/bin/python3.12}"
    export EMSDK_PYTHON
    . "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1
fi

# as tests/run_tests.sh builds hostsim: no fused multiply-add, so the floats round the same everywhere
FLAGS="-O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src"
EXPORTS='["_dock_init","_dock_midi","_dock_render","_dock_left","_dock_right","_dock_voices","_dock_rate"]'

if [ "$1" = test ]; then
    cc $FLAGS -DDOCK_SELFTEST -o "$OUT/selftest" emu/dock_wasm.c -lm
    "$OUT/selftest" "$OUT/native.f32"
    emcc $FLAGS -DDOCK_SELFTEST -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -o "$OUT/selftest.js" emu/dock_wasm.c
    node "$OUT/selftest.js" "$OUT/wasm.f32"
    if cmp -s "$OUT/native.f32" "$OUT/wasm.f32"; then
        echo "selftest: native and WebAssembly renders are identical"
    else
        echo "selftest: renders DIFFER"; exit 1
    fi
    exit 0
fi

emcc $FLAGS -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,worker,node -sSINGLE_FILE=1 \
    -sEXPORTED_FUNCTIONS="$EXPORTS" -sEXPORTED_RUNTIME_METHODS='["HEAPF32"]' \
    -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=16MB -sFILESYSTEM=0 --no-entry \
    -o "$OUT/dock.mjs" emu/dock_wasm.c
ls -la "$OUT/dock.mjs"
