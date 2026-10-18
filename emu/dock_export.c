/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * A Line-up as a small readable text file (JSON), the emulator's "Export Line-up": everything a record holds
 * (firmware/src/dock_lineups.c dock_lineup_t), and a few names for people reading it (engine_name, sound, member, the
 * effects' names), which tools/dock_lineup_import.py ignores. That tool turns such a file into a factory Line-up
 * (firmware/lineups/<name>.dock -> firmware/src/dock_factory.h). Format 1:
 *   dock_lineup 1, name, p_count (P_COUNT: the params' count), voices [Bass, Keys, Lead, drummer's lane],
 *   room {mode 0 CHAIN / 1 SPLIT, return, fx: three {type, mix, rate, char, tone}}, macros [4][4], groove [4],
 *   parts: four {engine, preset, params [p_count], patch: the engine's own patch, hex in lines of 32 bytes ([] none)}
 * Included by emu/dock_web.c (host and wasm only: not in the device's image). */
#include <stdarg.h>
#include <stdio.h>

static uint32_t dx_n, dx_max;
static char *dx_o;
static void dx_put(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void dx_put(const char *fmt, ...)
{
    va_list ap;
    int n;
    if (dx_n >= dx_max)
        return;
    va_start(ap, fmt);
    n = vsnprintf(dx_o + dx_n, dx_max - dx_n, fmt, ap);
    va_end(ap);
    dx_n = n < 0 ? dx_max : dx_n + (uint32_t)n > dx_max ? dx_max : dx_n + (uint32_t)n;
}
static void dx_str(const char *s, uint32_t max)        /* a JSON string (names are printable ASCII) */
{
    uint32_t i;
    dx_put("\"");
    for (i = 0; i < max && s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\')
            dx_put("\\%c", c);
        else if (c >= 32u && c < 127u)
            dx_put("%c", c);
        else
            dx_put("\\u%04x", c);
    }
    dx_put("\"");
}

/* r -> o (max bytes, 0-terminated); its length (max - 1: it did not fit). sound[k]: part k's sound's name (0: none) */
static uint32_t dock_lineup_json(const dock_lineup_t *r, char *const sound[NPART], char *o, uint32_t max)
{
    static const char *const MEMBER[NPART] = {"Bass", "Keys", "Lead", "Drums"};
    uint32_t k, i, s;
    dx_o = o;
    dx_n = 0;
    dx_max = max ? max - 1u : 0u;
    dx_put("{\n  \"dock_lineup\": 1,\n  \"name\": ");
    dx_str(r->name, DOCK_NAME);
    dx_put(",\n  \"p_count\": %u,\n  \"voices\": [%u, %u, %u, %u],\n", (unsigned)P_COUNT, r->share[0], r->share[1],
           r->share[2], r->share[3]);
    dx_put("  \"room\": {\"mode\": %u, \"return\": %u, \"fx\": [\n", r->room.mode, r->room.ret);
    for (s = 0; s < FX_SLOTS; s++) {
        const dock_fx_t *f = &r->room.fx[s];
        dx_put("    {\"type\": %u, \"name\": \"%s\", \"mix\": %u, \"rate\": %u, \"char\": %u, \"tone\": %d}%s\n", f->type,
               f->type < NELEM(N_FX) ? N_FX[f->type] : "?", f->mix, f->rate, f->chr, f->tone, s + 1u < FX_SLOTS ? "," : "");
    }
    dx_put("  ]},\n  \"macros\": [");
    for (k = 0; k < NPART; k++)
        dx_put("[%d, %d, %d, %d]%s", r->macro[k][0], r->macro[k][1], r->macro[k][2], r->macro[k][3],
               k + 1u < NPART ? ", " : "");
    dx_put("],\n  \"groove\": [%u, %u, %u, %u],\n  \"parts\": [\n", r->groove[0], r->groove[1], r->groove[2], r->groove[3]);
    for (k = 0; k < NPART; k++) {
        const dock_part_t *d = &r->part[k];
        uint32_t e = d->engine, n = e < NENGINES ? dock_patch_size(e) : 0u;
        dx_put("    {\"member\": \"%s\", \"engine\": %u, \"engine_name\": ", MEMBER[k], e);
        dx_str(e < NENGINES ? ENGINES[e]->name : "?", 16);
        dx_put(", \"preset\": %u, \"sound\": ", d->preset);
        dx_str(sound && sound[k] ? sound[k] : "", 24);
        dx_put(",\n     \"params\": [");
        for (i = 0; i < P_COUNT; i++)
            dx_put("%s%d", !i ? "" : i % 16u ? ", " : ",\n                ", d->p[i]);
        dx_put("],\n     \"patch\": [");
        for (i = 0; i < n; i++)
            dx_put("%s%02x", !i ? "\"" : i % 32u ? "" : "\",\n               \"", d->patch[i]);
        dx_put("%s]}%s\n", n ? "\"" : "", k + 1u < NPART ? "," : "");
    }
    dx_put("  ]\n}\n");
    o[dx_n] = 0;
    return dx_n;
}
