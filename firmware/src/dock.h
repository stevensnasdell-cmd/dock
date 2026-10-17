/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Dock's switches and the band's shape (DOCK.md §3). Included by core.h; each switch can be turned off (-DDOCK_...=0)
 * to get Melodee's behaviour back, for comparison and for Melodee's own host tests. */
#ifndef DOCK_H
#define DOCK_H

#ifndef DOCK_BAND
#define DOCK_BAND 1               /* the band: always four parts, Bass and Lead mono legato, the Yacht power-on sounds */
#endif
#ifndef DOCK_VOICES
#define DOCK_VOICES DOCK_BAND     /* voices by member (voice.c): shares, and the drummer's own lane */
#endif
#ifndef DOCK_ORGAN_PLUCK
#define DOCK_ORGAN_PLUCK DOCK_BAND /* WHEEL (the organ) and PHYS (PLUCK) back from Melodee's retired extras, without the
                                    * others (PHASE, VOICE, TRIO, NOISE stay out): core.h WHEEL_PHYS_ON */
#endif
#ifndef DOCK_SPRING
#define DOCK_SPRING DOCK_BAND     /* the effects' SPRING (fx.c, after Felucca's), appended as type 10: Hot's room */
#endif

/* the parts, in order: MIDI channels 1..4 by default (G_ROUTE CH1-4) */
enum { DOCK_BASS, DOCK_KEYS, DOCK_LEAD, DOCK_DRUMS };

/* each part's badge letter, where Melodee drew a track's numeral */
#if DOCK_BAND
#define DOCK_BADGE(k) ("BKLD"[(k) & 3u])
#else
#define DOCK_BADGE(k) ((char)('1' + (k)))
#endif

/* the engines a part may play: the drummer only drums, the band everything else (the same list on all three) */
#if DOCK_BAND
#define DOCK_ENGINE_OK(part, e) (((part) == DOCK_DRUMS) == ((e) == ENGI_DRUM))
#else
#define DOCK_ENGINE_OK(part, e) 1
#endif

/* Bass and Lead: the members that play one note at a time */
#define DOCK_MONO_PART(part) (DOCK_BAND && ((part) == DOCK_BASS || (part) == DOCK_LEAD))

/* Bass and Lead play MONO: every note strikes again (a pluck plucks, a filter snaps), and under DOCK_BAND their glide
 * moves only between overlapping notes (a sequencer's tie or slide: voice.c), as a player slides */
#define DOCK_MONO_MODE V_MONO

/* the voice mode a sound load sets (pr_mono: the preset's own MONO flag): Bass and Lead always mono legato */
#if DOCK_BAND
#define DOCK_LOAD_VOICE(part, pr_mono) \
    (((part) == DOCK_BASS || (part) == DOCK_LEAD) ? DOCK_MONO_MODE : (pr_mono) ? V_LEGATO : V_POLY)
#else
#define DOCK_LOAD_VOICE(part, pr_mono) ((pr_mono) ? V_LEGATO : V_POLY)
#endif

#endif
