/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell */
/* Dock's common ENV and LFO, the "front door" (docs/design/DOCK_SIMPLIFY.md): page 1 of ENV (ATK DEC SUS REL) and of
 * LFO (RATE DEPTH) on every engine. Each is an offset, -64..63, that nudges the sound's OWN envelopes and LFOs from
 * where the sound has them; 0, the middle, is the sound as made (every engine renders exactly as without this file).
 * Nothing is overwritten: each engine reads its values through these offsets where it uses them (its effective copy:
 * the Prophet's value cache, FM6's macro patch, the CZ-1's envelope definitions; the others where they read the
 * value), so the engine's own pages keep the sound's values and still change it as before.
 *
 * What an offset means, the same everywhere:
 *   times (ATK DEC REL, RATE)  16 steps = twice as long (RATE: twice as fast), so the ends are x16 and /16, each
 *                              engine through its own law (fr_step: steps of its table per doubling; fr_mul: exact)
 *   levels (SUS, DEPTH)        + moves the level from where it is toward its top, - toward 0: at +63 the top, at -64
 *                              nothing (fr_lvl)
 *   DEPTH on a sound with no LFO at all: + brings in the engine's main LFO target (fr_lfo_main: vibrato on the part's
 *                              LFO, the EP's TREM, the engine's own LFO where it is routed)
 * A control that means nothing on an engine (an EP has no attack or sustain; a drum kit only decays) is greyed out on
 * the page (dock_front_live), never faked.
 *
 * Which envelopes and LFOs move, by engine (the wiring; job 3's Shape macro reuses it through fr_off):
 *   PROPHET  both envelopes (filter, amp: A D S R; the fixed release with RELEASE off too); its LFO's rate and
 *            initial amount
 *   FM6      every operator's EG (R1 attack, R2 R3 decay, L3 sustain, R4 release); its LFO's speed, PMD and AMD
 *   CZ-1     the DCW and DCA envelopes of both lines (stage 1 attack, the stages to the sustain point decay, its level
 *            the sustain, the stages after it release); its vibrato's rate and depth
 *   EP       DECAY and RELEASE (the tine's, the damper's); TREM's rate (the part's LFO) and depth
 *   organ    the part's ADSR, and DEC the percussion's decay; vibrato on the part's LFO
 *   PLUCK    the part's ADSR, and DEC the string's DAMP; the part's LFO
 *   JUNO, SID the part's ADSR (SID: the chip's envelope); the part's LFO
 *   DRUM     DEC only: the kit's DECAY
 * and on every engine the part's own LFO (LFO WAVE, LFO SYNC, LFO DEST: its rate and the four depths).
 *
 * The offsets are the part's, kept with a saved Line-up (dock_lineups.c), 0 again when a sound is loaded (its "as
 * made") or a factory Line-up. Included by engines.c, before the engines */
enum { FR_ATK, FR_DEC, FR_SUS, FR_REL, FR_RATE, FR_DEPTH, FR_N };
#define FR_MIN (-64)
#define FR_MAX 63
#include "dock_macro.c"                                 /* the macros: Shape adds its share to these (mc_fr) */

#if DOCK_BAND
static int16_t dock_front[NPART][FR_N];                 /* each part's offsets (the ENV / LFO pages write them) */
/* part t's offset k: what every engine reads, the ENV / LFO page's plus the Shape macro's share */
static inline int32_t fr_off(const track_t *t, uint32_t k)
{
    uint32_t i = (uint32_t)(t - trk);
    int32_t o;
    if (i >= NPART)
        return 0;
    o = dock_front[i][k % FR_N] + mc_fr(t, k % FR_N);
    return o < FR_MIN ? FR_MIN : o > FR_MAX ? FR_MAX : o;
}
static inline int fr_any(const track_t *t, uint32_t k0, uint32_t k1)   /* any of offsets k0..k1 off the middle */
{
    for (uint32_t k = k0; k <= k1; k++)
        if (fr_off(t, k))
            return 1;
    return 0;
}
#else
#define fr_off(t, k) 0
#define fr_any(t, k0, k1) 0
#endif

/* 2^(i/16) for i = -64..64, Q16 (index i + 64): the exact time law */
static const uint32_t FR_EXP2[129] = {
    4096, 4277, 4467, 4664, 4871, 5087, 5312, 5547, 5793, 6049, 6317, 6597,
    6889, 7194, 7512, 7845, 8192, 8555, 8933, 9329, 9742, 10173, 10624, 11094,
    11585, 12098, 12634, 13193, 13777, 14387, 15024, 15689, 16384, 17109, 17867, 18658,
    19484, 20347, 21247, 22188, 23170, 24196, 25268, 26386, 27554, 28774, 30048, 31379,
    32768, 34219, 35734, 37316, 38968, 40693, 42495, 44376, 46341, 48393, 50535, 52773,
    55109, 57549, 60097, 62757, 65536, 68438, 71468, 74632, 77936, 81386, 84990, 88752,
    92682, 96785, 101070, 105545, 110218, 115098, 120194, 125515, 131072, 136875, 142935, 149263,
    155872, 162773, 169979, 177505, 185364, 193571, 202141, 211090, 220436, 230195, 240387, 251030,
    262144, 273750, 285870, 298527, 311744, 325546, 339959, 355010, 370728, 387141, 404281, 422180,
    440872, 460391, 480774, 502060, 524288, 547500, 571740, 597053, 623487, 651091, 679917, 710020,
    741455, 774282, 808563, 844361, 881744, 920782, 961548, 1004120, 1048576,
};
/* x times 2^(off/16) (off -64..64): a time or a rate scaled exactly */
static inline float fr_mul(int32_t off) { return (float)FR_EXP2[clamp(off, -64, 64) + 64] * (1.0f / 65536.0f); }
static inline uint32_t fr_mulq(uint32_t x, int32_t off)
{
    return (uint32_t)(((uint64_t)x * FR_EXP2[clamp(off, -64, 64) + 64]) >> 16);
}
/* a value of an index law: off/16 doublings at spo32 / 32 steps per doubling (+ = more steps), rounded, in lo..hi */
static inline int32_t fr_step(int32_t v, int32_t off, int32_t spo32, int32_t lo, int32_t hi)
{
    int32_t d = off * spo32;
    if (!off)
        return v;                                       /* (the middle: the value itself, whatever it is) */
    d = d >= 0 ? (d + 256) >> 9 : -((-d + 256) >> 9);
    return clamp(v + d, lo, hi);
}
/* a level lo..hi moved by off: + toward hi (hi at 63), - toward lo (lo at -64) */
static inline int32_t fr_lvl(int32_t v, int32_t off, int32_t lo, int32_t hi)
{
    if (!off)
        return v;
    v = clamp(v, lo, hi);
    return off > 0 ? v + (hi - v) * off / FR_MAX : off < 0 ? v + (v - lo) * off / -FR_MIN : v;
}
static inline float fr_lvlf(float v, int32_t off, float hi)   /* .. a float level 0..hi */
{
    return off > 0 ? v + (hi - v) * (float)off * (1.0f / FR_MAX) : off < 0 ? v + v * (float)off * (1.0f / -FR_MIN) : v;
}
/* a bipolar depth (-64..63): its size moved as a level, its sign kept */
static inline int32_t fr_dep(int32_t v, int32_t off)
{
    return v >= 0 ? fr_lvl(v, off, 0, 63) : -fr_lvl(-v, off, 0, 64);
}

/* the steps per doubling of time (x32) of the tables the offsets move */
#define FR_SPO_ADSR 304                                 /* ENV_LIN / ENV_EXP, TIME_MS (x3.2 every 16): 9.5 */
#define FR_SPO_P5 304                                   /* the Prophet's P5_ENV_ATK / EXP: as the ADSR */
#define FR_SPO_LFO 416                                  /* LFO_INC, P5_LFO_INC (x2.3 every 16): 13 */
#define FR_SPO_DX 200                                   /* a DX7 EG rate (Dexed: x2 every 4 qrate, 64/41 a rate): 6.2 */
#define FR_SPO_EP 1024                                  /* the EP's DECAY / RELEASE: S = 2^((v - 64) / 32) */
#define FR_SPO_KIT 768                                  /* a drum kit's DECY (the 808's 2^(x / 24)) */
#define FR_SPO_PLUCK 256                                /* PLUCK's DAMP (measured: about 8 steps per doubling) */

/* the part's ADSR values (P_ATK .. P_REL) through the offsets: t->p itself at the middle, else q filled */
static const int16_t *fr_adsr(const track_t *t, int16_t *q)
{
    if (!fr_any(t, FR_ATK, FR_REL))
        return t->p;
    q[P_ATK] = (int16_t)fr_step(t->p[P_ATK], fr_off(t, FR_ATK), FR_SPO_ADSR, 0, 127);
    q[P_DEC] = (int16_t)fr_step(t->p[P_DEC], fr_off(t, FR_DEC), FR_SPO_ADSR, 0, 127);
    q[P_SUS] = (int16_t)fr_lvl(t->p[P_SUS], fr_off(t, FR_SUS), 0, 127);
    q[P_REL] = (int16_t)fr_step(t->p[P_REL], fr_off(t, FR_REL), FR_SPO_ADSR, 0, 127);
    return q;
}
/* the part's LFO rate (an LFO_INC index) */
static inline uint32_t fr_lrate(const track_t *t)
{
    return (uint32_t)fr_step(t->p[P_LRATE] & 127, fr_off(t, FR_RATE), FR_SPO_LFO, 0, 127);
}

/* control k (FR_*) means something on part t's engine; else the page greys it (and a page of none is not shown) */
static int dock_front_live(const track_t *t, uint32_t k)
{
    uint32_t e = t->eng_req % NENGINES;
    if (e == ENGI_DRUM)                                 /* a kit: only its decay */
        return k == FR_DEC;
    if (e == ENGI_EP)                                   /* a Rhodes: struck, no attack; it dies away, no sustain */
        return k != FR_ATK && k != FR_SUS;
    return k < FR_N;
}
