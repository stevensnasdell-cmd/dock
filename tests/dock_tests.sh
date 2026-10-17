#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Steven Snasdell
# Dock's quick host checks (no JieLi toolchain needed): the band's voices, the JUNO, EP, organ and PLUCK engines, the UI on Dock and on Melodee's own rules,
# the emulator's boot and keys. Needs build/gen (emu/build.sh makes it).   tests/dock_tests.sh
set -e
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT" build/emu
F="-O1 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src"
fail=0
run() { name=$1; shift; if "$@" > "$OUT/$name.log" 2>&1; then echo "ok    $name"; else echo "FAIL  $name ($OUT/$name.log)"; fail=1; fi; }

cc $F -o $OUT/dock_voice_test tests/dock_voice_test.c -lm
run "voices by member" $OUT/dock_voice_test
cc $F -o $OUT/juno_test tests/juno_test.c -lm
run "JUNO: sounds, chorus, cost" $OUT/juno_test
cc $F -o $OUT/ep_test tests/ep_test.c -lm
run "EP: sounds, bark, damper, trem, cost" $OUT/ep_test
cc $F -o $OUT/organ_pluck_test tests/organ_pluck_test.c -lm
run "organ (WHEEL) and PLUCK: sounds, rotary, upright, cost" $OUT/organ_pluck_test
cc $F -DMELODEE_USB_AUDIO=1 -o $OUT/dock_ui_test tests/dock_ui_test.c -lm
run "the band on the UI" $OUT/dock_ui_test
# Melodee's UI suite checks Melodee's rules (1-4 tracks, chords on track 1): it runs with DOCK_BAND=0; on Dock its
# track-count and track-1 chord checks fail by design. Dock's own behaviour is checked by the Dock tests here.
cc $F -DMELODEE_USB_AUDIO=1 -DDOCK_BAND=0 -o $OUT/ui_test_melodee tests/ui_test.c -lm
run "UI (Melodee rules, DOCK_BAND=0)" $OUT/ui_test_melodee
# every screen in every palette (Melodee's lint): no text overlapping, cut or spilling out of its cell
cc $F -DMELODEE_USB_AUDIO=1 -Itests -o $OUT/ui_render tests/ui_render.c -lm
mkdir -p build/ui_new/ppm
run "every screen: no text cut or overlapping" sh -c "$OUT/ui_render build/ui_new | tee /dev/stderr | grep -q ' 0 lint findings'"
cc $F -DMELODEE_USB_AUDIO=1 -DDOCK_WEB_SELFTEST -o build/emu/web_selftest emu/dock_web.c -lm
run "emulator boot, keys, EDIT" build/emu/web_selftest
exit $fail
