#include "roguecore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_map(const uint8_t *tiles, int w, int h, int px, int py) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (x == px && y == py) {
                putchar('@');
                continue;
            }
            uint8_t t = tiles[y * w + x];
            if (t == RC_TILE_WALL) {
                putchar('#');
            } else if (t == RC_TILE_FLOOR) {
                putchar('.');
            } else if (t == RC_TILE_STAIRS) {
                putchar('>');
            } else {
                putchar('?');
            }
        }
        putchar('\n');
    }
}

static void usage(const char *argv0) {
    fprintf(stderr, "用法: %s [--seed N]\n", argv0);
    fprintf(stderr, "  鍵: w/a/s/d 移動, ? 或 h 說明, q 離開\n");
}

static void print_help(void) {
    puts("【怎麼玩】");
    puts("  @ = 玩家  . = 地板  # = 牆  > = 樓梯（走到這格就贏）");
    puts("  在畫面最下方提示後輸入一個字母再按 Enter：w上 s下 a左 d右；? 說明；q 離開");
    puts("目標：把 @ 移到地圖上的 > 。");
    puts("");
}

int main(int argc, char **argv) {
    uint32_t seed = 1u;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    void *g = rc_game_create(seed);
    if (!g) {
        fprintf(stderr, "rc_game_create 失敗\n");
        return 1;
    }

    int w = rc_game_width(g);
    int h = rc_game_height(g);
    size_t bufn = (size_t)w * (size_t)h;
    uint8_t *tiles = (uint8_t *)malloc(bufn);
    if (!tiles) {
        rc_game_destroy(g);
        return 1;
    }

    printf("Roguelike 地城（C 核心） seed=%u\n", (unsigned)seed);
    print_help();

    for (;;) {
        if (rc_game_tiles(g, tiles, bufn) < 0) {
            fprintf(stderr, "rc_game_tiles 失敗\n");
            break;
        }
        int px, py;
        rc_game_player(g, &px, &py);
        print_map(tiles, w, h, px, py);
        if (rc_game_done(g)) {
            printf("你抵達樓梯，勝利！\n");
            break;
        }
        printf("移動 (w/a/s/d，? 說明，q 離開) > ");
        fflush(stdout);
        char line[32];
        if (!fgets(line, sizeof line, stdin)) {
            break;
        }
        char c = line[0];
        if (c == '?' || c == 'h' || c == 'H') {
            print_help();
            continue;
        }
        int dx = 0, dy = 0;
        if (c == 'w' || c == 'W') {
            dy = -1;
        } else if (c == 's' || c == 'S') {
            dy = 1;
        } else if (c == 'a' || c == 'A') {
            dx = -1;
        } else if (c == 'd' || c == 'D') {
            dx = 1;
        } else if (c == 'q' || c == 'Q') {
            break;
        } else {
            continue;
        }
        int mv = rc_game_move(g, dx, dy);
        if (mv == 1) {
            printf("(撞牆)\n");
        } else if (mv < 0) {
            printf("(無效移動)\n");
        }
    }

    free(tiles);
    rc_game_destroy(g);
    return 0;
}
