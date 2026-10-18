/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Steven Snasdell */
/* Your Line-ups (dock_lineups.c), kept: one A/B object of storage.c, the one of Melodee's project slot D
 * (OBJ_BANK0 + DOCK_LU_SLOT: five sectors and two more a copy, power-safe as a project's: the commit record last),
 * which projects leave alone under DOCK_SAVE (project.c proj_dock_slot). A header, then up to DOCK_STORE_MAX records
 * (dock_lineup_t), each named after its Line-up: a factory Line-up moved or added keeps yours. A record of another
 * P_COUNT or size (another firmware's) is not read: yours of before then do not load (they stay in the flash until a
 * save replaces them; a conversion when that happens). Read and written through the projects' staging RAM (proj_wire,
 * no RAM of its own), on the main loop only; the host tests without flash (MELODEE_FLASH 0) keep it in RAM.
 * Included by project.c */
#define DOCK_STORE_MAGIC 0x31554C44u                    /* "DLU1" */
#define DOCK_STORE_MAX 8u
typedef struct {
    uint32_t magic;
    uint16_t p_count, rec;                              /* P_COUNT, sizeof(dock_lineup_t) when written */
    uint8_t n, rsv[7];                                  /* the records that follow */
} dock_store_hdr_t;
#define DOCK_STORE_BYTES(n) ((uint32_t)sizeof(dock_store_hdr_t) + (uint32_t)(n) * (uint32_t)sizeof(dock_lineup_t))
_Static_assert(sizeof(dock_store_hdr_t) == 16u && DOCK_STORE_BYTES(DOCK_STORE_MAX) <= PROJ_BANK_BYTES,
               "the store fits the staging RAM");
#if MELODEE_FLASH
_Static_assert(DOCK_STORE_BYTES(DOCK_STORE_MAX) <= ST_BANK_BASE_MAX + ST_BANK_EXT_MAX, "the store fits its object");
#else
static uint8_t dock_store_ram[DOCK_STORE_BYTES(DOCK_STORE_MAX)] __attribute__((aligned(4)));
static uint32_t dock_store_ram_n;                       /* its bytes (0: nothing kept) */
#endif

static dock_store_hdr_t *dock_store_hdr(void) { return (dock_store_hdr_t *)proj_wire_u.raw; }
static dock_lineup_t *dock_store_rec(uint32_t j) { return (dock_lineup_t *)(proj_wire_u.raw + DOCK_STORE_BYTES(j)); }

/* the store -> proj_wire, checked; an empty one (n 0) when there is none or it is not this firmware's */
static dock_store_hdr_t *dock_store_read(void)
{
    dock_store_hdr_t *h = dock_store_hdr();
    int n = -1;
    (void)proj_wire_borrow();                           /* (a backup's copy there is gone) */
#if MELODEE_FLASH
    if (flash_ok)
        n = st_load(OBJ_BANK0 + DOCK_LU_SLOT, proj_wire_u.raw, DOCK_STORE_BYTES(DOCK_STORE_MAX));
#else
    if (dock_store_ram_n) {
        n = (int)dock_store_ram_n;
        memcpy(proj_wire_u.raw, dock_store_ram, dock_store_ram_n);
    }
#endif
    if (n < (int)sizeof *h || h->magic != DOCK_STORE_MAGIC || h->p_count != P_COUNT || h->rec != sizeof(dock_lineup_t) ||
        h->n > DOCK_STORE_MAX || (uint32_t)n != DOCK_STORE_BYTES(h->n)) {
        memset(h, 0, sizeof *h);
        h->magic = DOCK_STORE_MAGIC;
        h->p_count = P_COUNT;
        h->rec = sizeof(dock_lineup_t);
    }
    return h;
}
static int dock_store_write(const dock_store_hdr_t *h)
{
    uint32_t len = DOCK_STORE_BYTES(h->n);
#if MELODEE_FLASH
    if (!flash_ok || st_save(OBJ_BANK0 + DOCK_LU_SLOT, proj_wire_u.raw, len))
        return 2;
#else
    memcpy(dock_store_ram, proj_wire_u.raw, len);
    dock_store_ram_n = len;
#endif
    return 0;
}
static int dock_store_find(const dock_store_hdr_t *h, const char *name)   /* the record named so, -1 none */
{
    uint32_t j;
    for (j = 0; j < h->n; j++)
        if (!memcmp(dock_store_rec(j)->name, name, str_len(name) + 1u < DOCK_NAME ? str_len(name) + 1u : DOCK_NAME))
            return (int)j;
    return -1;
}

static const dock_lineup_t *dock_store_get(const char *name)
{
    const dock_store_hdr_t *h = dock_store_read();
    int j = dock_store_find(h, name);
    return j < 0 ? 0 : dock_store_rec((uint32_t)j);
}

static int dock_store_put(uint32_t i)                   /* 0 kept, 2 failed, 3 full */
{
    dock_store_hdr_t *h = dock_store_read();
    int j = dock_store_find(h, DOCK_LINEUPS[i % DOCK_NLINEUPS].name);
    if (j < 0) {
        if (h->n >= DOCK_STORE_MAX)
            return 3;
        j = h->n++;
    }
    dock_lineup_capture(dock_store_rec((uint32_t)j), i);
    return dock_store_write(h);
}

static int dock_store_drop(const char *name)
{
    dock_store_hdr_t *h = dock_store_read();
    int j = dock_store_find(h, name);
    uint32_t k;
    if (j < 0)
        return 0;
    for (k = (uint32_t)j; k + 1u < h->n; k++)
        memcpy(dock_store_rec(k), dock_store_rec(k + 1u), sizeof(dock_lineup_t));
    h->n--;
    return dock_store_write(h);
}
