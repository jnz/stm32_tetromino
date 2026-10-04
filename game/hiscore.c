/*
 * See hiscore.h.
 *
 * One record, 16 little endian words:
 *    0  magic 'TTR2'
 *    1  sequence number, starts at 1
 *    2  best score, low word
 *    3  best score, high word
 *    4  best lines
 *    5  best level in bits 0..7, settings in bits 8..31
 *    6  games
 *    7  best score of a human player, low word
 *    8  best score of a human player, high word
 *    9..14  reserved, 0
 *   15  CRC-32 over words 0..14
 *
 * Records of the first format ('TTR1', 32 bit scores) are not read: the
 * first save after an update starts the log afresh.
 */
#include "hiscore.h"
#include "hiscore_port.h"
#include <string.h>

#define HS_MAGIC     0x32525454U          /* 'T' 'T' 'R' '2' */
#define HS_WORDS     16U
#define HS_REC_SZ    (HS_WORDS * 4U)
#define HS_TOTAL     (HS_SECTORS * HS_SECTOR_SZ)
#define HS_SLOTS     (HS_TOTAL / HS_REC_SZ)

#define HS_NO_SECTOR 0xFFFFFFFFU

static uint32_t s_seq;        /* sequence of the latest valid record */
static uint32_t s_next_off;   /* where the next record goes */
static uint32_t s_latest;     /* sector holding that record, or none */

static uint32_t crc32(const uint32_t *w, uint32_t nwords)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t i, b;

    for (i = 0; i < nwords; i++) {
        for (b = 0; b < 4U; b++) {
            int k;

            crc ^= (w[i] >> (8U * b)) & 0xFFU;
            for (k = 0; k < 8; k++)
                crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

static int slot_erased(const uint32_t *r)
{
    uint32_t i;

    for (i = 0; i < HS_WORDS; i++) {
        if (r[i] != 0xFFFFFFFFU)
            return 0;
    }
    return 1;
}

static int record_valid(const uint32_t *r)
{
    return r[0] == HS_MAGIC && r[HS_WORDS - 1U] == crc32(r, HS_WORDS - 1U);
}

static uint64_t u64(uint32_t lo, uint32_t hi)
{
    return ((uint64_t)hi << 32) | lo;
}

void hiscore_load(hiscore_t *out)
{
    uint32_t r[HS_WORDS];
    uint32_t best_off = 0;
    uint32_t off;

    memset(out, 0, sizeof *out);
    s_seq = 0;
    s_latest = HS_NO_SECTOR;

    for (off = 0; off < HS_TOTAL; off += HS_REC_SZ) {
        if (hs_port_read(off, r, HS_REC_SZ) != 0 || !record_valid(r))
            continue;
        if (r[1] > s_seq) {
            s_seq = r[1];
            best_off = off;
            out->best_score = u64(r[2], r[3]);
            out->best_lines = r[4];
            out->best_level = r[5] & 0xFFU;
            out->settings   = r[5] >> 8;
            out->games      = r[6];
            out->human_best = u64(r[7], r[8]);
        }
    }

    /* Nothing stored: start at the beginning, which erases sector 0 on the
     * first save, whatever it held before. */
    s_next_off = (s_seq == 0U) ? 0U : (best_off + HS_REC_SZ) % HS_TOTAL;
    if (s_seq != 0U)
        s_latest = best_off / HS_SECTOR_SZ;
}

int hiscore_save(const hiscore_t *hs)
{
    uint32_t rec[HS_WORDS];
    uint32_t back[HS_WORDS];
    uint32_t tries;

    memset(rec, 0, sizeof rec);
    rec[0] = HS_MAGIC;
    rec[1] = s_seq + 1U;
    rec[2] = (uint32_t)hs->best_score;
    rec[3] = (uint32_t)(hs->best_score >> 32);
    rec[4] = hs->best_lines;
    rec[5] = (hs->best_level & 0xFFU) | (hs->settings << 8);
    rec[6] = hs->games;
    rec[7] = (uint32_t)hs->human_best;
    rec[8] = (uint32_t)(hs->human_best >> 32);
    rec[HS_WORDS - 1U] = crc32(rec, HS_WORDS - 1U);

    /* Bounded: at most one pass over every slot. Slots that are neither
     * erased nor take a write (left over from a write that was cut short)
     * are stepped over. */
    for (tries = 0; tries < HS_SLOTS; tries++) {
        const uint32_t off = s_next_off;

        s_next_off = (s_next_off + HS_REC_SZ) % HS_TOTAL;

        /* Entering a sector: everything in it is older than the record
         * just behind us, so it can go. Unless it is the sector that
         * record is in, which only happens when every slot of the other
         * sector refused the write: then the flash is failing, and
         * erasing would throw away the one good record. */
        if (off % HS_SECTOR_SZ == 0U) {
            if (off / HS_SECTOR_SZ == s_latest) {
                s_next_off = off;
                return -1;
            }
            if (hs_port_erase(off / HS_SECTOR_SZ) != 0) {
                s_next_off = off;   /* try the erase again next time */
                return -1;
            }
        }

        if (hs_port_read(off, back, HS_REC_SZ) != 0 || !slot_erased(back))
            continue;
        if (hs_port_program(off, rec, HS_REC_SZ) != 0)
            continue;
        if (hs_port_read(off, back, HS_REC_SZ) == 0 &&
            memcmp(back, rec, HS_REC_SZ) == 0) {
            s_seq = rec[1];
            s_latest = off / HS_SECTOR_SZ;
            return 0;
        }
    }
    return -1;
}
