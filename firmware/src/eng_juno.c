/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell */
/* JUNO: a Juno-60-style voice and its chorus (DOCK.md §4 "JUNO engine spec"). Written for Dock; behaviour after the
 * published Juno-60 analyses (Andy Harman's Juno60 notes and JunoX: reference only, no code taken).
 *
 * A voice: one DCO (polyBLEP sawtooth, pulse with PWM, a square sub an octave down, white noise, a level each) ->
 * the fixed high pass (the Juno-60's four positions: off, 154, 339, 720 Hz, 6 dB/oct) -> a 4-pole resonant low pass
 * (zero-delay-feedback ladder, its feedback through a soft saturator: resonance thins the bass and sings at the top)
 * -> the VCA. Float, as the Prophet.
 *
 * The track supplies the rest, as for every engine: one ADSR for VCF and VCA (ENV), ENV -> FILTER (ENV DEST FLT, +-:
 * the Juno's polarity switch), the LFO with its fade-in delay (LFO RATE, FADE), LFO -> DCO pitch, VCF and pulse
 * width (LFO DEST PIT, FLT, SHP), key tracking through the matrix (a sound sets slot 4 KEY -> CUT: 64 = 100 %, one
 * CUT step a semitone), glide. VCA GATE (HPF "0 GT".."3 GT"): the VCA follows the key, the ENV moves the filter only
 * (a voice still ends with the ADSR, so GATE wants SUS above 0). DRIFT: the track's DTUNE (VOICE 2), outside UNISON a
 * per-note offset of up to +-8 cents and a cutoff offset of up to a semitone, as a Juno's voice cards differ.
 *
 * CHORUS (EDIT 2): the Juno-60's: the part's voices summed into one bucket-brigade-like line (a 12 dB low pass
 * before it, one after each tap), read by two taps a triangle LFO sweeps in opposite directions. I: 0.513 Hz, II:
 * 0.863 Hz, both 1.66 .. 5.35 ms, stereo (left: dry + tap 1, right: dry + tap 2); I+II: 9.75 Hz over 3.3 .. 3.7 ms,
 * mono. One 256-sample float line per part (1 KB). The part plays in stereo while I or II is on (voice.c
 * trk_stereo). */

#define JUNO_POLY 8u
#define JUNO_DL 256u                                   /* the chorus line: 5.8 ms at 44.1 kHz */
_Static_assert(FS == 44100, "JUNO: the chorus and the filter assume 44.1 kHz");

static const char *const N_JUNO_WAVE[] = {"SAW", "PULSE", "SAW+PLS", "OFF"};
static const char *const N_JUNO_HPF[] = {"0", "1", "2", "3", "0 GT", "1 GT", "2 GT", "3 GT"};
static const char *const N_JUNO_CHORUS[] = {"OFF", "I", "II", "I+II"};
enum { JW_SAW, JW_PULSE, JW_BOTH, JW_OFF };
enum { JC_OFF, JC_I, JC_II, JC_BOTH };

typedef struct {
    float z[4];                                        /* the ladder's four integrators */
    float zh;                                          /* the high pass's */
    float gate;                                        /* VCA GATE's level (0..1) */
    uint32_t noise;                                    /* the noise generator */
    int32_t drift;                                     /* DRIFT: the pitch offset (1/4096, as vmod_t.fine) */
    float drift_cut;                                   /* .. and the cutoff's (semitones) */
    uint8_t sub;                                       /* the sub square's state (flips as the DCO wraps) */
} juno_voice_t;
typedef struct {
    juno_voice_t v[JUNO_POLY];
    float dl[JUNO_DL];                                 /* the chorus line */
    float pre[2], post[2];                             /* its low passes: 12 dB in, one pole after each tap */
    uint32_t w, lfo;                                   /* write position; the chorus LFO's phase */
    uint8_t mode, clean;                               /* the chorus mode last run; the line is all zero */
} juno_part_t;
static juno_part_t *juno_part(uint32_t part);          /* engines.c: shared runtime pool */
static juno_voice_t *juno_voice(track_t *t, voice_t *v)
{
    juno_part_t *jp = juno_part((uint32_t)(t - trk) % NPART);
    return jp ? &jp->v[(uint32_t)(v - t->v) % JUNO_POLY] : 0;
}
static uint32_t juno_chorus(const track_t *t) { return (uint32_t)t->p[P_E7] & 3u; }
static int juno_gate_mode(const track_t *t) { return t->p[P_E4] >= 4; }

/* the high pass's TPT coefficient G = g / (1 + g), g = tan(pi f / FS), for 154, 339, 720 Hz (0: off) */
static const float JUNO_HPF_G[4] = {0.0f, 0.010852f, 0.023580f, 0.048830f};

static inline __attribute__((always_inline)) float juno_blep(float t, float dt)   /* polyBLEP residual */
{
    if (t < dt) { float x = t / dt; return x + x - x * x - 1.0f; }
    if (t > 1.0f - dt) { float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
    return 0.0f;
}
static inline __attribute__((always_inline)) float juno_sat(float x)              /* soft saturation, |y| <= 1 */
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static void juno_note_on(track_t *t, voice_t *v)
{
    juno_voice_t *s = juno_voice(t, v);
    if (!s) return;
    if (!voice_was) {                                  /* a free voice: everything at rest */
        memset(s, 0, sizeof *s);
        s->noise = 0x6A09E667u + v->age * 2654435761u + (uint32_t)(v - t->v) * 40503u;
    }
    if (t->p[P_VOICE] != V_UNISON) {                   /* DRIFT (DTUNE): this note's own small offsets */
        int32_t r = v->mrnd, d = t->p[P_DETUNE];       /* (mod.c RAND of the note: -32768..32767) */
        s->drift = (r * d) / 75000;                    /* up to ~ +-8 cents at 127 (1 ct = 2.367 / 4096) */
        s->drift_cut = ((float)(((uint32_t)r * 7919u) & 0xFFFFu) * (1.0f / 65536.0f) - 0.5f) * (float)d * (1.0f / 127.0f);
    } else {
        s->drift = 0;
        s->drift_cut = 0.0f;
    }
}

static void juno_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    juno_voice_t *s = juno_voice(t, v);
    const int16_t *p = t->p;
    if (!s) return;
    uint32_t wave = (uint32_t)p[P_E0] & 3u, hpf = (uint32_t)p[P_E4] & 3u, ph = v->ph[0];
    uint32_t inc = m->inc + (uint32_t)((int32_t)(m->inc >> 12) * s->drift);
    float dt = (float)inc * (1.0f / 4294967296.0f);
    float saw_l = wave == JW_SAW || wave == JW_BOTH ? 1.0f : 0.0f;
    float pls_l = wave == JW_PULSE || wave == JW_BOTH ? 1.0f : 0.0f;
    float sub_l = (float)clamp(p[P_E2], 0, 127) * (1.0f / 127.0f);
    float nse_l = (float)clamp(p[P_E3], 0, 127) * (1.0f / 127.0f) * 0.8f;
    /* PW 0 = square, 127 = a thin pulse; SHP modulation (LFO / ENV -> SHP) folds around the square */
    float pw = (float)p[P_E1] * (1.0f / 127.0f) + (float)(m->shape - (64 << 8)) * (1.0f / (64.0f * 256.0f));
    pw = fm_minf(fm_fabsf(pw), 1.0f);
    float width = 0.5f - 0.46f * pw, dc = 2.0f * width - 1.0f;
    /* CUT in semitones (0 = 26 Hz, MIDI note 20); ENV, LFO, the matrix (KEY) add theirs (m->cutoff: Q8 steps) */
    float cut = (float)p[P_E5] + (float)m->cutoff * (1.0f / 256.0f) + s->drift_cut;
    float fc = 440.0f * fm_exp2f((cut + 20.0f - 69.0f) * (1.0f / 12.0f));
    fc = fm_clampf(fc, 10.0f, 0.45f * (float)FS);
    float g = fm_tanf(3.14159265f * fc / (float)FS), G = g / (1.0f + g), B = 1.0f - G;
    float G2 = G * G, G4 = G2 * G2;
    float r = (float)mc_lvl(t, MC_BITE, clamp(p[P_E6], 0, 127), 0, 127) * (1.0f / 127.0f), k = 4.2f * r;   /* (Bite) */
    float inv = 1.0f / (1.0f + k * G4), comp = 1.0f + 0.35f * k;   /* (some of the bass resonance takes back) */
    float Gh = JUNO_HPF_G[hpf];
    float z0 = s->z[0], z1 = s->z[1], z2 = s->z[2], z3 = s->z[3], zh = s->zh, sub = s->sub ? 1.0f : -1.0f;
    uint32_t noise = s->noise;
    /* VCA GATE: the key's own level (velocity, the LFO / matrix gain left out), ramped over the block */
    int gate = juno_gate_mode(t);
    float g0 = s->gate, g1 = v->gate ? 1.0f : g0 * 0.55f;
    float gv = (float)v->vel * (1.0f / 127.0f);
    s->gate = g1 < 1e-4f ? 0.0f : g1;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t prev = ph;
        ph += inc;
        if (ph < prev)                                 /* the DCO wrapped: the sub square flips */
            sub = -sub;
        float tt = (float)ph * (1.0f / 4294967296.0f), x = 0.0f;
        float bl = juno_blep(tt, dt);
        if (saw_l != 0.0f)
            x += saw_l * (tt + tt - 1.0f - bl);
        if (pls_l != 0.0f) {                           /* up at the wrap, down at the width */
            float tw = tt - width;
            tw += tw < 0.0f ? 1.0f : 0.0f;
            x += pls_l * ((tt < width ? 1.0f : -1.0f) + bl - juno_blep(tw, dt) - dc);
        }
        if (sub_l != 0.0f)                             /* its edges sit on the DCO's wrap */
            x += sub_l * (sub + (tt < dt ? sub * bl : -sub * bl));
        if (nse_l != 0.0f) {
            noise = noise * 1664525u + 1013904223u;
            x += nse_l * (float)(int32_t)noise * (1.0f / 2147483648.0f);
        }
        x *= 0.5f;
        if (Gh != 0.0f) {                              /* HPF: x minus its one-pole low pass */
            float hv = (x - zh) * Gh, hl = hv + zh;
            zh = hl + hv;
            x -= hl;
        }
        /* the ladder: the linear estimate of its output gives the feedback; the input then saturates */
        float S = B * (G * (G2 * z0 + G * z1 + z2) + z3);
        float y4 = (G4 * x * comp + S) * inv;
        float u = juno_sat(x * comp - k * y4), yv;
        yv = (u - z0) * G; float y1 = yv + z0; z0 = y1 + yv;
        yv = (y1 - z1) * G; float y2 = yv + z1; z1 = y2 + yv;
        yv = (y2 - z2) * G; float y3 = yv + z2; z2 = y3 + yv;
        yv = (y3 - z3) * G; float y = yv + z3; z3 = y + yv;
        int32_t o = (int32_t)fm_clampf(y * 32767.0f, -32767.0f, 32767.0f);
        if (gate) {
            float a = (g0 + (g1 - g0) * (float)i * (1.0f / (float)CTL)) * gv;
            out[i] += mulq15((int32_t)((float)o * a), VOICE_FS) << 1;
        } else
            out[i] += voice_amp(o, m, i) << 1;
    }
    v->ph[0] = ph;
    s->z[0] = z0; s->z[1] = z1; s->z[2] = z2; s->z[3] = z3; s->zh = zh;
    s->sub = sub > 0.0f;
    s->noise = noise;
}

/* ----------------------------------------------------------------------------------------- chorus --- */
/* delay centre and sweep (samples) and the LFO's increment per sample of each mode */
static const float JUNO_CH_C[4] = {0.0f, 154.4f, 154.4f, 77.2f};   /* (1.66 + 5.35) / 2 ms; (3.3 + 3.7) / 2 */
static const float JUNO_CH_A[4] = {0.0f, 81.4f, 81.4f, 4.4f};      /* (5.35 - 1.66) / 2 ms; (3.7 - 3.3) / 2 */
static const uint32_t JUNO_CH_INC[4] = {0u, 49960u, 84045u, 949565u};   /* 0.513, 0.863, 9.75 Hz: f / FS * 2^32 */

/* I and II: the part is stereo (voice.c renders it with spread_buf, mix.c mixes it full width) */
static int juno_stereo(const track_t *t) { uint32_t c = juno_chorus(t); return c == JC_I || c == JC_II; }

static int32_t *spread_buf;                            /* voice.c: the left channel's buffer while the part is stereo */
static void juno_post(track_t *t, int32_t *out, uint32_t n, uint32_t nr)
{
    juno_part_t *jp = juno_part((uint32_t)(t - trk) % NPART);
    uint32_t mode = juno_chorus(t);
    if (!jp) return;
    if (!nr || !mode || mode != jp->mode) {            /* silence, OFF or a new mode: an empty line */
        if (!jp->clean) {
            memset(jp->dl, 0, sizeof jp->dl);
            jp->pre[0] = jp->pre[1] = jp->post[0] = jp->post[1] = 0.0f;
            jp->clean = 1;
        }
        jp->mode = (uint8_t)mode;
        if (!nr || !mode) return;
    }
    jp->clean = 0;
    const float c = JUNO_CH_C[mode], a = JUNO_CH_A[mode], kin = 0.72f, kout = 0.76f;   /* ~9 and ~10 kHz */
    const uint32_t li = JUNO_CH_INC[mode];
    float p0 = jp->pre[0], p1 = jp->pre[1], q0 = jp->post[0], q1 = jp->post[1];
    uint32_t w = jp->w, lfo = jp->lfo;
    int stereo = mode != JC_BOTH;
    for (uint32_t i = 0; i < n; i++) {
        float x = (float)out[i];
        p0 += kin * (x - p0);
        p1 += kin * (p0 - p1);
        jp->dl[w] = p1;
        lfo += li;                                     /* triangle, -1..1 */
        float tri = (float)(int32_t)(lfo ^ (uint32_t)((int32_t)lfo >> 31)) * (1.0f / 1073741824.0f) - 1.0f;
        float d0 = c + a * tri, d1 = c - a * tri, r0, r1;
        {
            float pos = (float)w - d0 + (float)JUNO_DL;
            uint32_t i0 = (uint32_t)pos;
            float f = pos - (float)i0;
            float s0 = jp->dl[i0 & (JUNO_DL - 1u)], s1 = jp->dl[(i0 + 1u) & (JUNO_DL - 1u)];
            r0 = s0 + (s1 - s0) * f;
        }
        q0 += kout * (r0 - q0);
        if (stereo) {
            float pos = (float)w - d1 + (float)JUNO_DL;
            uint32_t i0 = (uint32_t)pos;
            float f = pos - (float)i0;
            float s0 = jp->dl[i0 & (JUNO_DL - 1u)], s1 = jp->dl[(i0 + 1u) & (JUNO_DL - 1u)];
            r1 = s0 + (s1 - s0) * f;
            q1 += kout * (r1 - q1);
            float l = 0.62f * (x + q0), rr = 0.62f * (x + q1);
            spread_buf[i] = (int32_t)l;
            out[i] = (int32_t)(l + rr);
        } else
            out[i] = (int32_t)(0.62f * (x + q0));
        w = (w + 1u) & (JUNO_DL - 1u);
    }
    jp->pre[0] = p0; jp->pre[1] = p1; jp->post[0] = q0; jp->post[1] = q1;
    jp->w = w; jp->lfo = lfo;
}

/* ---------------------------------------------------------------------------------------- presets --- */
typedef struct {
    const char *name;
    int8_t e[8];                                       /* WAVE PW SUB NOISE HPF CUT RES CHORUS */
    uint8_t env[4];                                    /* ATK DEC SUS REL */
    int8_t fenv;                                       /* ENV -> FILTER */
    uint8_t mono;
    uint8_t lrate, lfade;                              /* LFO: RATE, FADE (the Juno's DELAY) */
    int8_t lpit, lflt, lshp;                           /* LFO -> DCO, VCF, PW */
    int8_t kybd;                                       /* KEY -> CUT (matrix slot 4), 64 = 100 % */
    uint8_t glide, drift;                              /* GLD; DTUNE (DRIFT) */
} juno_patch_t;
static const juno_patch_t JUNO_PATCH[] = {
    /* name           WAVE PW SUB NSE HPF CUT RES CHO   ATK DEC SUS REL  FENV MONO RATE FADE PIT FLT SHP KYBD GLD DRIFT */
    {"INIT JUNO",    {0,   0,  0,  0,  0, 127,  0, 0}, {0,  70, 127, 40},  0, 0,  60,   0,  0,  0,  0,   0,  0, 40},
    {"SILK LEAD",    {2,  40, 30,  0,  1,  72, 24, 1}, {20, 85, 100, 70}, 20, 1,  88,  90,  1,  0, 18,  32, 45, 40},
    {"STR MACHINE",  {1,  20,  0,  6,  2,  96,  0, 3}, {72, 90, 112, 82},  0, 0,  47,   0,  0,  0, 44,  40,  0, 50},
    {"STRING WASH",  {2,  30, 20,  0,  1,  80, 12, 2}, {98,100, 118,104},  6, 0,  34,   0,  0,  4, 36,  32,  0, 70},
    {"WIDE PAD",     {0,   0, 50,  0,  0,  66, 30, 2}, {90,105, 110,100}, 14, 0,  30,   0,  0,  8,  0,  24,  0, 60},
    {"SQUARE LEAD",  {1,   0,  0,  0,  1, 100,  8, 0}, {8,  70, 120, 40},  0, 1,  89,  95,  1,  0,  0,  64, 70, 30},
    {"JUNO BASS",    {0,   0,110,  0,  0,  34, 40, 0}, {0,  66,  50, 34}, 36, 1,  60,   0,  0,  0,  0,  40,  0, 20},
    {"JUNO BRASS",   {0,   0, 30,  0,  1,  46, 14, 1}, {44, 80,  96, 56}, 34, 0,  88,  95,  1,  0,  0,  40,  0, 40},
    {"GATE ORGAN",   {1,  24, 64,  0,  6,  70, 10, 2}, {0,  62,  30, 30}, 28, 0,  60,   0,  0,  0,  0,  64,  0, 30},
    {"GLASS LEAD",   {1,  90,  0,  0,  3, 110, 30, 2}, {10, 90, 100, 96},  8, 1,  86, 100,  1,  0, 10,  48, 30, 30},
};
#define JUNO_NPRESETS NELEM(JUNO_PATCH)
#define JUNO_SILK_LEAD 1u                              /* Dock's Lead at power-on (engines.c TRK_DEF) */

static void juno_preset_make(uint32_t k, preset_t *pr, char *name)
{
    const juno_patch_t *jp = &JUNO_PATCH[k % JUNO_NPRESETS];
    memcpy(pr->e, jp->e, sizeof pr->e);
    memcpy(pr->env, jp->env, sizeof pr->env);
    pr->fenv = jp->fenv;
    pr->mono = jp->mono;
    str_cpy(name, jp->name, 21);
}
/* a JUNO sound loaded (ui.c apply_preset_to, after the common values went to their defaults): the LFO, its routes,
 * key tracking, glide and drift of the sound */
static void juno_preset_loaded(track_t *t, uint32_t pi)
{
    const juno_patch_t *jp = &JUNO_PATCH[pi % JUNO_NPRESETS];
    t->p[P_LWAVE] = 1;                                 /* the Juno's LFO: a triangle */
    t->p[P_LRATE] = jp->lrate;
    t->p[P_LFADE] = jp->lfade;
    t->p[P_LD_PIT] = jp->lpit;
    t->p[P_LD_FLT] = jp->lflt;
    t->p[P_LD_SHP] = jp->lshp;
    t->p[P_GLIDE] = jp->glide;
    t->p[P_DETUNE] = jp->drift;
    if (jp->kybd) {
        t->p[P_M4SRC] = 4;                             /* MS_KEY (mod.c, after us: checked there) */
        t->p[P_M4DST] = 2;                             /* MD_CUT */
        t->p[P_M4AMT] = jp->kybd;
    }
}

/* Host measurement (tests/juno_test.c: a JUNO voice against a PROPHET voice, the same notes) puts a voice at about
 * two Prophet voices' worth less: two units against the Prophet's three. To be confirmed on the device (Phase 2). */
static uint32_t juno_units(const track_t *t) { (void)t; return 2u; }

static const engine_t ENG_JUNO = {
    .name = "JUNO",
    .page_title = {"DCO", "VCF"},
    .edit = {
        {"WAVE", F_ENUM, 0, 3, 0, N_JUNO_WAVE, 0},
        {"PW", F_PCT, 0, 127, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"NOISE", F_PCT, 0, 127, 0, 0, 0},
        {"HPF", F_ENUM, 0, 7, 0, N_JUNO_HPF, 0},
        {"CUT", F_PCT, 0, 127, 127, 0, 0},
        {"RES", F_PCT, 0, 127, 0, 0, 0},
        {"CHOR", F_ENUM, 0, 3, 0, N_JUNO_CHORUS, 0},
    },
    .preset_make = juno_preset_make,
    .npresets = JUNO_NPRESETS,
    .note_on = juno_note_on,
    .render = juno_render,
    .post = juno_post,
    .knob = {P_E5, P_E6, P_REL, P_E7},                 /* Tone, Bite, Shape, Space (DOCK.md §6) */
    .poly = JUNO_POLY,
    .units = juno_units,
};
