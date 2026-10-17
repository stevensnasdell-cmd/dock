/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Dock's band on the real UI (DOCK.md §3, §10 step 2), on Melodee's UI test stubs (tests/ui_test.c): always four parts,
 * Bass and Lead mono legato whatever sound loads, no patterns or recording behind SEQ and REC, no Add / Remove track.
 *   cc -O1 -w -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -o build/host/dock_ui_test tests/dock_ui_test.c -lm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
static void dcheck(const char *what, int ok)
{
    printf("dock ui: %-66s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

int main(void)
{
    uint32_t sig;
    ui_power_on();
    frame();

    dcheck("four parts at power-on, whatever the song says", trk_n() == 4u);
    song.ntrk = 1;
    dcheck("  (a song of one track still has four)", trk_n() == 4u);
    dcheck("Bass and Lead mono legato, Keys poly", trk[DOCK_BASS].p[P_VOICE] == V_LEGATO &&
                                                    trk[DOCK_LEAD].p[P_VOICE] == V_LEGATO &&
                                                    trk[DOCK_KEYS].p[P_VOICE] == V_POLY);
    apply_preset_to(&trk[DOCK_BASS], 3);          /* PROPHET Forever Keys: a poly preset onto the Bass */
    apply_preset_to(&trk[DOCK_LEAD], 3);
    apply_preset_to(&trk[DOCK_KEYS], 3);
    dcheck("a poly sound loaded on Bass and Lead plays mono legato", trk[DOCK_BASS].p[P_VOICE] == V_LEGATO &&
                                                                      trk[DOCK_LEAD].p[P_VOICE] == V_LEGATO);
    dcheck("  and stays poly on Keys", trk[DOCK_KEYS].p[P_VOICE] == V_POLY);
    dcheck("Bass and Lead: last-note priority (a sequencer's slides)", trk[DOCK_BASS].p[P_PRIO] == 0 &&
                                                                         trk[DOCK_LEAD].p[P_PRIO] == 0);

    ui_power_on();
    frame();
    press(B_SEQ);
    dcheck("SEQ: no page (HOME stays)", ui.home == 1u);
    hold(B_SEQ);
    dcheck("SEQ held: no patterns grid, HOME stays", ui.home == 1u && !pattern_keys_on());
    sig = steps_sig(TSEL);
    press(B_REC);
    dcheck("REC: no recording armed, transport stopped", !song.rec && !song.playing);
    hold(B_REC);
    dcheck("REC held: no capture into a pattern", steps_sig(TSEL) == sig && ui.home == 1u);
    dcheck("the Stage sheet has no Add / Remove track", NELEM(SHEET_STAGE) == 5u);

    printf(fails ? "dock ui: %d FAILED\n" : "dock ui: all passed\n", fails);
    return fails != 0;
}
