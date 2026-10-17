/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell */
/* Dock's loudness trims: factory sounds measured too quiet or too loud for their role (a bass line, a 4-note chord, a
 * lead line, a beat, each alone at LEVEL's default), brought to Bass -24, Keys -27, Lead -26, Drums -25 dB RMS at the
 * device's level. Applied to the part's audio before its LEVEL (mix.c dock_trim), so the mixer still balances on top;
 * a user preset, or a sound not listed here, plays as it is. Measured 2026-10-17 in the emulator; re-measure when an
 * engine's gain changes. Included by mix.c */
typedef struct { uint8_t eng, preset; uint16_t q12; } dock_trim_t;   /* q12: the gain, 4096 = 0 dB */
static const dock_trim_t DOCK_TRIMS[] = {
    {ENGI_EP, EP_SUITCASE, 23034},           /* SUITCASE: +15.0 dB (measured -42.2 in its role) */
    {ENGI_JUNO, 1, 7715},                    /* SILK LEAD: +5.5 dB (measured -31.7 in its role) */
    {ENGI_DRUM, 0, 5786},                    /* 808 KIT: +3.0 dB (measured -27.8 in its role) */
    {ENGI_PROPHET, 21, 3651},                /* Bass Face: -1.0 dB (measured -22.9 in its role) */
    {ENGI_JUNO, 2, 3072},                    /* STR MACHINE: -2.5 dB (measured -24.4 in its role) */
    {ENGI_JUNO, 5, 3254},                    /* SQUARE LEAD: -2.0 dB (measured -24.0 in its role) */
    {ENGI_DRUM, 2, 25844},                   /* SID KIT: +16.0 dB (measured -40.8 in its role; ~3 dB under, as UPRIGHT) */
    {ENGI_PHYS, PHYS_UPRIGHT, 43387},        /* UPRIGHT: +20.5 dB (measured -44.5 in its role; lands ~3 dB under: its
                                              * thump meets the master limiter, and more gain only squashes it) */
    {ENGI_EP, EP_WURLY, 12953},              /* WURLY: +10.0 dB (measured -37.2 in its role) */
    {ENGI_PHYS, PHYS_SURF, 6492},            /* SURF TWANG: +4.0 dB (measured -30.2 in its role) */
    {ENGI_CZ, 15, 2900},                     /* ELEC.BASS 2: -3.0 dB (measured -21.1 in its role) */
    {ENGI_WHEEL, WHEEL_MOD_HAMMOND, 2440},   /* MOD HAMMOND: -4.5 dB (measured -22.3 in its role) */
    {ENGI_DRUM, 1, 5462},                    /* 909 KIT: +2.5 dB (measured -27.7 in its role) */
    {ENGI_JUNO, 6, 10289},                   /* JUNO BASS: +8.0 dB (measured -32.2 in its role) */
    {ENGI_JUNO, 9, 8173},                    /* GLASS LEAD: +6.0 dB (measured -32.0 in its role) */
    {ENGI_DRUM, 3, 8657},                    /* SID DIGI: +6.5 dB (measured -31.4 in its role) */
};

static uint32_t dock_trim_q12(const track_t *t)
{
    uint32_t i;
    if (t->user || t->user_native)                      /* (a user preset: as its maker set it) */
        return 4096u;
    for (i = 0; i < sizeof DOCK_TRIMS / sizeof DOCK_TRIMS[0]; i++)
        if (DOCK_TRIMS[i].eng == t->engine && DOCK_TRIMS[i].preset == t->preset)
            return DOCK_TRIMS[i].q12;
    return 4096u;
}

/* the part's audio (and its width, spread) times its trim, kept within the mixer's input range */
static void dock_trim(const track_t *t, int32_t *b, int32_t *spread, uint32_t n, int spread_on)
{
    uint32_t g = dock_trim_q12(t), i;
    if (g == 4096u)
        return;
    for (i = 0; i < n; i++)
        b[i] = clamp((int32_t)(((int64_t)b[i] * g) >> 12), -884000, 884000);
    if (spread_on)
        for (i = 0; i < n; i++)
            spread[i] = clamp((int32_t)(((int64_t)spread[i] * g) >> 12), -884000, 884000);
}
