/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Dock browser bridge: the real firmware DSP behind a few plain functions, for WebAssembly
 * (emu/build.sh) and natively (the self-test). It reuses tests/hostsim.c, Melodee's host build of the
 * same sources, so the stubs that stand in for the hardware live in one place.
 *
 *   dock_init()                 power-on state: every part's default sound, MIDI route CH1-4
 *   dock_midi(status, d1, d2)   one MIDI message, through the firmware's own queue and router
 *   dock_render(frames)         renders frames (a multiple of 32) into dock_left() / dock_right()
 *   dock_voices(part)           sounding voices of part 0..3
 *   dock_rate()                 the device's sample rate (44,117.6 Hz, as 44118)
 *
 * DOCK_SELFTEST builds a main that plays a fixed MIDI script and writes raw float32 stereo to a file:
 * the native and WebAssembly builds must write identical bytes. */
#define main hostsim_main
#include "../tests/hostsim.c"
#undef main

#define DOCK_MAX_FRAMES 1024u

static float dock_l[DOCK_MAX_FRAMES], dock_r[DOCK_MAX_FRAMES];

void dock_init(void)
{
    uint32_t k;
    host_tracks_init();
    song.g[G_BPM] = 120;
    for (k = 0; k < NPART; k++)
        host_power_on_sound(&trk[k], k);
}

/* status byte + data, as a USB-MIDI packet (CIN = the status's upper nibble) from source 1 (USB) */
int dock_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    if (status < 0x80u || status >= 0xF0u)
        return 0;                                   /* channel messages only for now */
    return midi_enqueue((status >> 4) | status << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24, 1u);
}

/* renders in the firmware's own CTL-sample chunks; mix_block's Q15 stereo to float */
uint32_t dock_render(uint32_t frames)
{
    static int32_t o[2u * CTL];
    uint32_t f, i;
    if (frames > DOCK_MAX_FRAMES)
        frames = DOCK_MAX_FRAMES;
    frames -= frames % CTL;
    for (f = 0; f < frames; f += CTL) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            dock_l[f + i] = (float)o[2u * i] * (1.0f / 32768.0f);
            dock_r[f + i] = (float)o[2u * i + 1u] * (1.0f / 32768.0f);
        }
    }
    return frames;
}

float *dock_left(void) { return dock_l; }
float *dock_right(void) { return dock_r; }
uint32_t dock_rate(void) { return FS; }

uint32_t dock_voices(uint32_t part)
{
    uint32_t i, n = 0;
    if (part >= NPART)
        return 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[part].v[i].active;
    return n;
}

#ifdef DOCK_SELFTEST
/* a room on the shared effects (as dock_lineups.c sets one; this bridge has no UI to load a Line-up): SPLIT, three
 * slots {type, mix, rate, char}, the same sends on every part */
static void selftest_room(const int16_t fx[3][4], uint32_t send)
{
    song.g[G_FXMODE] = 1;
    song.g[G_FXRETURN] = 100;
    for (uint32_t s = 0; s < FX_SLOTS; s++) {
        for (uint32_t k = 0; k < FX_PARAMS; k++)
            song.g[G_MFXTYPE + s * FX_PARAMS + k] = fx[s][k];
        for (uint32_t k = 0; k < NPART; k++)
            trk[k].p[P_FXSEND + s] = (int16_t)send;
    }
}
/* bass riff on ch 1, a chord on ch 2, a lead note on ch 3, 4 s; three times: the power-on sounds (Yacht) in a plate
 * with a delay, then the organs (Mod's Keys and Lead: WHEEL MOD HAMMOND, VOX COMBO) in Goth's wash (reverb, delay,
 * chorus), then the plucks (Hot's Bass and Lead: PLUCK UPRIGHT, SURF TWANG) in Hot's spring and slapback */
int main(int argc, char **argv)
{
    static const uint8_t riff[8] = {36, 36, 48, 36, 39, 36, 43, 46};
    const uint32_t block = 128u, eighth = FS / 4u;
    uint32_t total = 4u * FS, pass;
    FILE *w = fopen(argc > 1 ? argv[1] : "selftest.f32", "wb");
    if (!w)
        return 1;
    dock_init();
    for (pass = 0; pass < 3u; pass++) {
        uint32_t f = 0, step = 0, held = 0;
        static const int16_t rooms[3][3][4] = {
            {{FX_REVERB, 127, 48, 66}, {FX_ECHO, 127, 103, 55}, {0}},
            {{FX_REVERB, 127, 127, 124}, {FX_ECHO, 127, 127, 100}, {FX_CHORUS, 127, 18, 110}},
            {{FX_SPRING, 127, 40, 112}, {FX_ECHO, 127, 36, 0}, {0}}};
        selftest_room(rooms[pass], 60);
        if (pass == 1u) {
            host_preset(&trk[DOCK_KEYS], ENGI_WHEEL, WHEEL_MOD_HAMMOND);
            host_preset(&trk[DOCK_LEAD], ENGI_WHEEL, WHEEL_VOX);
        } else if (pass == 2u) {
            host_preset(&trk[DOCK_BASS], ENGI_PHYS, PHYS_UPRIGHT);
            host_preset(&trk[DOCK_LEAD], ENGI_PHYS, PHYS_SURF);
        }
        if (pass)                                       /* (as a load sets them: dock.h DOCK_LOAD_VOICE) */
            trk[DOCK_BASS].p[P_VOICE] = trk[DOCK_LEAD].p[P_VOICE] = DOCK_MONO_MODE;
        while (f < total) {
            if (f >= step * eighth && step < 16u) {
                if (held)
                    dock_midi(0x80, held, 0);
                held = riff[step % 8u];
                dock_midi(0x90, held, 100);
                if (step == 0u || step == 8u) {
                    dock_midi(0x91, 60, 90); dock_midi(0x91, 63, 90); dock_midi(0x91, 67, 90);
                }
                if (step == 4u)
                    dock_midi(0x92, 72, 110);
                if (step == 7u) {
                    dock_midi(0x81, 60, 0); dock_midi(0x81, 63, 0); dock_midi(0x81, 67, 0);
                    dock_midi(0x82, 72, 0);
                }
                step++;
            }
            dock_render(block);
            for (uint32_t i = 0; i < block; i++) {
                fwrite(&dock_l[i], 4, 1, w);
                fwrite(&dock_r[i], 4, 1, w);
            }
            f += block;
        }
        if (held)
            dock_midi(0x80, held, 0);
        printf("selftest: pass %u, %u frames at %u Hz, voices now %u/%u/%u\n", pass, total, FS, dock_voices(0),
               dock_voices(1), dock_voices(2));
    }
    fclose(w);
    return 0;
}
#endif
