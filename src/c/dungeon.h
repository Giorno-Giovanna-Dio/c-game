#ifndef DUNGEON_H
#define DUNGEON_H

#include "rng.h"
#include <stddef.h>
#include <stdint.h>

#define DUN_TILE_WALL 0u
#define DUN_TILE_FLOOR 1u
#define DUN_TILE_STAIRS 2u

/**
 * 以房間 + 走廊演算法填滿 tiles（長度 w*h，row-major）。
 * 回傳樓梯格索引、玩家起始格索引（皆為線性索引）。
 */
void rc_dungeon_generate(rc_rng_t *rng, int w, int h, uint8_t *tiles, int *out_stairs_idx,
                         int *out_player_idx);

#endif
