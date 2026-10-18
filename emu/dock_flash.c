/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * The emulator's flash, from Felucca's browser build (web/emu/felucca_web.c of hugelton/Felucca, GPL-3.0): a 1 MiB
 * NOR image behind storage.c's three hooks, erased to 0xFF, a write only clearing bits as on the part; the FM-1's
 * JEDEC id, so persist_boot takes it for the real one. The page (panel/worklet.js storeSectors / restoreSectors) keeps
 * the sectors that hold something in the browser's storage and hands them back before boot, so the settings, user
 * presets, projects and your Line-ups (Dock: dock_store.c) survive a reload. Included by tests/ui_test.c
 * (UI_TEST_FLASH_HOOKS) for emu/dock_web.c, just before storage.c. */
static uint8_t nor[0x100000];                           /* the SPI NOR (erased: 0xFF) */
static uint32_t web_flash_writes;                       /* +1 a write or an erase: the page then sends the sectors */
static uint8_t flash_ok;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off >= sizeof nor || n > sizeof nor - off)
        return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (off >= sizeof nor || 4096u > sizeof nor - off)
        return -8;
    memset(nor + (off & ~4095u), 0xFF, 4096);
    web_flash_writes++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)       /* NOR: a write only clears bits */
{
    const uint8_t *s = src;
    uint32_t i;
    if (off >= sizeof nor || n > sizeof nor - off)
        return -8;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    web_flash_writes++;
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }             /* the FM-1's part (project.c persist_boot) */
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
