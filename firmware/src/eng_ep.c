/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Portions Copyright (C) 2026 Leonardo Mirabal (FRodhes: the voice model, rhodes.c, fxp.h) */
/* EP: Dock's electric piano (DOCK.md §4 "EP engine spec"): FRodhes' physically modelled Rhodes
 * (github.com/leonardoMirabal/frodhes, firmware/src/rhodes.c, IMPLEMENTATION-v3 §4.3, §5) as a Melodee engine.
 *
 * A voice (FRodhes, kept as it is): a hammer force pulse (shapes from a host contact simulation) drives the tine's
 * normal modes (V1 the tine-bar sustain mode exactly on f0, V2 the other tine-bar mode, O1 / O2 the tine's 2nd and
 * 3rd modes; H1 is reserved). Each mode is an exact-pole coupled-form resonator ("MC3") with first-order error
 * feedback. The tip displacement goes through the electromagnetic pickup: a radial flux table Phi(s),
 * s = (x + off)^2, by cubic Hermite, the output the flux difference (the bark: the tine swings across the pole).
 * Key-up brings the damper in (a crossfade of the modes' coefficients to their felt-damped pair), with a soft felt
 * knock and noise. The part's voices then pass the pickup load (an RLC low pass) and the TYPE's amp / speaker
 * (up to two biquads). All the physics is precomputed per key on the host: tools/gen_rhodes.py ->
 * build/gen/rhodes_tables.h (tools/rhodes/, from FRodhes).
 *
 * What Dock changed around it: the voices live in Melodee's allocator (voice_t) with their model state in the
 * part's engine state (ep_part_t), so voices by member, DOCK_ENGINE_OK and the shares apply; FRodhes' event ring is
 * gone (note_on, the gate and done() are Melodee's). The engine owns the amplitude (engine_t.ownenv, as FM6): the
 * ADSR does nothing, velocity is the hammer's, a voice ends when its modes have died away. A steal or a legato move
 * to another key fades the old note over 64 samples first (FRodhes' steal), the same key re-strikes the ringing
 * tine. Pitch: the key's tables give its tuning; bend, TUNE, glide, vibrato and A4 move the modes by up to about
 * +-4 semitones (the coefficient path's own small-offset series, FRodhes' TUNE path). The knobs' values are read at
 * each strike, as in FRodhes. The sustain pedal is Melodee's (it holds the gate): no half-pedal. FRodhes' DRIVE and
 * effects are not part of the engine (Dock's rooms come with the effects, DOCK.md §10 step 8).
 *
 * TYPE: FRodhes' four voicings (STAGE 77, SUITCASE, EARLY FELT, BELL DYNO) and Dock's WURLY (params.json: an
 * off-axis pickup, hard hammer, strong overtones, a nasal resonant load and amp; here also a symmetric soft clip
 * for odd harmonics). Not a reed and electrostatic pickup model: a Wurlitzer-ish voicing of the tine.
 * TREM: the part's LFO (its RATE and WAVE) as a Suitcase's stereo vibrato (the part plays in stereo, voice.c
 * trk_stereo) or, on WURLY, a mono tremolo. */
/* ---- generated-table types (must match tools/gen_rhodes.py) */
typedef struct { int32_t sh, ch, th, d, drel, cr, b; } rh_mode_t;
typedef struct {
    rh_mode_t m[5];
    int32_t off, gain, kill, hc, pk;
    uint16_t prate, life;
    uint8_t band, fam, res, pad;
} rh_key_t;
typedef struct {              /* physical voicing per TYPE (params.json "types"); STAGE 77 = all unity */
    int32_t off_delta;        /* Q25 gap: pickup offset change (+ = toward the pole axis) */
    int32_t bark, bark_g;     /* Q16: tip / gap scale (pickup distance) and its gain compensation */
    int32_t bell, ham;        /* Q16: overtone force scale, hammer contact-rate scale */
    uint8_t nbody, drive_row; /* body biquads in use (RH_BODY), the drive's row (FRodhes' DRIVE: not in Dock) */
} rh_type_t;
#include "rhodes_tables.h"     /* build/gen: tools/gen_rhodes.py */

/* ---- FRodhes' fixed-point helpers (fxp.h), named for this engine */
static inline int32_t rh_mul30(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 30); }
static inline int32_t rh_mul16(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 16); }
static inline int32_t rh_mul15s(int32_t a, int32_t b) { return (a * b) >> 15; }   /* |a|, |b| <= 32768 */
static inline int32_t rh_iabs(int32_t v) { return v < 0 ? -v : v; }
static inline int32_t rh_sat28(int64_t v)
{
    return v > ((1 << 28) - 1) ? ((1 << 28) - 1) : v < -((1 << 28) - 1) ? -((1 << 28) - 1) : (int32_t)v;
}

#define EP_POLY 8u
#define RH_V1 0u
#define RH_O1 2u
#define RH_O2 3u
#define RH_SUB 32u                             /* FRodhes' sub-block (Melodee's CTL) */
#define RH_RAMP_SUB 7u                         /* the damper's crossfade: 7 sub-blocks */
#define RH_FADE_SAMPLES 64u                    /* a steal's fade */
#define RH_Q30 (1 << 30)
#define RH_S_MAX_HI 2048u                      /* s >> 44 at 32 gap^2 */
#define RH_THUMP 3146                          /* FRodhes' NOISE at its default (48): KN_THUMP, KN_FELT */
#define RH_FELT 16
_Static_assert(CTL == RH_SUB, "EP: Melodee's control block is FRodhes' sub-block");
_Static_assert(FS == 44100, "EP: the tables are made for 44.1 kHz");
enum { EPT_STAGE, EPT_SUITCASE, EPT_FELT, EPT_DYNO, EPT_WURLY };
_Static_assert(RH_NTYPE == 5, "EP: TYPE names (N_EP_TYPE) and tools/gen_rhodes.py TYPE_NAMES");
static const char *const N_EP_TYPE[] = {"STAGE", "SUITCASE", "FELT", "DYNO", "WURLY"};

typedef struct {
    int32_t x, y, ex, ey;                      /* Q25 gap, MC3 states and error-feedback remainders */
    int32_t e, eps;                            /* live Q30 */
    int32_t e0, eps0, e1, eps1;                /* sustain pair, release pair */
    int32_t es, epss;                          /* values at the start of the current crossfade */
    int32_t b;                                 /* Q30 force (Q15) -> y (Q25), with the TYPE bell */
    int32_t peak;
    uint8_t live;
} rh_ms_t;

typedef struct {
    rh_ms_t m[RH_NM];
    const rh_key_t *k;
    uint8_t note, vel, alive, held;
    uint8_t ramp;                              /* damper crossfade sub-blocks left */
    uint8_t pend, pend_note, pend_vel;         /* a start waiting for the steal fade */
    int32_t w_tgt;                             /* damper weight Q15: 0 = undamped, 32768 = full felt */
    int32_t fade;                              /* Q15 output gain while fading out (32768 = not fading) */
    uint32_t life;                             /* control blocks left at most */
    int32_t Hprev, off, gain;
    uint32_t ppos, prate;
    int32_t pamp;
    const int16_t *ptab, *ptab2;               /* pulse shapes of the two velocity layers around vel */
    int32_t pmix;                              /* Q15 weight of ptab2 (layer crossfade) */
    int32_t felt_lp;
    uint16_t felt_n;                           /* felt-noise samples left (key-up) */
    uint16_t lfsr;
    int32_t t30;                               /* (Dock) the pitch offset the coefficients were made for, Q30 */
    int32_t k_dec, k_rel;                      /* the DECAY / RELEASE of its strike, Q16 (for a retune) */
} ep_voice_t;

typedef struct {
    ep_voice_t v[EP_POLY];
    int32_t rlc_m1, rlc_m2, rlc_o1, rlc_o2, rlc_e;
    int32_t bd_m[2][2], bd_o[2][2], bd_e[2];   /* the TYPE's body biquads */
    int32_t trem;                              /* the tremolo's gain at the end of the last block, Q15 */
    uint8_t clean;
} ep_part_t;
static ep_part_t *ep_part(uint32_t part);      /* engines.c: shared runtime pool */
static ep_voice_t *ep_voice(track_t *t, voice_t *v)
{
    ep_part_t *P = ep_part((uint32_t)(t - trk) % NPART);
    return P ? &P->v[(uint32_t)(v - t->v) % EP_POLY] : 0;
}
static uint32_t ep_type(const track_t *t) { return (uint32_t)clamp(t->p[P_E6], 0, RH_NTYPE - 1); }

/* ---- run-time coefficient path (§5.6): (E, eps) from (sin, cos of theta/2, theta, d, cr), offset t, decay k */
static void rh_coef(const rh_mode_t *m, int32_t d30, int32_t t30, int32_t k16, int32_t *E, int32_t *eps)
{
    int32_t a = (int32_t)(((int64_t)k16 * (k16 - 65536)) >> 16);          /* k(k-1), Q16 */
    int32_t c = (int32_t)(((int64_t)a * (k16 - 131072)) >> 16);           /* k(k-1)(k-2), Q16 */
    int32_t d2 = rh_mul30(d30, d30), d3 = rh_mul30(d2, d30);
    int64_t dp = (((int64_t)k16 * d30) >> 16) - ((((int64_t)a * d2) >> 16) >> 1)
               + (int32_t)(((int64_t)c * d3) >> 16) / 6;
    int32_t dpi = dp < 1 ? 1 : (int32_t)dp;
    int32_t dp2 = rh_mul30(dpi, dpi), dp3 = rh_mul30(dp2, dpi);
    int32_t q = RH_Q30 - (dpi >> 2) - 3 * (dp2 >> 5);
    int32_t om = (dpi >> 1) + (dp2 >> 3) + (dp3 >> 4);
    int32_t dl = rh_mul30(m->th, t30);
    int32_t dl2 = rh_mul30(dl, dl), dl3 = rh_mul30(dl2, dl), dl4 = rh_mul30(dl2, dl2), dl5 = rh_mul30(dl4, dl);
    int32_t cdl = RH_Q30 - (dl2 >> 1) + dl4 / 24, sdl = dl - dl3 / 6 + dl5 / 120;
    int32_t sp = rh_mul30(m->sh, cdl) + rh_mul30(m->ch, sdl);
    int32_t cr = m->cr - 2 * rh_mul30(m->cr, t30) + 3 * rh_mul30(m->cr, rh_mul30(t30, t30));
    int32_t corr = RH_Q30 + rh_mul16(rh_mul30(om, om), cr);
    int32_t t1 = (int32_t)(((int64_t)q * sp) >> 29);                      /* 2 q sin(theta'/2), Q30 */
    *E = rh_mul30(t1, corr);
    *eps = dpi;
}

/* the knobs' curves (FRodhes' gen_fx_tables.py KN_*), Q16: 2^((v - 64) / div) */
static int32_t ep_knob(int32_t v, float div)
{
    return (int32_t)(65536.0f * fm_exp2f((float)(clamp(v, 0, 127) - 64) / div) + 0.5f);
}

/* ---- voices */
static void rh_load_pulse(ep_voice_t *v, uint32_t vel, int32_t ham)
{
    uint32_t key = (uint32_t)(v->k - RH_KEYS), i = RH_VPOS_I[vel], f = RH_VPOS_F[vel];
    const uint16_t *pa = RH_PAMP[key];
    const uint32_t *pr = RH_PRATE[key];
    v->ptab = RH_PULSE[key][RH_VLAY_I[vel]];
    v->ptab2 = RH_PULSE[key][RH_VLAY_I[vel] + 1u < RH_NLAYER ? RH_VLAY_I[vel] + 1u : RH_NLAYER - 1u];
    v->pmix = RH_VLAY_W[vel];
    v->pamp = (int32_t)pa[i] + ((((int32_t)pa[i + 1] - (int32_t)pa[i]) * (int32_t)f) >> 8);
    v->prate = (uint32_t)((int32_t)pr[i] + ((((int32_t)pr[i + 1] - (int32_t)pr[i]) * (int32_t)f) >> 8)) << 4;  /* Q12 -> Q16 */
    {   /* HAMMER knob x TYPE tips scale the contact rate; the force scales with it so the impulse (the hammer's
         * momentum) is kept: a softer tip pushes longer and less hard, it does not give more energy */
        uint32_t h = (uint32_t)ham;
        v->prate = (uint32_t)(((uint64_t)v->prate * h) >> 16);
        v->pamp = (int32_t)(((int64_t)v->pamp * h) >> 16);
        if (v->pamp > 32767) v->pamp = 32767;                                 /* mul15s range */
    }
    v->ppos = 0;
}

static int32_t rh_hermite(const int32_t *P, const int32_t *M, int64_t s)
{
    uint32_t i, t;
    int32_t p0, p1, m0, m1, c2, c3, a;
    if (s < 0) s = 0;
    if ((uint64_t)s >= ((uint64_t)RH_S_MAX_HI << 44)) s = ((int64_t)RH_S_MAX_HI << 44) - 1;
    i = (uint32_t)(s >> 44);
    t = (uint32_t)(s >> 22) & 0x3FFFFFu;
    p0 = P[i]; p1 = P[i + 1]; m0 = M[i]; m1 = M[i + 1];
    c3 = 2 * (p0 - p1) + m0 + m1;
    c2 = 3 * (p1 - p0) - 2 * m0 - m1;
    a = c2 + (int32_t)(((int64_t)t * c3) >> 22);
    a = m0 + (int32_t)(((int64_t)t * a) >> 22);
    return p0 + (int32_t)(((int64_t)t * a) >> 22);
}

/* the HAMMER knob x the TYPE's tips, Q16 */
static int32_t ep_ham(const track_t *t)
{
    return (int32_t)(((uint64_t)(uint32_t)ep_knob(t->p[P_E3], 128.0f) * (uint32_t)RH_TYPE[ep_type(t)].ham) >> 16);
}

/* a new strike of note on a voice at rest (or whose fade ended): FRodhes' rh_init, the knobs from the track */
static void rh_init(const track_t *t, ep_voice_t *v, uint32_t note, uint32_t vel)
{
    const rh_type_t *ty = &RH_TYPE[ep_type(t)];
    uint32_t m, n = note < RH_KEY_LO ? RH_KEY_LO : note > RH_KEY_LO + RH_NKEY - 1u ? RH_KEY_LO + RH_NKEY - 1u : note;
    const rh_key_t *k = &RH_KEYS[n - RH_KEY_LO];
    int32_t bark_s = ep_knob(t->p[P_E1], 64.0f), bark_g = (int32_t)(4294967296.0f / (float)bark_s + 0.5f);
    int32_t bell = ep_knob(t->p[P_E2], 32.0f), u;
    v->k = k;
    v->k_dec = (int32_t)RH_DECAY_K[fr_step(clamp(t->p[P_E4], 0, 127), fr_off(t, FR_DEC), FR_SPO_EP, 0, 127)];
    v->k_rel = (int32_t)RH_DECAY_K[fr_step(clamp(t->p[P_E5], 0, 127), fr_off(t, FR_REL), FR_SPO_EP, 0, 127)];
                                                       /* k = 1/S, S = 2^((v-64)/32): higher = longer (Dock's common
                                                        * DEC / REL nudge them, dock_front.c) */
    v->t30 = 0;
    for (m = 0; m < RH_NM; m++) {
        rh_ms_t *s = &v->m[m];
        const rh_mode_t *km = &k->m[m];
        s->x = s->y = s->ex = s->ey = 0;
        s->peak = 0;
        s->live = km->th != 0;
        if (!s->live) { s->e = s->eps = 0; continue; }
        rh_coef(km, km->d, 0, v->k_dec, &s->e0, &s->eps0);
        rh_coef(km, km->drel, 0, v->k_rel, &s->e1, &s->eps1);
        s->e = s->es = s->e0; s->eps = s->epss = s->eps0;
        {   /* force gain: BARK scales the tip (the pickup then barks), BELL the overtones; saturating */
            int64_t b = ((((int64_t)km->b * bark_s) >> 16) * ty->bark) >> 16;
            if (m == RH_O1 || m == RH_O2) b = (((b * ty->bell) >> 16) * bell) >> 16;
            s->b = b > 0x7FFFFFFF ? 0x7FFFFFFF : b < -0x7FFFFFFF ? -0x7FFFFFFF : (int32_t)b;
        }
    }
    v->note = (uint8_t)note; v->vel = (uint8_t)vel;
    v->off = k->off + ty->off_delta + t->p[P_E0] * (1 << 18);      /* TONE: +-0.5 gap */
    v->gain = rh_mul16(rh_mul16(k->gain, bark_g), ty->bark_g);
    u = v->off;
    v->Hprev = rh_hermite(RH_PHI[k->fam], RH_SLOPE[k->fam], (int64_t)u * u);
    v->alive = 1; v->held = 1; v->ramp = 0; v->w_tgt = 0;
    v->fade = 32768; v->pend = 0; v->felt_n = 0; v->felt_lp = 0;
    v->life = (uint32_t)k->life * (256u / RH_SUB);
    if (!v->lfsr) v->lfsr = 0xACE1u;
    rh_load_pulse(v, vel, ep_ham(t));
}

static void rh_set_damper(ep_voice_t *v, int32_t w)
{
    uint32_t m;
    if (w == v->w_tgt) return;
    for (m = 0; m < RH_NM; m++) { v->m[m].es = v->m[m].e; v->m[m].epss = v->m[m].eps; }
    v->w_tgt = w;
    v->ramp = RH_RAMP_SUB;
}

/* a key on this voice (note on, or a legato move): the same key still ringing re-strikes its tine (the strikes
 * superpose), another key fades the old note out first (FRodhes' rh_strike) */
static void ep_strike(track_t *t, ep_voice_t *s, uint32_t note, uint32_t vel)
{
    vel = vel < 1u ? 1u : vel > 127u ? 127u : vel;
    if (s->alive && !s->pend && s->note == note && s->fade == 32768) {
        uint32_t m;
        for (m = 0; m < RH_NM; m++) if (s->k->m[m].th) s->m[m].live = 1;
        s->vel = (uint8_t)vel; s->held = 1; s->life = (uint32_t)s->k->life * (256u / RH_SUB);
        rh_set_damper(s, 0);
        rh_load_pulse(s, vel, ep_ham(t));
        return;
    }
    if (!s->alive) { rh_init(t, s, note, vel); return; }
    s->pend = 1; s->pend_note = (uint8_t)note; s->pend_vel = (uint8_t)vel;
    if (s->fade == 32768) s->fade = 32767;
}

static void ep_note_on(track_t *t, voice_t *v)
{
    ep_voice_t *s = ep_voice(t, v);
    if (!s) return;
    if (!voice_was) s->alive = 0;              /* a free voice: whatever its state held has ended */
    ep_strike(t, s, v->note, v->vel);
}
static void ep_legato(track_t *t, voice_t *v)  /* MONO / LEGATO (Dock's Bass and Lead): a piano strikes again */
{
    ep_voice_t *s = ep_voice(t, v);
    if (s) ep_strike(t, s, v->note, v->mvel ? v->mvel : v->vel);
}
static int ep_done(track_t *t, voice_t *v)
{
    ep_voice_t *s = ep_voice(t, v);
    return !s || !s->alive;
}

/* (Dock) the voice's pitch against its key's tuning: TUNE, A4, bend, glide, vibrato, UNISON's detune. The
 * coefficients follow when it moves (the series of rh_coef holds to about +-4 semitones: clamped there) */
static void ep_retune(ep_voice_t *v, const vmod_t *mo)
{
    uint32_t key = (uint32_t)(v->k - RH_KEYS) + RH_KEY_LO, m;
    float r = (float)mo->inc / (float)pitch_inc((int32_t)key * 16) - 1.0f;
    int32_t t30 = (int32_t)(fm_clampf(r, -0.25f, 0.25f) * 1073741824.0f), d = t30 - v->t30;
    if (d < 120000 && d > -120000)             /* (~0.2 cent) */
        return;
    v->t30 = t30;
    for (m = 0; m < RH_NM; m++) {
        rh_ms_t *s = &v->m[m];
        const rh_mode_t *km = &v->k->m[m];
        if (!km->th) continue;
        rh_coef(km, km->d, t30, v->k_dec, &s->e0, &s->eps0);
        rh_coef(km, km->drel, t30, v->k_rel, &s->e1, &s->eps1);
        if (!v->ramp) {                        /* (a crossfade under way takes them from here) */
            s->e = s->e0 + (int32_t)(((int64_t)(s->e1 - s->e0) * v->w_tgt) >> 15);
            s->eps = s->eps0 + (int32_t)(((int64_t)(s->eps1 - s->eps0) * v->w_tgt) >> 15);
        }
    }
}

/* one voice, one 32-frame sub-block, into o[] (Q20, FRodhes' mix units) */
static void rh_voice_sub(ep_voice_t *v, int32_t *o)
{
    const rh_key_t *k = v->k;
    const int32_t *P = RH_PHI[k->fam], *M = RH_SLOPE[k->fam];
    int32_t xs[RH_SUB], F[RH_SUB];             /* (on the stack: the parts may render on two cores) */
    uint32_t j, m;
    int32_t sum = 0, Hp = v->Hprev, off = v->off, gain = v->gain;
    if (v->ramp) {                                        /* damper crossfade: E and eps together */
        int32_t f;
        v->ramp--;
        f = (int32_t)(RH_RAMP_SUB - v->ramp) * 4681;      /* Q15 of i/7 */
        if (f > 32768) f = 32768;
        if (!v->ramp) f = 32768;
        for (m = 0; m < RH_NM; m++) {
            rh_ms_t *s = &v->m[m];
            int32_t et, pt;
            if (!s->live) continue;
            et = s->e0 + (int32_t)(((int64_t)(s->e1 - s->e0) * v->w_tgt) >> 15);
            pt = s->eps0 + (int32_t)(((int64_t)(s->eps1 - s->eps0) * v->w_tgt) >> 15);
            s->e = s->es + (int32_t)(((int64_t)(et - s->es) * f) >> 15);
            s->eps = s->epss + (int32_t)(((int64_t)(pt - s->epss) * f) >> 15);
        }
    }
    for (j = 0; j < RH_SUB; j++) {                        /* hammer force for the sub-block */
        if (v->ptab) {
            uint32_t i = v->ppos >> 16;
            int32_t f = (int32_t)((v->ppos >> 1) & 0x7FFFu);
            int32_t a = v->ptab[i], b = v->ptab[i + 1];
            if (v->pmix) {
                a += (int32_t)(((int64_t)(v->ptab2[i] - a) * v->pmix) >> 15);
                b += (int32_t)(((int64_t)(v->ptab2[i + 1] - b) * v->pmix) >> 15);
            }
            F[j] = rh_mul15s(a + (int32_t)(((int64_t)(b - a) * f) >> 15), v->pamp);
            v->ppos += v->prate;
            if ((v->ppos >> 16) >= RH_PLEN) v->ptab = 0;
        } else
            F[j] = 0;
        xs[j] = 0;
    }
    for (m = 0; m < RH_NM; m++) {                         /* modes: MC3 + error feedback, mode-outer */
        rh_ms_t *s = &v->m[m];
        int32_t x, y, ex, ey, e, eps, b, pk = 0;
        if (!s->live) continue;
        x = s->x; y = s->y; ex = s->ex; ey = s->ey; e = s->e; eps = s->eps; b = s->b;
        for (j = 0; j < RH_SUB; j++) {
            int64_t t;
            if (F[j]) y += (int32_t)(((int64_t)F[j] * b) >> 20);
            t = (int64_t)e * y + ex;
            x += (int32_t)(t >> 30); ex = (int32_t)(t & 0x3FFFFFFF);
            t = -(int64_t)eps * y - (int64_t)e * x + ey;
            y += (int32_t)(t >> 30); ey = (int32_t)(t & 0x3FFFFFFF);
            xs[j] += x;
            if (rh_iabs(x) > pk) pk = rh_iabs(x);
            if (rh_iabs(y) > pk) pk = rh_iabs(y);
        }
        s->x = rh_sat28(x); s->y = rh_sat28(y); s->ex = ex; s->ey = ey; s->peak = pk;
        sum += pk >> 3;
    }
    for (j = 0; j < RH_SUB; j++) {                        /* pickup, then the per-key gain */
        int32_t u = xs[j] + off;
        int32_t H = rh_hermite(P, M, (int64_t)u * u);
        int32_t q = (int32_t)(((int64_t)(H - Hp) * gain) >> 24);
        Hp = H;
        if (v->fade != 32768) {
            q = (int32_t)(((int64_t)q * v->fade) >> 15);
            v->fade -= (int32_t)(32768 / RH_FADE_SAMPLES);
            if (v->fade < 0) v->fade = 0;
        }
        if (v->felt_n) {                                  /* felt noise: LFSR, one-pole ~2 kHz */
            int32_t nz;
            v->lfsr = (uint16_t)((v->lfsr >> 1) ^ (-(v->lfsr & 1u) & 0xB400u));
            nz = (int32_t)v->lfsr - 32768;
            v->felt_lp += (nz - v->felt_lp) >> 3;
            q += (int32_t)(((int64_t)v->felt_lp * RH_FELT) >> 10);   /* Q15 * Q15 >> 10 = Q20 */
            v->felt_n--;
        }
        o[j] = q;
    }
    v->Hprev = Hp;
    if (v->fade == 0) {                                   /* a steal finished fading: the waiting note may start */
        v->alive = 0;
        return;
    }
    if (!v->ptab && !v->felt_n && sum < (k->kill >> 3)) { v->alive = 0; return; }
    for (m = 1; m < RH_NM; m++)                           /* sleep overtone modes that have died away */
        if (v->m[m].live && !v->ptab && v->m[m].peak < (k->kill >> 3)) {
            v->m[m].live = 0; v->m[m].x = v->m[m].y = v->m[m].ex = v->m[m].ey = 0;
        }
}

static void ep_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    ep_voice_t *s = ep_voice(t, v);
    int32_t o[RH_SUB];
    if (!s || n != RH_SUB) return;
    if (!s->alive && s->pend) {                           /* (a fade that ended in its last block) */
        s->pend = 0;
        rh_init(t, s, s->pend_note, s->pend_vel);
    }
    if (!s->alive) return;
    if (!v->gate && s->held) {                            /* key up (Melodee holds the gate under the pedal) */
        s->held = 0;
        rh_set_damper(s, 32768);
        if (!s->ptab) {                                   /* the felt lands on the tine: a soft knock */
            uint32_t key = (uint32_t)(s->k - RH_KEYS);
            s->ptab = RH_PULSE[key][0];
            s->pmix = 0;
            s->pamp = RH_THUMP;
            s->prate = (uint32_t)RH_PRATE[key][1] << 4;
            s->ppos = 0;
        }
        s->felt_n = 353;                                  /* 8 ms of felt noise */
    }
    ep_retune(s, m);
    rh_voice_sub(s, o);
    for (uint32_t i = 0; i < n; i++) {                    /* Q20 -> Q15 (FRodhes rounds after its filters) */
        int32_t q = clamp((o[i] + 16) >> 5, -65535, 65535);
        out[i] += voice_amp(q, m, i) << 1;
    }
    if (!s->alive && s->pend) {                           /* the steal's fade is done: the new strike */
        s->pend = 0;
        rh_init(t, s, s->pend_note, s->pend_vel);
    } else if (s->alive && s->life && --s->life == 0)
        s->alive = 0;
}

/* ---------------------------------------------------------------------------------- the part --- */
/* the TREM: a Suitcase's stereo vibrato (pans with the LFO) on every TYPE but WURLY, whose tremolo is mono */
static int32_t ep_trem(const track_t *t)               /* TREM, through Dock's common DEPTH (dock_front.c) */
{
    return fr_lvl(clamp(t->p[P_E7], 0, 127), fr_off(t, FR_DEPTH), 0, 127);
}
static int ep_stereo(const track_t *t) { return ep_trem(t) > 0 && ep_type(t) != EPT_WURLY; }

static int32_t *spread_buf;                            /* voice.c: the left channel's buffer while the part is stereo */
static void ep_post(track_t *t, int32_t *out, uint32_t n, uint32_t nr)
{
    ep_part_t *P = ep_part((uint32_t)(t - trk) % NPART);
    uint32_t ty = ep_type(t), nb = RH_TYPE[ty].nbody, j, sb;
    const int32_t *c = RH_RLC[ty];
    int32_t d = ep_trem(t) * 258, l0, l1, wurly = ty == EPT_WURLY, stereo = ep_stereo(t);
    if (!P) return;
    if (!nr) {                                         /* silence: the filters at rest */
        if (!P->clean) {
            P->rlc_m1 = P->rlc_m2 = P->rlc_o1 = P->rlc_o2 = P->rlc_e = 0;
            memset(P->bd_m, 0, sizeof P->bd_m); memset(P->bd_o, 0, sizeof P->bd_o); memset(P->bd_e, 0, sizeof P->bd_e);
            P->clean = 1;
        }
        return;
    }
    P->clean = 0;
    l1 = t->lfo_val;                                   /* the LFO, ramped over the block from where it was */
    l0 = P->trem;
    P->trem = l1;
    for (j = 0; j < n; j++) {     /* pickup load: RLC low-pass on the mix (Q28 DF1, error feedback) */
        int32_t x = out[j], y;
        int64_t acc = (int64_t)c[0] * x + (int64_t)c[1] * P->rlc_m1 + (int64_t)c[2] * P->rlc_m2
                    - (int64_t)c[3] * P->rlc_o1 - (int64_t)c[4] * P->rlc_o2 + P->rlc_e;
        y = (int32_t)(acc >> 28); P->rlc_e = (int32_t)(acc & 0xFFFFFFF);
        P->rlc_m2 = P->rlc_m1; P->rlc_m1 = x; P->rlc_o2 = P->rlc_o1; P->rlc_o1 = y;
        for (sb = 0; sb < nb; sb++) {                  /* the instrument's amp / speaker (Q28 DF1) */
            const int32_t *b = RH_BODY[ty][sb];
            int64_t a2 = (int64_t)b[0] * y + (int64_t)b[1] * P->bd_m[sb][0] + (int64_t)b[2] * P->bd_m[sb][1]
                       - (int64_t)b[3] * P->bd_o[sb][0] - (int64_t)b[4] * P->bd_o[sb][1] + P->bd_e[sb];
            int32_t z = (int32_t)(a2 >> 28);
            P->bd_e[sb] = (int32_t)(a2 & 0xFFFFFFF);
            P->bd_m[sb][1] = P->bd_m[sb][0]; P->bd_m[sb][0] = y; P->bd_o[sb][1] = P->bd_o[sb][0]; P->bd_o[sb][0] = z;
            y = z;
        }
        if (wurly)                                     /* (Dock) WURLY: the amp's symmetric clip, odd harmonics */
            y = softclip(clamp((y * 3) >> 2, -1 << 22, 1 << 22)) * 2;
        if (d) {
            int32_t l = l0 + (((l1 - l0) * (int32_t)j) >> 5), g;   /* (n = 32) */
            if (stereo) {                              /* left = (1 + depth lfo) / 2, right the rest */
                g = 16384 + (int32_t)(((int64_t)d * l) >> 16);
                spread_buf[j] = (int32_t)(((int64_t)y * g) >> 15);
            } else {                                   /* mono: down to 1 - depth at the LFO's bottom */
                g = 32768 - (int32_t)(((int64_t)d * ((32767 - l) >> 1)) >> 15);
                y = (int32_t)(((int64_t)y * g) >> 15);
            }
        }
        out[j] = y;
    }
}

/* ---------------------------------------------------------------------------------------- presets --- */
typedef struct {
    const char *name;
    int8_t e[8];                                       /* TONE BARK BELL HAMMER DECAY RELEASE TYPE TREM */
    uint8_t lrate;                                     /* the TREM's speed (LFO RATE: 85 ~ 4.5 Hz) */
} ep_patch_t;
static const ep_patch_t EP_PATCH[] = {
    /* name             TONE BARK BELL HAMR DCAY REL TYPE TREM  RATE */
    {"INIT EP",        {  0,  64,  64,  64,  64,  64,  0,   0}, 85},
    {"SUITCASE",       { -4,  64,  60,  64,  64,  64,  1,  70}, 85},
    {"STAGE 77",       {  2,  72,  64,  64,  64,  64,  0,   0}, 85},
    {"EARLY FELT",     { -6,  60,  64,  64,  72,  64,  2,  30}, 81},
    {"BELL DYNO",      {  6,  70,  84,  70,  64,  64,  3,  40}, 85},
    {"WURLY",          {  0,  84,  64,  76,  40,  52,  4,  44}, 89},
    {"BARK STAGE",     {  6, 104,  64,  92,  58,  64,  0,   0}, 85},
    {"BALLAD CASE",    {-14,  50,  48,  56,  84,  72,  1,  48}, 78},
};
#define EP_NPRESETS NELEM(EP_PATCH)
#define EP_SUITCASE 1u                                 /* Dock's Keys at power-on (engines.c TRK_DEF, Yacht) */
#define EP_WURLY 5u                                    /* Hot's Keys (dock_lineups.c) */

static void ep_preset_make(uint32_t k, preset_t *pr, char *name)
{
    static const uint8_t env[4] = {0, 64, 127, 40};    /* (the ADSR does nothing here: engine_t.ownenv) */
    const ep_patch_t *ep = &EP_PATCH[k % EP_NPRESETS];
    memcpy(pr->e, ep->e, sizeof pr->e);
    memcpy(pr->env, env, sizeof pr->env);
    pr->fenv = 0;
    pr->mono = 0;
    str_cpy(name, ep->name, 21);
}
/* an EP sound loaded (ui.c apply_preset_to, after the common values went to their defaults): the TREM's LFO */
static void ep_preset_loaded(track_t *t, uint32_t pi)
{
    t->p[P_LWAVE] = 0;                                 /* SIN */
    t->p[P_LRATE] = EP_PATCH[pi % EP_NPRESETS].lrate;
}

/* Host measurement (tests/ep_test.c: an EP voice against a PROPHET and a JUNO voice, the same notes): about 0.45
 * of a PROPHET voice, 0.7 of a JUNO voice; two units, as JUNO (its 64-bit multiplies cost more on the FM-1's 32-bit
 * core than on a computer). To be confirmed on the device (Phase 2). */
static uint32_t ep_units(const track_t *t) { (void)t; return 2u; }

static const engine_t ENG_EP = {
    .name = "EP",
    .page_title = {"TINE", "BODY"},
    .edit = {
        {"TONE", F_BIPCT, -64, 63, 0, 0, 0},
        {"BARK", F_PCT, 0, 127, 64, 0, 0},
        {"BELL", F_PCT, 0, 127, 64, 0, 0},
        {"HAMR", F_PCT, 0, 127, 64, 0, 0},              /* HAMMER */
        {"DECAY", F_PCT, 0, 127, 64, 0, 0},
        {"RELEASE", F_PCT, 0, 127, 64, 0, 0},
        {"TYPE", F_ENUM, 0, RH_NTYPE - 1, 0, N_EP_TYPE, 0},
        {"TREM", F_PCT, 0, 127, 0, 0, 0},
    },
    .preset_make = ep_preset_make,
    .npresets = EP_NPRESETS,
    .note_on = ep_note_on,
    .render = ep_render,
    .ownenv = 1,
    .done = ep_done,
    .legato = ep_legato,
    .post = ep_post,
    .knob = {P_E0, P_E1, P_E4, P_E7},                  /* Tone, Bite (bark), Shape (decay), Space (trem) (DOCK.md §6) */
    .poly = EP_POLY,
    .units = ep_units,
};
