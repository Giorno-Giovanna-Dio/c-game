#include "roguecore.h"
#include "dungeon.h"
#include "rng.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    rc_rng_t rng;
    int w, h;
    uint8_t *tiles;
    int px, py;
} Game;

void *rc_game_create(uint32_t seed) {
    Game *g = (Game *)calloc(1, sizeof(Game));
    if (!g) {
        return NULL;
    }
    g->w = RC_GAME_WIDTH;
    g->h = RC_GAME_HEIGHT;
    g->tiles = (uint8_t *)malloc((size_t)g->w * (size_t)g->h);
    if (!g->tiles) {
        free(g);
        return NULL;
    }
    rc_rng_seed(&g->rng, seed);
    int si = 0, pi = 0;
    rc_dungeon_generate(&g->rng, g->w, g->h, g->tiles, &si, &pi);
    g->px = pi % g->w;
    g->py = pi / g->w;
    return g;
}

void rc_game_destroy(void *handle) {
    Game *g = (Game *)handle;
    if (!g) {
        return;
    }
    free(g->tiles);
    free(g);
}

int rc_game_width(const void *handle) {
    const Game *g = (const Game *)handle;
    return g ? g->w : -1;
}

int rc_game_height(const void *handle) {
    const Game *g = (const Game *)handle;
    return g ? g->h : -1;
}

static int in_bounds(const Game *g, int x, int y) {
    return x >= 0 && y >= 0 && x < g->w && y < g->h;
}

static uint8_t tile_at(const Game *g, int x, int y) {
    return g->tiles[y * g->w + x];
}

int rc_game_move(void *handle, int dx, int dy) {
    Game *g = (Game *)handle;
    if (!g || (dx == 0 && dy == 0) || (dx != 0 && dy != 0)) {
        return -1;
    }
    if (dx < -1 || dx > 1 || dy < -1 || dy > 1) {
        return -1;
    }
    int nx = g->px + dx;
    int ny = g->py + dy;
    if (!in_bounds(g, nx, ny)) {
        return 1;
    }
    uint8_t t = tile_at(g, nx, ny);
    if (t == RC_TILE_WALL) {
        return 1;
    }
    g->px = nx;
    g->py = ny;
    return 0;
}

void rc_game_player(const void *handle, int *out_x, int *out_y) {
    const Game *g = (const Game *)handle;
    if (!g || !out_x || !out_y) {
        return;
    }
    *out_x = g->px;
    *out_y = g->py;
}

int rc_game_tiles(const void *handle, uint8_t *out, size_t cap) {
    const Game *g = (const Game *)handle;
    if (!g || !out) {
        return -1;
    }
    size_t need = (size_t)g->w * (size_t)g->h;
    if (cap < need) {
        return -1;
    }
    memcpy(out, g->tiles, need);
    return (int)need;
}

int rc_game_done(const void *handle) {
    const Game *g = (const Game *)handle;
    if (!g) {
        return 0;
    }
    return tile_at(g, g->px, g->py) == RC_TILE_STAIRS ? 1 : 0;
}
