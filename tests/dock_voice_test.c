/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Dock's voices (DOCK.md §3, voice.c "Dock: voices by member"): each band member keeps its own share, a member over its
 * share steals only from itself, and the drummer's lane never takes a band voice nor gives one up to the band.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/dock_voice_test tests/dock_voice_test.c -lm */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok)
{
    printf("dock voices: %-62s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static void blocks(uint32_t n)                    /* let fades finish (a stolen voice fades over one block) */
{
    static int32_t o[2u * CTL];
    while (n--)
        mix_block(o, CTL);
}

static uint32_t sounding(const track_t *t)        /* held or ringing, not fading out */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].stage != 4u;
    return n;
}

static uint32_t held(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].stage != 4u && t->v[i].note == note)
            return 1;
    return 0;
}

int main(void)
{
    static const uint8_t chord[7] = {60, 64, 67, 71, 74, 77, 81};
    track_t *bass = &trk[0], *keys = &trk[1], *lead = &trk[2], *drums = &trk[3];
    uint32_t i, k, kills0;
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_power_on_sound(&trk[k], k);
    keys->p[P_VOICE] = V_POLY;
    keys->p[P_SUS] = 127;
    blocks(10);

    trk_note_on(bass, 36, 100);
    trk_note_on(lead, 72, 100);
    for (i = 0; i < 6u; i++)
        trk_note_on(keys, chord[i], 90);
    blocks(20);
    check("full band: bass 1, keys 6, lead 1", sounding(bass) == 1u && sounding(keys) == 6u && sounding(lead) == 1u);

    kills0 = voice_kills_part[0] + voice_kills_part[2];
    trk_note_on(keys, chord[6], 90);              /* a 7th key: over the share */
    blocks(20);
    check("7th key sounds, keys stay at 6", held(keys, chord[6]) && sounding(keys) == 6u);
    check("the lowest key of the chord is kept", held(keys, chord[0]));
    check("bass and lead untouched by the keys", held(bass, 36) && held(lead, 72) &&
                                                   voice_kills_part[0] + voice_kills_part[2] == kills0);

    for (i = 0; i < 24u; i++) {                   /* a drum flurry: kick, snare, hats, toms */
        static const uint8_t hits[6] = {36, 38, 42, 46, 45, 50};
        trk_note_on(drums, hits[i % 6u], 110);
        blocks(2);
        trk_note_off(drums, hits[i % 6u]);
    }
    check("drums stay in their lane (6 at most)", sounding(drums) <= 6u);
    check("drums never take a band voice", held(bass, 36) && held(lead, 72) && sounding(keys) == 6u &&
                                             voice_kills_part[0] + voice_kills_part[2] == kills0);

    trk_note_on(bass, 38, 100);                   /* a new bass note: steals only the bass's own */
    blocks(20);
    check("bass moves to its new note, still 1 voice", held(bass, 38) && sounding(bass) == 1u);
    check("keys and lead keep theirs", sounding(keys) == 6u && held(lead, 72));

    {   /* Bass and Lead: an overlapping note (a tie, a slide) strikes again, as a player's next note does */
        uint32_t age0;
        host_preset(lead, ENGI_PHYS, PHYS_SURF);
        lead->p[P_VOICE] = DOCK_MONO_MODE;
        blocks(20);
        trk_note_on(lead, 72, 100);
        blocks(10);
        age0 = lead->v[0].age;
        trk_note_on(lead, 74, 100);                   /* before 72's note-off: overlapping */
        trk_note_off(lead, 72);
        blocks(4);
        check("an overlapping note on the Lead strikes again (the pluck plucks)", lead->v[0].age != age0 && held(lead, 74));
        trk_note_off(lead, 74);
    }

    printf(fails ? "dock voices: %d FAILED\n" : "dock voices: all passed\n", fails);
    return fails != 0;
}
