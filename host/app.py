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

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIB_PATH = os.path.join(ROOT, "libroguecore.so")

RC_TILE_WALL = 0
RC_TILE_FLOOR = 1
RC_TILE_STAIRS = 2

HELP_TEXT = """
【怎麼玩】
  @ = 你（玩家）　. = 可走地板　# = 牆（不能走）　> = 樓梯（走到這格就贏）
  在底下一行的提示符「> 」後面輸入一個字母再按 Enter：
    w = 上　s = 下　a = 左　d = 右
    ? 或 h = 再看這段說明　q = 結束遊戲
目標：用 WASD 把 @ 移到地圖上的「>」那一格。
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


def tile_char(t: int, is_player: bool) -> str:
    if is_player:
        return "@"
    if t == RC_TILE_WALL:
        return "#"
    if t == RC_TILE_FLOOR:
        return "."
    if t == RC_TILE_STAIRS:
        return ">"
    return "?"


def main() -> None:
    ap = argparse.ArgumentParser(description="Roguelike Host（Python + C .so）")
    ap.add_argument("--seed", type=int, default=1, help="隨機種子")
    ap.add_argument("--smoke", action="store_true", help="載入函式庫並做最小 API 測試後結束")
    args = ap.parse_args()

    lib = load_lib()
    if args.smoke:
        run_smoke(lib)
        return

    g = lib.rc_game_create(c_uint32(args.seed & 0xFFFFFFFF))
    if not g:
        print("rc_game_create 失敗", file=sys.stderr)
        sys.exit(1)
    w = lib.rc_game_width(g)
    h = lib.rc_game_height(g)
    n = w * h
    buf = (ctypes.c_uint8 * n)()

    print(f"Python Host + C 核心 | seed={args.seed}")
    print(HELP_TEXT)
    print()

    try:
        while True:
            if lib.rc_game_tiles(g, buf, n) < 0:
                print("rc_game_tiles 失敗", file=sys.stderr)
                break
            px = c_int()
            py = c_int()
            lib.rc_game_player(g, ctypes.byref(px), ctypes.byref(py))
            for y in range(h):
                row = []
                for x in range(w):
                    row.append(
                        tile_char(buf[y * w + x], x == px.value and y == py.value)
                    )
                print("".join(row))
            if lib.rc_game_done(g):
                print("你抵達樓梯，勝利！")
                break
            s = input("移動 (w/a/s/d，? 說明，q 離開) > ").strip().lower()
            if not s:
                continue
            c = s[0]
            if c == "q":
                break
            if c == "?" or c == "h":
                print(HELP_TEXT)
                print()
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
                continue
            mv = lib.rc_game_move(g, dx, dy)
            if mv == 1:
                print("(撞牆)")
            elif mv < 0:
                print("(無效移動)")
    finally:
        lib.rc_game_destroy(g)


if __name__ == "__main__":
    main()
