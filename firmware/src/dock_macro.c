/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell */
/* Dock's four macros (DOCK.md §6): HOME's KNOB 1-4, Tone · Bite · Shape · Space, on every engine. Each is an offset,
 * -64..63, from the sound as made (0, the middle: every engine renders exactly as without this file). Like the common
 * ENV / LFO (dock_front.c) nothing is overwritten: each engine reads its values through the offsets where it uses
 * them, so its own pages keep the sound's values and still change it.
 *
 * What each moves, by engine (the shares below; DOCK.md §6's table):
 *            Tone                     Bite                        Shape (via dock_front.c)    Space
 *   JUNO     cutoff                   resonance, PWM              attack, release             the room sends
 *   PROPHET  cutoff                   resonance, poly-mod         filter env amount; decay,   the room sends
 *                                                                 release (both envelopes)
 *   EP       TONE (the pickup)        BARK                        decay (and release)         TREM depth, the sends
 *   organ    the drawbars' tilt       drive                       percussion decay            rotor slow / fast, sends
 *            (top up, sub down)                                                               (past ±32; a rotary on)
 *   PLUCK    brightness               pluck position              decay (DAMP)                the room sends
 *   FM6      brightness (mod index)   feedback                    decay, release              the room sends
 *   CZ-1     DCW depth                the DCW envelope's sweep    decay, release              the room sends
 *   SID      cutoff                   resonance, pulse width      decay, release              the room sends
 *   DRUM     TONE                     drive (DRV)                 decay (DECY)                the room sends
 * Laws: a level (resonance, drive, bark, the sends) moves toward its top (+) or toward nothing (-), as dock_front.c's
 * fr_lvl; a filter moves in steps (Tone: about +-4 octaves at the ends); Shape is dock_front.c's time law (16 steps =
 * twice as long), added to the ENV page's own offsets in fr_off.
 *
 * The offsets are the part's: 0 when a sound loads (browsing starts neutral), back with Undo, kept with a saved
 * Line-up (dock_lineup_t.macro) and 0 on a factory one. HOME's knobs turn them for the selected member (PART) or for
 * the band (BAND: Bass, Keys and Lead each from its own value, and the drummer's Space), ui_dock.c dock_macro_turn.
 * Included by dock_front.c (before the engines). */
enum { MC_TONE, MC_BITE, MC_SHAPE, MC_SPACE, MC_N };
#define MC_MIN (-64)
#define MC_MAX 63

#if DOCK_BAND
static int16_t dock_macro[NPART][MC_N];                 /* each part's Tone, Bite, Shape, Space (HOME's knobs) */
static inline int32_t mc_off(const track_t *t, uint32_t k)
{
    uint32_t i = (uint32_t)(t - trk);
    return i < NPART ? dock_macro[i][k % MC_N] : 0;
}
static inline int mc_any(const track_t *t)
{
    uint32_t i = (uint32_t)(t - trk);
    return i < NPART && (dock_macro[i][0] | dock_macro[i][1] | dock_macro[i][2] | dock_macro[i][3]);
}
#else
#define mc_off(t, k) 0
#define mc_any(t) 0
#endif

/* Shape's share of the common ENV offset k (dock_front.c FR_*): the envelope times it lengthens on part t's engine */
static inline int32_t mc_fr(const track_t *t, uint32_t k)
{
    int32_t s = mc_off(t, MC_SHAPE);
    if (!s)
        return 0;
    switch (t->eng_req % NENGINES) {
    case ENGI_JUNO:                                     /* attack and release */
        return k == FR_ATK || k == FR_REL ? s : 0;
    case ENGI_EP:                                       /* the tine's decay; its release half as much */
        return k == FR_DEC ? s : k == FR_REL ? s / 2 : 0;
    case ENGI_WHEEL:                                    /* the percussion's decay */
    case ENGI_PHYS:                                     /* the string's damping */
    case ENGI_DRUM:                                     /* the kit's DECY */
        return k == FR_DEC ? s : 0;
    default:                                            /* PROPHET (with its filter env amount), FM6, CZ-1, SID */
        return k == FR_DEC || k == FR_REL ? s : 0;
    }
}

/* a level v in lo..hi moved by macro k of part t (v itself at 0) */
static inline int32_t mc_lvl(const track_t *t, uint32_t k, int32_t v, int32_t lo, int32_t hi)
{
    int32_t o = mc_off(t, k);
    if (!o)
        return v;
    v = v < lo ? lo : v > hi ? hi : v;
    return o > 0 ? v + (hi - v) * o / MC_MAX : v + (v - lo) * o / -MC_MIN;
}
/* a value v moved by macro k scaled num / den, kept in lo..hi (v itself at 0) */
static inline int32_t mc_add(const track_t *t, uint32_t k, int32_t v, int32_t num, int32_t den, int32_t lo, int32_t hi)
{
    int32_t o = mc_off(t, k);
    if (!o)
        return v;
    v += o * num / den;
    return v < lo ? lo : v > hi ? hi : v;
}

/* Tone and Bite on the voice's modulation (voice.c, vmod_t: cutoff and shape, Q8 steps), for the engines that read
 * them as their filter and their pulse width / feedback; the others take theirs where they read the value */
static inline int32_t mc_cutoff(const track_t *t)
{
    int32_t o = mc_off(t, MC_TONE);
    if (!o)
        return 0;
    switch (t->eng_req % NENGINES) {
    case ENGI_JUNO:                                     /* semitones: +-48 */
    case ENGI_PHYS:                                     /* BRIT steps: +-48 */
    case 3u:                                            /* SID: its FILT steps, +-48 */
        return o * 192;
    case ENGI_FM6:                                      /* the modulators' index, about +-21 of its TONE */
        return o * 128;
    case 15u:                                           /* CZ-1 (cz_patch.h ENGI_CZ): the DCW depth */
        return o * 96;
    default:                                            /* (PROPHET: in its value cache; EP, organ, DRUM: their own) */
        return 0;
    }
}
static inline int32_t mc_shape(const track_t *t)
{
    int32_t o = mc_off(t, MC_BITE);
    if (!o)
        return 0;
    switch (t->eng_req % NENGINES) {
    case ENGI_JUNO:                                     /* PWM: +-0.25 of the width (with the resonance) */
    case 3u:                                            /* SID: the pulse width (with the resonance) */
        return o * 64;
    case ENGI_FM6:                                      /* the feedback: +-8, its whole range */
        return o * 256;
    default:
        return 0;
    }
}

/* Space: a room send amount (P_FXSEND..P_FXSEND3, 0..127) as heard */
static inline int32_t mc_send(const track_t *t, int32_t v) { return mc_lvl(t, MC_SPACE, v, 0, 127); }
