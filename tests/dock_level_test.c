/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Dock's balance (firmware/src/dock_trim.c): every Line-up sound, played alone in its role at LEVEL's default, lands
 * near its role's loudness (Bass -24, Keys -27, Lead -26, Drums -25 dB RMS), within 3.5 dB (the percussive ones sit a
 * little under: the master limiter takes their attacks). Dry: each Line-up's room (its shared effects' return) is off
 * here, so the trims stay the sounds' own; tests/dock_room_test.c checks the rooms. Runs the emulator's firmware build
 * (emu/dock_web.c). */
#include "../emu/dock_web.c"
static void run_ms(uint32_t ms){uint32_t e=fm1_ms+ms;while((int32_t)(fm1_ms-e)<0)web_render(128);}
/* play part k alone for 4 s in its role and return the RMS in dBFS (device level) */
static double play(uint32_t k){
  static const uint8_t riff[8]={36,36,43,36,39,36,41,43}, lead[8]={72,74,75,79,77,75,74,72}, kick[16]={36,42,38,42,36,36,38,42,36,42,38,42,36,42,38,46};
  double acc=0; uint32_t n=0, f, step=0, held=0, ch=k;
  for (f=0; f<4u*44100u; f+=128u){
    if (f >= step*(44100u/4u) && step < 16u){
      if (k==DOCK_KEYS){ if (step%8u==0){ static const uint8_t c[2][4]={{60,64,67,71},{57,60,64,67}}; if(step){for(int i=0;i<4;i++)web_midi(0x80|ch,c[0][i],0);} for(int i=0;i<4;i++)web_midi(0x90|ch,c[step/8][i],90);} }
      else { if (held) web_midi(0x80|ch,held,0); held = k==DOCK_BASS?riff[step%8]:k==DOCK_LEAD?lead[step%8]:kick[step]; web_midi(0x90|ch,held,100); }
      step++;
    }
    web_render(128);
    for (int i=0;i<128;i++){ acc += (double)out_l[i]*out_l[i] + (double)out_r[i]*out_r[i]; n+=2; }
  }
  for (int nn=0; nn<128; nn++) web_midi(0x80|ch, nn, 0);
  run_ms(1500);
  return 10.0*log10(acc/n + 1e-12);
}
/* part k's pitch error in cents on a held note (autocorrelation; checked at 60 and 72; drums: none) */
static double tune_cents(uint32_t k, uint32_t note)
{
    static float x[22050];
    uint32_t n = 0, l, best_l = 0;
    double best = 0, hz, want = 440.0 * pow(2.0, ((double)note - 69.0) / 12.0), c;
    web_midi(0x90 | k, note, 100);
    run_ms(150);                                        /* (past the attack) */
    while (n < 22050u) {
        web_render(128);
        for (uint32_t i = 0; i < 128u && n < 22050u; i++) x[n++] = out_l[i] + out_r[i];
    }
    web_midi(0x80 | k, note, 0);
    run_ms(1200);
    for (l = 44100u / 1000u; l < 44100u / 40u; l++) {
        double s = 0;
        for (uint32_t i = 0; i + l < 22050u; i++) s += (double)x[i] * x[i + l];
        if (s > best) { best = s; best_l = l; }
    }
    hz = 44100.0 / best_l;
    c = 1200.0 * log2(hz / want);
    while (c > 600) c -= 1200;                          /* (an octave either way is the same note) */
    while (c < -600) c += 1200;
    return c;
}

int main(void){ web_boot(); run_ms(300);
  static const char *R[4]={"Bass","Keys","Lead","Drums"};
  static const double T[4]={-24,-27,-26,-25};
  int bad=0;
  for (uint32_t i=0;i<DOCK_NLINEUPS;i++){
    dock_lineup_load(i); song.g[G_FXRETURN] = 0; run_ms(300);   /* (dry: the room's return faded out) */
    printf("%-6s", DOCK_LINEUPS[i].name);
    for (uint32_t k=0;k<NPART;k++){ char nm[24]; int pd; double db; dk_name(k,nm,sizeof nm,&pd); db=play(k);
      int ok = fabs(db - T[k]) <= 3.5; bad += !ok; printf("  %s %-14s %6.1f%s|", R[k], nm, db, ok ? " " : "!"); }
    printf("\n");
  }
  for (uint32_t i=0;i<DOCK_NLINEUPS;i++){                /* in tune: every melodic Line-up sound within 25 cents */
    dock_lineup_load(i);
    for (uint32_t k=0;k<NPART;k++) for (uint32_t f=0;f<FX_SLOTS;f++) trk[k].p[P_FXSEND+f]=0;   /* (dry: a room blurs pitch) */
    run_ms(1500);
    printf("%-6s tuning", DOCK_LINEUPS[i].name);
    for (uint32_t k=0;k<DOCK_DRUMS;k++){ char nm[24]; int pd; double c = (tune_cents(k,60) + tune_cents(k,72)) / 2;   /* (mid notes: a rich low note fools autocorrelation) */
      int ok = fabs(c) <= 25.0; bad += !ok; dk_name(k,nm,sizeof nm,&pd); printf("  %s %-14s %+4.0fc%s|", R[k], nm, c, ok ? " " : "!"); }
    printf("\n");
  }
  printf(bad ? "dock balance: %d sounds off their role's level or out of tune\n" : "dock balance: all passed\n", bad);
  return bad != 0;
}
