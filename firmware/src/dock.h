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

/* the parts, in order: MIDI channels 1..4 by default (G_ROUTE CH1-4) */
enum { DOCK_BASS, DOCK_KEYS, DOCK_LEAD, DOCK_DRUMS };

/* each part's badge letter, where Melodee drew a track's numeral */
#if DOCK_BAND
#define DOCK_BADGE(k) ("BKLD"[(k) & 3u])
#else
#define DOCK_BADGE(k) ((char)('1' + (k)))
#endif

/* Bass and Lead: the members that play one note at a time */
#define DOCK_MONO_PART(part) (DOCK_BAND && ((part) == DOCK_BASS || (part) == DOCK_LEAD))

/* the voice mode a sound load sets (pr_mono: the preset's own MONO flag): Bass and Lead always mono legato */
#if DOCK_BAND
#define DOCK_LOAD_VOICE(part, pr_mono) \
    (((part) == DOCK_BASS || (part) == DOCK_LEAD) ? V_LEGATO : (pr_mono) ? V_LEGATO : V_POLY)
#else
#define DOCK_LOAD_VOICE(part, pr_mono) ((pr_mono) ? V_LEGATO : V_POLY)
#endif

#endif
