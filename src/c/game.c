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
    int type;
} Monster;

typedef struct {
    int x, y;
    int type;
    int picked;
} Item;

typedef struct {
    rc_rng_t rng;
    int w, h;
    uint8_t *tiles;
    uint8_t *visible;
    uint8_t *explored;
    int px, py;
    int hp, max_hp;
    Monster monsters[RC_MAX_MONSTERS];
    int nmonsters;
    Item items[RC_MAX_ITEMS];
    int nitems;
    char msg[128];
    int floor;
    int won;
    int steps_left;
    int steps_max;
} Game;

static int idx(int w, int x, int y) { return y * w + x; }
static int in_bounds(const Game *g, int x, int y) {
    return x >= 0 && y >= 0 && x < g->w && y < g->h;
}
static uint8_t tile_at(const Game *g, int x, int y) {
    return g->tiles[idx(g->w, x, y)];
}

#define FOV_RADIUS 6

static void update_fov(Game *g) {
    int n = g->w * g->h;
    memset(g->visible, 0, (size_t)n);
    int r2 = FOV_RADIUS * FOV_RADIUS;
    for (int y = 0; y < g->h; y++) {
        for (int x = 0; x < g->w; x++) {
            int dx = x - g->px;
            int dy = y - g->py;
            if (dx * dx + dy * dy <= r2) {
                int i = idx(g->w, x, y);
                g->visible[i] = 1;
                g->explored[i] = 1;
            }
        }
    }
}

static int monster_at(const Game *g, int x, int y) {
    for (int i = 0; i < g->nmonsters; i++) {
        if (g->monsters[i].alive && g->monsters[i].x == x && g->monsters[i].y == y)
            return i;
    }
    return -1;
}

static int abs_i(int v) { return v < 0 ? -v : v; }

static const char *mon_name(int type) {
    switch (type) {
    case RC_MON_ZOMBIE: return "殭屍";
    case RC_MON_SLIME:  return "史萊姆";
    case RC_MON_HUNTER: return "獵人";
    default: return "怪物";
    }
}

static void spawn_monsters(Game *g) {
    int count = rc_rng_range(&g->rng, 3, 3 + g->floor);
    if (count > RC_MAX_MONSTERS) count = RC_MAX_MONSTERS;
    g->nmonsters = 0;
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
        int roll = rc_rng_range(&g->rng, 0, 9);
        if (g->floor <= 2) {
            m->type = (roll < 6) ? RC_MON_ZOMBIE : RC_MON_SLIME;
        } else {
            if (roll < 4) m->type = RC_MON_ZOMBIE;
            else if (roll < 7) m->type = RC_MON_SLIME;
            else m->type = RC_MON_HUNTER;
        }
        switch (m->type) {
        case RC_MON_SLIME:
            m->hp = rc_rng_range(&g->rng, 4, 5 + g->floor / 2);
            break;
        case RC_MON_HUNTER:
            m->hp = rc_rng_range(&g->rng, 3, 4 + g->floor / 2);
            break;
        default:
            m->hp = rc_rng_range(&g->rng, 2, 3 + g->floor / 2);
            break;
        }
        m->alive = 1;
    }
}

static int item_at(const Game *g, int x, int y) {
    for (int i = 0; i < g->nitems; i++) {
        if (!g->items[i].picked && g->items[i].x == x && g->items[i].y == y)
            return i;
    }
    return -1;
}

static void spawn_items(Game *g) {
    int count = rc_rng_range(&g->rng, 2, 3 + g->floor / 2);
    if (count > RC_MAX_ITEMS) count = RC_MAX_ITEMS;
    g->nitems = 0;
    for (int t = 0; t < 200 && g->nitems < count; t++) {
        int x = rc_rng_range(&g->rng, 1, g->w - 2);
        int y = rc_rng_range(&g->rng, 1, g->h - 2);
        if (tile_at(g, x, y) != RC_TILE_FLOOR) continue;
        if (x == g->px && y == g->py) continue;
        if (monster_at(g, x, y) >= 0) continue;
        if (item_at(g, x, y) >= 0) continue;
        Item *it = &g->items[g->nitems++];
        it->x = x;
        it->y = y;
        int roll = rc_rng_range(&g->rng, 0, 9);
        if (roll < 5) it->type = RC_ITEM_POTION;
        else if (roll < 8) it->type = RC_ITEM_BLINK;
        else it->type = RC_ITEM_MAP;
        it->picked = 0;
    }
}

static void pickup_item(Game *g) {
    int ii = item_at(g, g->px, g->py);
    if (ii < 0) return;
    Item *it = &g->items[ii];
    it->picked = 1;
    switch (it->type) {
    case RC_ITEM_POTION: {
        int heal = rc_rng_range(&g->rng, 5, 8);
        g->hp += heal;
        if (g->hp > g->max_hp) g->hp = g->max_hp;
        snprintf(g->msg, sizeof g->msg, "拾取治療藥水 ! 回復 %d HP（現 %d）", heal, g->hp);
        break;
    }
    case RC_ITEM_BLINK: {
        int best_x = -1, best_y = -1, best_dist = -1;
        for (int att = 0; att < 120; att++) {
            int bx = rc_rng_range(&g->rng, 1, g->w - 2);
            int by = rc_rng_range(&g->rng, 1, g->h - 2);
            if (tile_at(g, bx, by) != RC_TILE_FLOOR) continue;
            if (monster_at(g, bx, by) >= 0) continue;
            int safe = 1, min_md = 999;
            for (int j = 0; j < g->nmonsters; j++) {
                if (!g->monsters[j].alive) continue;
                int md = abs_i(bx - g->monsters[j].x) + abs_i(by - g->monsters[j].y);
                if (md < min_md) min_md = md;
                if (md < 4) { safe = 0; break; }
            }
            if (safe && min_md > best_dist) {
                best_x = bx; best_y = by; best_dist = min_md;
            }
        }
        if (best_x >= 0) {
            g->px = best_x;
            g->py = best_y;
            snprintf(g->msg, sizeof g->msg, "閃現卷軸 ~ 瞬移到安全位置！");
        } else {
            snprintf(g->msg, sizeof g->msg, "閃現卷軸 ~ 無處可去…");
        }
        break;
    }
    case RC_ITEM_MAP:
        for (int i = 0; i < g->w * g->h; i++) g->explored[i] = 1;
        snprintf(g->msg, sizeof g->msg, "拾取地圖卷軸 %% 揭示整層地形！");
        break;
    }
}

static void generate_floor(Game *g) {
    int si = 0, pi = 0;
    rc_dungeon_generate(&g->rng, g->w, g->h, g->tiles, &si, &pi);
    g->px = pi % g->w;
    g->py = pi / g->w;
    g->msg[0] = '\0';
    size_t n = (size_t)g->w * (size_t)g->h;
    memset(g->visible, 0, n);
    memset(g->explored, 0, n);
    g->steps_max = RC_BASE_STEPS + g->floor * 20;
    g->steps_left = g->steps_max;
    spawn_monsters(g);
    spawn_items(g);
    update_fov(g);
}

/* chase: 1=追蹤, 0=隨機, -1=逃跑 */
static void move_monster_once(Game *g, Monster *m, int chase, int can_attack) {
    int dx = 0, dy = 0;
    if (chase == 1) {
        int diffx = g->px - m->x;
        int diffy = g->py - m->y;
        if (abs_i(diffx) >= abs_i(diffy))
            dx = (diffx > 0) ? 1 : (diffx < 0 ? -1 : 0);
        else
            dy = (diffy > 0) ? 1 : (diffy < 0 ? -1 : 0);
    } else if (chase == -1) {
        int diffx = g->px - m->x;
        int diffy = g->py - m->y;
        if (abs_i(diffx) >= abs_i(diffy))
            dx = (diffx > 0) ? -1 : (diffx < 0 ? 1 : 0);
        else
            dy = (diffy > 0) ? -1 : (diffy < 0 ? 1 : 0);
    } else {
        int dir = rc_rng_range(&g->rng, 0, 3);
        const int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
        dx = dirs[dir][0]; dy = dirs[dir][1];
    }
    int nx = m->x + dx;
    int ny = m->y + dy;
    if (!in_bounds(g, nx, ny) || tile_at(g, nx, ny) == RC_TILE_WALL) return;
    if (monster_at(g, nx, ny) >= 0) return;
    if (nx == g->px && ny == g->py) {
        if (!can_attack) return;
        int lo = 2, hi = 3;
        if (m->type == RC_MON_SLIME)  { lo = 1; hi = 1; }
        if (m->type == RC_MON_HUNTER) { lo = 2; hi = 3; }
        int dmg = rc_rng_range(&g->rng, lo, hi);
        g->hp -= dmg;
        if (g->hp < 0) g->hp = 0;
        snprintf(g->msg, sizeof g->msg, "%s攻擊你！-%d HP（剩 %d）", mon_name(m->type), dmg, g->hp);
        return;
    }
    m->x = nx;
    m->y = ny;
}

static void monster_ai(Game *g) {
    for (int i = 0; i < g->nmonsters; i++) {
        Monster *m = &g->monsters[i];
        if (!m->alive) continue;
        switch (m->type) {
        case RC_MON_ZOMBIE: {
            int dist = abs_i(g->px - m->x) + abs_i(g->py - m->y);
            if (dist <= 6)
                move_monster_once(g, m, 1, 1);
            break;
        }
        case RC_MON_SLIME:
            move_monster_once(g, m, 0, 1);
            break;
        case RC_MON_HUNTER:
            move_monster_once(g, m, 1, 1);
            if (m->alive) {
                int flee = (m->hp <= 1) ? -1 : 1;
                move_monster_once(g, m, flee, 0);
            }
            break;
        }
    }
}

void *rc_game_create(uint32_t seed) {
    Game *g = (Game *)calloc(1, sizeof(Game));
    if (!g) return NULL;
    g->w = RC_GAME_WIDTH;
    g->h = RC_GAME_HEIGHT;
    size_t n = (size_t)g->w * (size_t)g->h;
    g->tiles = (uint8_t *)malloc(n);
    g->visible = (uint8_t *)calloc(n, 1);
    g->explored = (uint8_t *)calloc(n, 1);
    if (!g->tiles || !g->visible || !g->explored) {
        free(g->tiles); free(g->visible); free(g->explored); free(g); return NULL;
    }
    rc_rng_seed(&g->rng, seed);
    g->max_hp = RC_PLAYER_START_HP;
    g->hp = g->max_hp;
    g->floor = 1;
    g->won = 0;
    generate_floor(g);
    update_fov(g);
    return g;
}

void rc_game_destroy(void *handle) {
    Game *g = (Game *)handle;
    if (!g) return;
    free(g->tiles);
    free(g->visible);
    free(g->explored);
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
    if (g->steps_left > 0) {
        g->steps_left--;
    } else {
        g->hp -= 2;
        if (g->hp < 0) g->hp = 0;
        snprintf(g->msg, sizeof g->msg, "地城崩塌中！-2 HP（剩 %d）", g->hp);
        if (g->hp <= 0) return 3;
    }
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
            if (m->type == RC_MON_SLIME) {
                int heal = 2;
                g->hp += heal;
                if (g->hp > g->max_hp) g->hp = g->max_hp;
                snprintf(g->msg, sizeof g->msg,
                    "擊殺史萊姆！（傷害 %d）+%d HP", dmg, heal);
            } else {
                snprintf(g->msg, sizeof g->msg,
                    "你擊殺了%s！（傷害 %d）", mon_name(m->type), dmg);
            }
        } else {
            snprintf(g->msg, sizeof g->msg, "你攻擊%s！--%d（剩 %d HP）", mon_name(m->type), dmg, m->hp);
        }
        monster_ai(g);
        update_fov(g);
        return (g->hp <= 0) ? 3 : 2;
    }

    g->px = nx;
    g->py = ny;
    pickup_item(g);
    monster_ai(g);
    update_fov(g);
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
        if ((wrote + 1) * 4 > cap) break;
        out[wrote * 4 + 0] = g->monsters[i].x;
        out[wrote * 4 + 1] = g->monsters[i].y;
        out[wrote * 4 + 2] = g->monsters[i].hp;
        out[wrote * 4 + 3] = g->monsters[i].type;
        wrote++;
    }
    return wrote;
}

const char *rc_game_last_message(const void *handle) {
    const Game *g = (const Game *)handle;
    if (!g) return "";
    return g->msg;
}

int rc_game_steps_left(const void *handle) {
    const Game *g = (const Game *)handle;
    return g ? g->steps_left : 0;
}

int rc_game_steps_max(const void *handle) {
    const Game *g = (const Game *)handle;
    return g ? g->steps_max : 0;
}

int rc_game_timeout(const void *handle) {
    const Game *g = (const Game *)handle;
    return (g && g->steps_left <= 0) ? 1 : 0;
}

int rc_game_items(const void *handle, int *out, int cap) {
    const Game *g = (const Game *)handle;
    if (!g || !out) return 0;
    int wrote = 0;
    for (int i = 0; i < g->nitems; i++) {
        if (g->items[i].picked) continue;
        if ((wrote + 1) * 3 > cap) break;
        out[wrote * 3 + 0] = g->items[i].x;
        out[wrote * 3 + 1] = g->items[i].y;
        out[wrote * 3 + 2] = g->items[i].type;
        wrote++;
    }
    return wrote;
}

int rc_game_visibility(const void *handle, uint8_t *out, size_t cap) {
    const Game *g = (const Game *)handle;
    if (!g || !out) return -1;
    size_t n = (size_t)g->w * (size_t)g->h;
    if (cap < n) return -1;
    for (size_t i = 0; i < n; i++) {
        if (g->visible[i]) out[i] = RC_VIS_VISIBLE;
        else if (g->explored[i]) out[i] = RC_VIS_EXPLORED;
        else out[i] = RC_VIS_UNSEEN;
    }
    return (int)n;
}
