#ifndef TETRIS_HISCORE_H
#define TETRIS_HISCORE_H
/*
 * All-time high score, kept in flash across power cycles.
 *
 * The store is a log: every save appends one small record, and the one
 * with the highest sequence number wins on load. Two sectors take turns,
 * a sector is only erased when the log moves into it, so with 32 byte
 * records an erase happens once per 512 saves. A save that is cut short
 * by a power loss leaves a record with a bad checksum, which load skips,
 * so the previous state survives.
 */
#include <stdint.h>

typedef struct {
    uint32_t best_score;
    uint32_t best_lines;    /* lines of the best game */
    uint32_t best_level;    /* level the best game ended at */
    uint32_t games;         /* games finished, ever */
    uint32_t human_best;    /* best score of a human player (touch) */
} hiscore_t;

/* Reads the latest valid record. Fills *out with zeros if there is none.
 * Must run once before hiscore_save(). */
void hiscore_load(hiscore_t *out);

/* Appends a record. 0 = written and read back, -1 = flash error. */
int hiscore_save(const hiscore_t *hs);

#endif /* TETRIS_HISCORE_H */
