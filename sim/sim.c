/*
 * Host simulator: runs the same game, AI, high score store and renderer as
 * the firmware, against a simulated clock and a RAM flash, and writes
 * frames as PPM images.
 *
 *   sim [options]
 *     -t TICKS    game ticks to run (50 ms each)                 [2000]
 *     -e EVERY    write every n-th frame, 0 = none                 [0]
 *     -S START    no frames before this tick                       [0]
 *     -i TEXT     info text bottom right in the panel
 *     -o DIR      where frames go                                  [.]
 *     -s SEED     seed                                             [1]
 *     -b PERMILLE AI blunder rate                                  [50]
 *     -f FILE     keep the flash in FILE across runs
 *     -g          also write the first frame of every game over screen
 *     -H          touch the panel once at the start: a human game
 *
 * One line per finished game goes to stdout.
 */
#include "app.h"
#include "render.h"
#include "flash_ram.h"
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t s_fb[RENDER_W * RENDER_H];

/* The frame as the panel shows it at now: through the palette of a running
 * line clear effect, shifted by the burn in shift (fx.h). */
static void write_ppm(const char *dir, const char *name, uint32_t n,
                      const app_t *app, uint32_t now)
{
    char path[512];
    uint32_t pal[256];
    FILE *f;
    int dx, dy;
    int x, y;

    snprintf(path, sizeof path, "%s/%s_%06u.ppm", dir, name, (unsigned)n);
    f = fopen(path, "wb");
    if (f == NULL) {
        perror(path);
        exit(1);
    }
    fprintf(f, "P6\n%d %d\n255\n", RENDER_W, RENDER_H);
    (void)fx_palette(&app->fx, now, pal);
    fx_shift(now, &dx, &dy);
    for (y = 0; y < RENDER_H; y++) {
        for (x = 0; x < RENDER_W; x++) {
            /* through the palette, as the LTDC does it, black where the
             * shifted picture leaves the panel uncovered */
            const int sx = x - dx, sy = y - dy;
            const uint32_t c = (sx < 0 || sx >= RENDER_W || sy < 0 || sy >= RENDER_H)
                ? 0U : pal[s_fb[sy * RENDER_W + sx]];
            const uint8_t rgb[3] = { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c };

            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    unsigned long ticks = 2000, every = 0, seed = 1, blunder = 50, start = 0;
    const char *infotext = NULL;
    const char *dir = ".";
    const char *flashfile = NULL;
    int shot_gameover = 0;
    int human = 0;
    static app_t app;
    uint32_t now = 0;
    unsigned long i;
    int k;
    FILE *f;

    for (k = 1; k < argc; k++) {
        const char *a = argv[k];
        const char *v = (k + 1 < argc) ? argv[k + 1] : NULL;

        if (strcmp(a, "-g") == 0) {
            shot_gameover = 1;
            continue;
        }
        if (strcmp(a, "-H") == 0) {
            human = 1;
            continue;
        }
        if (v == NULL) {
            fprintf(stderr, "missing value for %s\n", a);
            return 2;
        }
        k++;
        if (strcmp(a, "-t") == 0) ticks = strtoul(v, NULL, 0);
        else if (strcmp(a, "-e") == 0) every = strtoul(v, NULL, 0);
        else if (strcmp(a, "-o") == 0) dir = v;
        else if (strcmp(a, "-s") == 0) seed = strtoul(v, NULL, 0);
        else if (strcmp(a, "-b") == 0) blunder = strtoul(v, NULL, 0);
        else if (strcmp(a, "-f") == 0) flashfile = v;
        else if (strcmp(a, "-S") == 0) start = strtoul(v, NULL, 0);
        else if (strcmp(a, "-i") == 0) infotext = v;
        else {
            fprintf(stderr, "unknown option %s\n", a);
            return 2;
        }
    }

    flash_ram_reset(0xFF);
    if (flashfile != NULL && (f = fopen(flashfile, "rb")) != NULL) {
        if (fread(flash_ram, 1, sizeof flash_ram, f) != sizeof flash_ram)
            flash_ram_reset(0xFF);
        fclose(f);
    }

    app_init(&app, (uint32_t)seed, (uint16_t)blunder, now);
    app.infotext = infotext;
    printf("loaded: best %llu (%u lines, level %u), %u games\n",
           (unsigned long long)app.hs.best_score, (unsigned)app.hs.best_lines,
           (unsigned)app.hs.best_level, (unsigned)app.hs.games);

    for (i = 0; i < ticks; i++) {
        const uint8_t was_over = app.game_over;

        now += TETRIS_TICK_MS;
        if (human && i == 0) {
            const app_touch_t tp = { 1, 1, 75, 200 };

            app_touch(&app, &tp, now);
        }
        app_tick(&app, now);

        if (app.game_over && !was_over) {
            printf("game %u over: score %llu, lines %u, level %u, %.1f min%s\n",
                   (unsigned)app.hs.games, (unsigned long long)app.game.score,
                   (unsigned)app.game.lines, (unsigned)app.game.level,
                   app.game.ticks * (TETRIS_TICK_MS / 1000.0) / 60.0,
                   app.new_record ? ", NEW RECORD" : "");
            if (shot_gameover) {
                app_render(&app, s_fb, NULL);
                write_ppm(dir, "gameover", app.hs.games, &app, now);
            }
        }
        if (every != 0 && i >= start && i % every == 0) {
            app_render(&app, s_fb, NULL);
            write_ppm(dir, "frame", (uint32_t)i, &app, now);
        }
    }

    if (flashfile != NULL && (f = fopen(flashfile, "wb")) != NULL) {
        fwrite(flash_ram, 1, sizeof flash_ram, f);
        fclose(f);
    }
    return 0;
}
