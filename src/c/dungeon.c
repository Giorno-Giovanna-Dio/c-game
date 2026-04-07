#include "dungeon.h"
#include <string.h>

#define MAX_ROOMS 24
#define MIN_ROOM_W 4
#define MIN_ROOM_H 3
#define MAX_TRY_ROOM 80

typedef struct {
    int x, y, w, h;
} Room;

static int idx(int w, int x, int y) { return y * w + x; }

static void carve_hline(uint8_t *tiles, int w, int h, int x0, int x1, int y) {
    if (y < 0 || y >= h) {
        return;
    }
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    for (int x = x0; x <= x1; x++) {
        if (x >= 0 && x < w) {
            tiles[idx(w, x, y)] = DUN_TILE_FLOOR;
        }
    }
}

static void carve_vline(uint8_t *tiles, int w, int h, int x, int y0, int y1) {
    if (x < 0 || x >= w) {
        return;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for (int y = y0; y <= y1; y++) {
        if (y >= 0 && y < h) {
            tiles[idx(w, x, y)] = DUN_TILE_FLOOR;
        }
    }
}

static void connect_rooms_rng(rc_rng_t *rng, uint8_t *tiles, int w, int h, int ax, int ay, int bx,
                              int by) {
    if (rc_rng_u32(rng) & 1u) {
        carve_hline(tiles, w, h, ax, bx, ay);
        carve_vline(tiles, w, h, bx, ay, by);
    } else {
        carve_vline(tiles, w, h, ax, ay, by);
        carve_hline(tiles, w, h, ax, bx, by);
    }
}

void rc_dungeon_generate(rc_rng_t *rng, int w, int h, uint8_t *tiles, int *out_stairs_idx,
                         int *out_player_idx) {
    memset(tiles, DUN_TILE_WALL, (size_t)w * (size_t)h);

    Room rooms[MAX_ROOMS];
    int nrooms = 0;

    for (int t = 0; t < MAX_TRY_ROOM && nrooms < MAX_ROOMS; t++) {
        int rw = rc_rng_range(rng, MIN_ROOM_W, 10);
        int rh = rc_rng_range(rng, MIN_ROOM_H, 8);
        int rx = rc_rng_range(rng, 1, w - rw - 2);
        int ry = rc_rng_range(rng, 1, h - rh - 2);

        int ok = 1;
        for (int i = 0; i < nrooms && ok; i++) {
            const Room *a = &rooms[i];
            if (!(rx + rw < a->x - 1 || a->x + a->w < rx - 1 || ry + rh < a->y - 1 ||
                  a->y + a->h < ry - 1)) {
                ok = 0;
            }
        }
        if (!ok) {
            continue;
        }

        Room *R = &rooms[nrooms++];
        R->x = rx;
        R->y = ry;
        R->w = rw;
        R->h = rh;
        for (int yy = ry; yy < ry + rh; yy++) {
            for (int xx = rx; xx < rx + rw; xx++) {
                tiles[idx(w, xx, yy)] = DUN_TILE_FLOOR;
            }
        }
    }

    if (nrooms == 0) {
        /* 後備：整層空房 */
        for (int yy = 1; yy < h - 1; yy++) {
            for (int xx = 1; xx < w - 1; xx++) {
                tiles[idx(w, xx, yy)] = DUN_TILE_FLOOR;
            }
        }
        *out_player_idx = idx(w, 1, 1);
        *out_stairs_idx = idx(w, w - 2, h - 2);
        tiles[*out_stairs_idx] = DUN_TILE_STAIRS;
        return;
    }

    for (int i = 1; i < nrooms; i++) {
        int ax = rooms[i - 1].x + rooms[i - 1].w / 2;
        int ay = rooms[i - 1].y + rooms[i - 1].h / 2;
        int bx = rooms[i].x + rooms[i].w / 2;
        int by = rooms[i].y + rooms[i].h / 2;
        connect_rooms_rng(rng, tiles, w, h, ax, ay, bx, by);
    }

    int pidx = idx(w, rooms[0].x + rooms[0].w / 2, rooms[0].y + rooms[0].h / 2);
    int sidx = idx(w, rooms[nrooms - 1].x + rooms[nrooms - 1].w / 2,
                   rooms[nrooms - 1].y + rooms[nrooms - 1].h / 2);

    tiles[sidx] = DUN_TILE_STAIRS;
    *out_player_idx = pidx;
    *out_stairs_idx = sidx;
}
