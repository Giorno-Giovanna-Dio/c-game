#include "roguecore.h"
#include "dungeon.h"
#include "rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int x, y;
    int hp;
    int alive;
} Monster;

typedef struct {
    rc_rng_t rng;
    int w, h;
    uint8_t *tiles;
    int px, py;
    int hp, max_hp;
    Monster monsters[RC_MAX_MONSTERS];
    int nmonsters;
    char msg[128];
    int floor;
    int won;
} Game;

static int idx(int w, int x, int y) { return y * w + x; }
static int in_bounds(const Game *g, int x, int y) {
    return x >= 0 && y >= 0 && x < g->w && y < g->h;
}
static uint8_t tile_at(const Game *g, int x, int y) {
    return g->tiles[idx(g->w, x, y)];
}

static int monster_at(const Game *g, int x, int y) {
    for (int i = 0; i < g->nmonsters; i++) {
        if (g->monsters[i].alive && g->monsters[i].x == x && g->monsters[i].y == y)
            return i;
    }
    return -1;
}

static int abs_i(int v) { return v < 0 ? -v : v; }

static void spawn_monsters(Game *g) {
    int count = rc_rng_range(&g->rng, 3, 3 + g->floor);
    if (count > RC_MAX_MONSTERS) count = RC_MAX_MONSTERS;
    g->nmonsters = 0;
    int hp_lo = 2;
    int hp_hi = 3 + g->floor / 2;
    for (int t = 0; t < 200 && g->nmonsters < count; t++) {
        int x = rc_rng_range(&g->rng, 1, g->w - 2);
        int y = rc_rng_range(&g->rng, 1, g->h - 2);
        if (tile_at(g, x, y) != RC_TILE_FLOOR) continue;
        if (x == g->px && y == g->py) continue;
        int dist = abs_i(x - g->px) + abs_i(y - g->py);
        if (dist < 5) continue;
        if (monster_at(g, x, y) >= 0) continue;
        Monster *m = &g->monsters[g->nmonsters++];
        m->x = x;
        m->y = y;
        m->hp = rc_rng_range(&g->rng, hp_lo, hp_hi);
        m->alive = 1;
    }
}

static void generate_floor(Game *g) {
    int si = 0, pi = 0;
    rc_dungeon_generate(&g->rng, g->w, g->h, g->tiles, &si, &pi);
    g->px = pi % g->w;
    g->py = pi / g->w;
    g->msg[0] = '\0';
    spawn_monsters(g);
}

static void monster_ai(Game *g) {
    for (int i = 0; i < g->nmonsters; i++) {
        Monster *m = &g->monsters[i];
        if (!m->alive) continue;
        int dx = 0, dy = 0;
        int diffx = g->px - m->x;
        int diffy = g->py - m->y;
        if (abs_i(diffx) >= abs_i(diffy)) {
            dx = (diffx > 0) ? 1 : (diffx < 0 ? -1 : 0);
        } else {
            dy = (diffy > 0) ? 1 : (diffy < 0 ? -1 : 0);
        }
        int nx = m->x + dx;
        int ny = m->y + dy;
        if (!in_bounds(g, nx, ny) || tile_at(g, nx, ny) == RC_TILE_WALL) continue;
        if (monster_at(g, nx, ny) >= 0) continue;
        if (nx == g->px && ny == g->py) {
            int dmg = rc_rng_range(&g->rng, 1, 3);
            g->hp -= dmg;
            if (g->hp < 0) g->hp = 0;
            snprintf(g->msg, sizeof g->msg, "怪物攻擊你！-%d HP（剩 %d）", dmg, g->hp);
            continue;
        }
        m->x = nx;
        m->y = ny;
    }
}

void *rc_game_create(uint32_t seed) {
    Game *g = (Game *)calloc(1, sizeof(Game));
    if (!g) return NULL;
    g->w = RC_GAME_WIDTH;
    g->h = RC_GAME_HEIGHT;
    g->tiles = (uint8_t *)malloc((size_t)g->w * (size_t)g->h);
    if (!g->tiles) { free(g); return NULL; }
    rc_rng_seed(&g->rng, seed);
    g->max_hp = RC_PLAYER_START_HP;
    g->hp = g->max_hp;
    g->floor = 1;
    g->won = 0;
    generate_floor(g);
    return g;
}

void rc_game_destroy(void *handle) {
    Game *g = (Game *)handle;
    if (!g) return;
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

int rc_game_move(void *handle, int dx, int dy) {
    Game *g = (Game *)handle;
    if (!g || (dx == 0 && dy == 0) || (dx != 0 && dy != 0)) return -1;
    if (dx < -1 || dx > 1 || dy < -1 || dy > 1) return -1;
    if (g->hp <= 0) return 3;

    g->msg[0] = '\0';
    int nx = g->px + dx;
    int ny = g->py + dy;
    if (!in_bounds(g, nx, ny)) return 1;
    uint8_t t = tile_at(g, nx, ny);
    if (t == RC_TILE_WALL) return 1;

    int mi = monster_at(g, nx, ny);
    if (mi >= 0) {
        Monster *m = &g->monsters[mi];
        int dmg = rc_rng_range(&g->rng, 2, 4);
        m->hp -= dmg;
        if (m->hp <= 0) {
            m->alive = 0;
            snprintf(g->msg, sizeof g->msg, "你擊殺了怪物！（傷害 %d）", dmg);
        } else {
            snprintf(g->msg, sizeof g->msg, "你攻擊怪物！--%d（怪物剩 %d HP）", dmg, m->hp);
        }
        monster_ai(g);
        return (g->hp <= 0) ? 3 : 2;
    }

    g->px = nx;
    g->py = ny;
    monster_ai(g);
    return (g->hp <= 0) ? 3 : 0;
}

void rc_game_player(const void *handle, int *out_x, int *out_y) {
    const Game *g = (const Game *)handle;
    if (!g || !out_x || !out_y) return;
    *out_x = g->px;
    *out_y = g->py;
}

int rc_game_tiles(const void *handle, uint8_t *out, size_t cap) {
    const Game *g = (const Game *)handle;
    if (!g || !out) return -1;
    size_t need = (size_t)g->w * (size_t)g->h;
    if (cap < need) return -1;
    memcpy(out, g->tiles, need);
    return (int)need;
}

int rc_game_descend(void *handle) {
    Game *g = (Game *)handle;
    if (!g) return 0;
    if (tile_at(g, g->px, g->py) != RC_TILE_STAIRS) return 0;
    if (g->floor >= RC_MAX_FLOORS) {
        g->won = 1;
        snprintf(g->msg, sizeof g->msg, "你征服了全部 %d 層地城！", RC_MAX_FLOORS);
        return 2;
    }
    g->floor++;
    int heal = 5 + g->floor;
    g->hp += heal;
    if (g->hp > g->max_hp) g->hp = g->max_hp;
    snprintf(g->msg, sizeof g->msg, "進入第 %d 層！回復 %d HP", g->floor, heal);
    generate_floor(g);
    return 1;
}

int rc_game_won(const void *handle) {
    const Game *g = (const Game *)handle;
    if (!g) return 0;
    return g->won;
}

int rc_game_floor(const void *handle) {
    const Game *g = (const Game *)handle;
    return g ? g->floor : 0;
}

int rc_game_dead(const void *handle) {
    const Game *g = (const Game *)handle;
    if (!g) return 0;
    return g->hp <= 0 ? 1 : 0;
}

int rc_game_player_hp(const void *handle) {
    const Game *g = (const Game *)handle;
    return g ? g->hp : 0;
}

int rc_game_player_max_hp(const void *handle) {
    const Game *g = (const Game *)handle;
    return g ? g->max_hp : 0;
}

int rc_game_monster_count(const void *handle) {
    const Game *g = (const Game *)handle;
    if (!g) return 0;
    int n = 0;
    for (int i = 0; i < g->nmonsters; i++)
        if (g->monsters[i].alive) n++;
    return n;
}

int rc_game_monsters(const void *handle, int *out, int cap) {
    const Game *g = (const Game *)handle;
    if (!g || !out) return 0;
    int wrote = 0;
    for (int i = 0; i < g->nmonsters; i++) {
        if (!g->monsters[i].alive) continue;
        if ((wrote + 1) * 3 > cap) break;
        out[wrote * 3 + 0] = g->monsters[i].x;
        out[wrote * 3 + 1] = g->monsters[i].y;
        out[wrote * 3 + 2] = g->monsters[i].hp;
        wrote++;
    }
    return wrote;
}

const char *rc_game_last_message(const void *handle) {
    const Game *g = (const Game *)handle;
    if (!g) return "";
    return g->msg;
}
