/**
 * roguecore — C 核心公開 API（供 Python ctypes 與 CLI 使用）
 * 地磚數值與課程報告中的「資料格式」章節一致。
 */
#ifndef ROGUECORE_H
#define ROGUECORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RC_TILE_WALL 0u
#define RC_TILE_FLOOR 1u
#define RC_TILE_STAIRS 2u

/** 建立遊戲；寬高固定於實作內（見 RC_GAME_WIDTH/HEIGHT） */
void *rc_game_create(uint32_t seed);
void rc_game_destroy(void *handle);

int rc_game_width(const void *handle);
int rc_game_height(const void *handle);

/** 移動玩家；(dx,dy) ∈ {(0,±1),(±1,0)}；0 成功、1 撞牆、-1 參數錯誤 */
int rc_game_move(void *handle, int dx, int dy);

void rc_game_player(const void *handle, int *out_x, int *out_y);

/**
 * 寫入地圖地磚（不含玩家標記）：row-major，長度 width*height。
 * 回傳寫入位元組數，失敗回傳 -1。
 */
int rc_game_tiles(const void *handle, uint8_t *out, size_t cap);

/** 玩家是否站在樓梯上 */
int rc_game_done(const void *handle);

#define RC_GAME_WIDTH 42
#define RC_GAME_HEIGHT 22

#ifdef __cplusplus
}
#endif

#endif
