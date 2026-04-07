#!/usr/bin/env python3
"""
Host 層：以 ctypes 載入 libroguecore.so，提供終端機遊玩介面。
C 核心負責地城生成與規則；此檔僅處理顯示與輸入。
"""
from __future__ import annotations

import argparse
import ctypes
import os
import sys
from ctypes import c_int, c_uint32, c_void_p

from terminal_ui import (
    TermStyle,
    clear_screen,
    format_hp_bar,
    format_map_lines,
    read_key_interactive,
    read_line_fallback,
    wrap_terminal_ui,
)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIB_PATH = os.path.join(ROOT, "libroguecore.so")

RC_TILE_WALL = 0
RC_TILE_FLOOR = 1
RC_TILE_STAIRS = 2
RC_MAX_MONSTERS = 16
RC_MAX_ITEMS = 12
RC_ITEM_POTION = 0
RC_ITEM_BLINK = 1
RC_ITEM_MAP = 2

HELP_TEXT = """
【怎麼玩】
  @ = 你（玩家）　E = 怪物（走進牠就攻擊）
  ! = 治療藥水（走上去自動拾取，回復 HP）
  ~ = 閃現卷軸（走上去自動拾取，瞬移到隨機位置）
  % = 地圖卷軸（走上去自動拾取，揭示整層地形）
  地板 / 牆 / 樓梯見畫面圖例
  互動終端機：直接按鍵，不必 Enter — w 上 s 下 a 左 d 右（方向鍵亦可）
  ? 或 h = 說明　q = 結束
目標：走到樓梯 > 下樓，征服全部 5 層地城！越深怪越多越強。
每層有步數限制，耗盡後每步扣 HP！善用道具生存下來！
""".strip()


def load_lib() -> ctypes.CDLL:
    if not os.path.isfile(LIB_PATH):
        print(f"找不到 {LIB_PATH}，請先在專案根目錄執行: make", file=sys.stderr)
        sys.exit(1)
    lib = ctypes.CDLL(LIB_PATH)
    lib.rc_game_create.argtypes = [c_uint32]
    lib.rc_game_create.restype = c_void_p
    lib.rc_game_destroy.argtypes = [c_void_p]
    lib.rc_game_destroy.restype = None
    lib.rc_game_width.argtypes = [c_void_p]
    lib.rc_game_width.restype = c_int
    lib.rc_game_height.argtypes = [c_void_p]
    lib.rc_game_height.restype = c_int
    lib.rc_game_move.argtypes = [c_void_p, c_int, c_int]
    lib.rc_game_move.restype = c_int
    lib.rc_game_player.argtypes = [c_void_p, ctypes.POINTER(c_int), ctypes.POINTER(c_int)]
    lib.rc_game_player.restype = None
    lib.rc_game_tiles.argtypes = [c_void_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    lib.rc_game_tiles.restype = c_int
    lib.rc_game_descend.argtypes = [c_void_p]
    lib.rc_game_descend.restype = c_int
    lib.rc_game_won.argtypes = [c_void_p]
    lib.rc_game_won.restype = c_int
    lib.rc_game_floor.argtypes = [c_void_p]
    lib.rc_game_floor.restype = c_int
    lib.rc_game_dead.argtypes = [c_void_p]
    lib.rc_game_dead.restype = c_int
    lib.rc_game_player_hp.argtypes = [c_void_p]
    lib.rc_game_player_hp.restype = c_int
    lib.rc_game_player_max_hp.argtypes = [c_void_p]
    lib.rc_game_player_max_hp.restype = c_int
    lib.rc_game_monster_count.argtypes = [c_void_p]
    lib.rc_game_monster_count.restype = c_int
    lib.rc_game_monsters.argtypes = [c_void_p, ctypes.POINTER(c_int), c_int]
    lib.rc_game_monsters.restype = c_int
    lib.rc_game_last_message.argtypes = [c_void_p]
    lib.rc_game_last_message.restype = ctypes.c_char_p
    lib.rc_game_visibility.argtypes = [c_void_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    lib.rc_game_visibility.restype = c_int
    lib.rc_game_steps_left.argtypes = [c_void_p]
    lib.rc_game_steps_left.restype = c_int
    lib.rc_game_steps_max.argtypes = [c_void_p]
    lib.rc_game_steps_max.restype = c_int
    lib.rc_game_timeout.argtypes = [c_void_p]
    lib.rc_game_timeout.restype = c_int
    lib.rc_game_items.argtypes = [c_void_p, ctypes.POINTER(c_int), c_int]
    lib.rc_game_items.restype = c_int
    return lib


def run_smoke(lib: ctypes.CDLL) -> None:
    g = lib.rc_game_create(7)
    if not g:
        raise RuntimeError("rc_game_create 回傳 NULL")
    w = lib.rc_game_width(g)
    h = lib.rc_game_height(g)
    if w < 8 or h < 8:
        raise RuntimeError("unexpected map size")
    n = w * h
    buf = (ctypes.c_uint8 * n)()
    if lib.rc_game_tiles(g, buf, n) != n:
        raise RuntimeError("rc_game_tiles 失敗")
    px = c_int()
    py = c_int()
    lib.rc_game_player(g, ctypes.byref(px), ctypes.byref(py))
    lib.rc_game_destroy(g)


def main() -> None:
    ap = argparse.ArgumentParser(description="Roguelike Host（Python + C .so）")
    ap.add_argument("--seed", type=int, default=1, help="隨機種子")
    ap.add_argument("--smoke", action="store_true", help="載入函式庫並做最小 API 測試後結束")
    ap.add_argument(
        "--line-mode",
        action="store_true",
        help="每行輸入一個指令並按 Enter（管道、除錯用；非 TTY 時自動啟用）",
    )
    ap.add_argument("--no-color", action="store_true", help="關閉 ANSI 色彩")
    args = ap.parse_args()

    lib = load_lib()
    if args.smoke:
        run_smoke(lib)
        return

    line_mode = args.line_mode or not sys.stdin.isatty()
    use_color = not args.no_color and sys.stdout.isatty() and not os.environ.get("NO_COLOR")
    term = wrap_terminal_ui(line_mode, use_color)

    g = lib.rc_game_create(c_uint32(args.seed & 0xFFFFFFFF))
    if not g:
        print("rc_game_create 失敗", file=sys.stderr)
        sys.exit(1)
    w = lib.rc_game_width(g)
    h = lib.rc_game_height(g)
    n = w * h
    buf = (ctypes.c_uint8 * n)()
    vis_buf = (ctypes.c_uint8 * n)()
    mon_buf = (c_int * (RC_MAX_MONSTERS * 3))()
    item_buf = (c_int * (RC_MAX_ITEMS * 3))()
    status = "WASD 移動，? 說明，q 離開"
    R = TermStyle.RESET

    def get_monsters() -> list[tuple[int, int, int]]:
        cnt = lib.rc_game_monsters(g, mon_buf, RC_MAX_MONSTERS * 3)
        return [(int(mon_buf[i * 3]), int(mon_buf[i * 3 + 1]), int(mon_buf[i * 3 + 2]))
                for i in range(cnt)]

    def get_items() -> list[tuple[int, int, int]]:
        cnt = lib.rc_game_items(g, item_buf, RC_MAX_ITEMS * 3)
        return [(int(item_buf[i * 3]), int(item_buf[i * 3 + 1]), int(item_buf[i * 3 + 2]))
                for i in range(cnt)]

    def get_combat_msg() -> str:
        raw = lib.rc_game_last_message(g)
        if raw:
            return raw.decode("utf-8", errors="replace")
        return ""

    def draw_frame() -> None:
        if not line_mode:
            clear_screen()
        hp = lib.rc_game_player_hp(g)
        max_hp = lib.rc_game_player_max_hp(g)
        fl = lib.rc_game_floor(g)
        steps = lib.rc_game_steps_left(g)
        steps_max = lib.rc_game_steps_max(g)
        step_warn = steps <= steps_max // 4
        if use_color:
            step_color = TermStyle.WARN if step_warn else TermStyle.DIM
            print(f"{TermStyle.TITLE}c-game{R}  B{fl}F  {format_hp_bar(hp, max_hp, color=True)}  {step_color}步數 {steps}/{steps_max}{R}")
        else:
            print(f"c-game  B{fl}F  {format_hp_bar(hp, max_hp, color=False)}  步數 {steps}/{steps_max}")
        print()
        if lib.rc_game_tiles(g, buf, n) < 0:
            print("rc_game_tiles 失敗", file=sys.stderr)
            return
        px = c_int()
        py = c_int()
        lib.rc_game_player(g, ctypes.byref(px), ctypes.byref(py))
        lib.rc_game_visibility(g, vis_buf, n)
        row_bytes = [int(buf[i]) for i in range(n)]
        vis_bytes = [int(vis_buf[i]) for i in range(n)]
        monsters = get_monsters()
        items_list = get_items()
        for line in format_map_lines(row_bytes, w, h, px.value, py.value, color=use_color, monsters=monsters, items=items_list, visibility=vis_bytes):
            print(line)
        print()
        alive = lib.rc_game_monster_count(g)
        n_items = len(items_list)
        if use_color:
            print(f"{TermStyle.HUD}# 牆　· 地板　> 樓梯　@ 你　E 怪物({alive})　! 藥水　~ 閃現　% 地圖({n_items}){R}")
            print(f"{TermStyle.DIM}{status}{R}")
        else:
            print(f"# 牆  . 地板  > 樓梯  @ 你  E 怪物({alive})  ! 藥水  ~ 閃現  % 地圖({n_items})")
            print(status)

    try:
        term.enter()
        if line_mode:
            print(HELP_TEXT)
            print()

        while True:
            draw_frame()
            if lib.rc_game_dead(g):
                if use_color:
                    print(f"\n{TermStyle.WARN}你被怪物擊殺，遊戲結束！{R}\n")
                else:
                    print("\n你被怪物擊殺，遊戲結束！\n")
                break
            if lib.rc_game_won(g):
                if use_color:
                    print(f"\n{TermStyle.TITLE}你征服了全部地城，勝利！{R}\n")
                else:
                    print("\n你征服了全部地城，勝利！\n")
                break
            desc = lib.rc_game_descend(g)
            if desc == 2:
                cmsg = get_combat_msg()
                status = cmsg if cmsg else "通關！"
                draw_frame()
                if use_color:
                    print(f"\n{TermStyle.TITLE}你征服了全部地城，勝利！{R}\n")
                else:
                    print("\n你征服了全部地城，勝利！\n")
                break
            elif desc == 1:
                cmsg = get_combat_msg()
                status = cmsg if cmsg else f"進入第 {lib.rc_game_floor(g)} 層！"
                continue

            if line_mode:
                s = read_line_fallback("移動 (w/a/s/d，? 說明，q 離開) > ")
                if not s:
                    continue
                c = s[0]
            else:
                try:
                    ch = read_key_interactive()
                    c = ch.lower() if ch else ""
                except KeyboardInterrupt:
                    status = "已中斷（Ctrl+C）"
                    draw_frame()
                    break
                if not c or c in ("\r", "\n"):
                    continue

            if c == "\x03":
                status = "已結束"
                break
            if c == "q":
                status = "已離開"
                break
            if c in ("?", "h"):
                clear_screen()
                print(HELP_TEXT)
                print()
                if line_mode:
                    read_line_fallback("按 Enter 回到遊戲…")
                else:
                    print("按任意鍵回到遊戲…")
                    read_key_interactive()
                status = "說明已關閉"
                continue

            dx, dy = 0, 0
            if c == "w":
                dy = -1
            elif c == "s":
                dy = 1
            elif c == "a":
                dx = -1
            elif c == "d":
                dx = 1
            else:
                if line_mode:
                    continue
                status = "請用 WASD、方向鍵、?、q"
                continue

            mv = lib.rc_game_move(g, dx, dy)
            cmsg = get_combat_msg()
            if mv == 1:
                status = "撞牆"
            elif mv == 2:
                status = cmsg if cmsg else "攻擊！"
            elif mv == 3:
                status = cmsg if cmsg else "你死了！"
            elif mv < 0:
                status = "無效移動"
            else:
                if cmsg:
                    status = cmsg
                else:
                    status = "WASD 移動，? 說明，q 離開"
    finally:
        term.leave()
        lib.rc_game_destroy(g)


if __name__ == "__main__":
    main()
