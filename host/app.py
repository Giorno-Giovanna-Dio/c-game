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

HELP_TEXT = """
【怎麼玩】
  @ = 你（玩家）　地板 / 牆 / 樓梯見畫面圖例
  互動終端機：直接按鍵，不必 Enter — w 上 s 下 a 左 d 右（方向鍵亦可）
  ? 或 h = 說明　q = 結束
  若改為「每行輸入」模式：請加參數 --line-mode（管道或非 TTY 時會自動啟用）
目標：把 @ 移到樓梯那一格。
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
    lib.rc_game_done.argtypes = [c_void_p]
    lib.rc_game_done.restype = c_int
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
    status = "WASD 移動，? 說明，q 離開"
    R = TermStyle.RESET

    def draw_frame() -> None:
        if not line_mode:
            clear_screen()
        if use_color:
            print(f"{TermStyle.TITLE}c-game{R}  Python Host + C 核心  seed={args.seed}")
        else:
            print(f"c-game  Python Host + C 核心  seed={args.seed}")
        print()
        if lib.rc_game_tiles(g, buf, n) < 0:
            print("rc_game_tiles 失敗", file=sys.stderr)
            return
        px = c_int()
        py = c_int()
        lib.rc_game_player(g, ctypes.byref(px), ctypes.byref(py))
        row_bytes = [int(buf[i]) for i in range(n)]
        for line in format_map_lines(row_bytes, w, h, px.value, py.value, color=use_color):
            print(line)
        print()
        if use_color:
            print(f"{TermStyle.HUD}# 牆　· 地板　> 樓梯　@ 你{R}")
            print(f"{TermStyle.DIM}{status}{R}")
        else:
            print("# 牆  . 地板  > 樓梯  @ 你")
            print(status)

    try:
        term.enter()
        if line_mode:
            print(HELP_TEXT)
            print()

        while True:
            draw_frame()
            if lib.rc_game_done(g):
                if use_color:
                    print(f"\n{TermStyle.TITLE}你抵達樓梯，勝利！{R}\n")
                else:
                    print("\n你抵達樓梯，勝利！\n")
                break

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
            if mv == 1:
                status = "撞牆"
            elif mv < 0:
                status = "無效移動"
            else:
                status = "WASD 移動，? 說明，q 離開"
    finally:
        term.leave()
        lib.rc_game_destroy(g)


if __name__ == "__main__":
    main()
