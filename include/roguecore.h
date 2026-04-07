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

/**
 * 嘗試下樓：若玩家站在樓梯上則進入下一層，回傳 1。
 * 若已到最終層（RC_MAX_FLOORS）則回傳 2（勝利）。
 * 否則回傳 0（不在樓梯上）。
 */
int rc_game_descend(void *handle);

/** 玩家是否已通關（完成所有層） */
int rc_game_won(const void *handle);

/** 目前層數（1-based） */
int rc_game_floor(const void *handle);

/** 玩家是否死亡（HP <= 0） */
int rc_game_dead(const void *handle);

/** 玩家血量 */
int rc_game_player_hp(const void *handle);
int rc_game_player_max_hp(const void *handle);

/** 怪物數量（含死亡） */
int rc_game_monster_count(const void *handle);

/**
 * 寫入怪物資料（存活的）。每隻寫 4 個 int：x, y, hp, type。
 * out 長度至少 cap 個 int。回傳寫入的怪物數。
 */
int rc_game_monsters(const void *handle, int *out, int cap);

#define RC_MON_ZOMBIE 0
#define RC_MON_SLIME 1
#define RC_MON_HUNTER 2

/**
 * rc_game_move 回傳值：
 *   0 = 移動成功
 *   1 = 撞牆
 *   2 = 攻擊怪物（已移動扣血）
 *   3 = 玩家死亡
 *  -1 = 參數錯誤
 */

/** 最近一次戰鬥的傷害訊息（長度上限 128）。無戰鬥時回傳空字串。 */
const char *rc_game_last_message(const void *handle);

/**
 * 寫入視野資料（row-major，長度 w*h）。
 * 值：0 = 未探索、1 = 已探索但不在視野、2 = 目前可見。
 */
int rc_game_visibility(const void *handle, uint8_t *out, size_t cap);

/** 剩餘步數 / 該層最大步數 */
int rc_game_steps_left(const void *handle);
int rc_game_steps_max(const void *handle);

/** 步數是否耗盡 */
int rc_game_timeout(const void *handle);

#define RC_VIS_UNSEEN 0u
#define RC_VIS_EXPLORED 1u
#define RC_VIS_VISIBLE 2u

#define RC_GAME_WIDTH 42
#define RC_GAME_HEIGHT 22
#define RC_MAX_MONSTERS 16
#define RC_MAX_ITEMS 12
#define RC_PLAYER_START_HP 25
#define RC_MAX_FLOORS 5
#define RC_BASE_STEPS 120

#define RC_ITEM_POTION 0
#define RC_ITEM_BLINK 1
#define RC_ITEM_MAP 2

/**
 * 寫入道具資料（未拾取的）。每個寫 3 個 int：x, y, type。
 * 回傳寫入的道具數。
 */
int rc_game_items(const void *handle, int *out, int cap);

#ifdef __cplusplus
}
#endif

#endif
