"""
終端機 UI 輔助：單鍵輸入（免 Enter）、清屏、ANSI 色彩。
目標環境：Linux / macOS 的互動式終端機（Ubuntu 24.04 課程環境）。
"""
from __future__ import annotations

import os
import sys
import types

class TermStyle:
    RESET = "\033[0m"
    DIM = "\033[2m"
    BOLD = "\033[1m"
    WALL = "\033[38;5;237m"
    FLOOR = "\033[38;5;245m"
    PLAYER = "\033[1;93m"
    STAIRS = "\033[1;92m"
    MONSTER = "\033[1;91m"
    HUD = "\033[36m"
    WARN = "\033[1;91m"
    TITLE = "\033[1;97m"
    HP_GOOD = "\033[1;92m"
    HP_MED = "\033[1;33m"
    HP_LOW = "\033[1;91m"


def clear_screen() -> None:
    sys.stdout.write("\033[2J\033[H")
    sys.stdout.flush()


def cursor_hide() -> None:
    if sys.stdout.isatty():
        sys.stdout.write("\033[?25l")
        sys.stdout.flush()


def cursor_show() -> None:
    if sys.stdout.isatty():
        sys.stdout.write("\033[?25h")
        sys.stdout.flush()


def read_key_raw() -> str:
    """從 stdin 讀取一個位元組（已處於 raw 時呼叫）；回傳字元或空字串。"""
    data = os.read(sys.stdin.fileno(), 1)
    if not data:
        return ""
    return data.decode("utf-8", errors="replace")


def read_key_interactive() -> str:
    """
    互動模式：單鍵、不需 Enter。
    方向鍵會轉成 w/a/s/d（若終端機送出 ESC 序列）。
    """
    if sys.platform == "win32":
        try:
            import msvcrt

            ch = msvcrt.getwch()
            return ch.lower() if ch else ""
        except Exception:
            return ""

    import select
    import termios
    import tty

    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        b = os.read(fd, 1)
        if not b:
            return ""
        if b == b"\x1b":
            # 嘗試讀完整方向鍵序列（逾時則當作 ESC）
            if select.select([fd], [], [], 0.05)[0]:
                rest = os.read(fd, 2)
                seq = b + rest
                if seq == b"\x1b[A":
                    return "w"
                if seq == b"\x1b[B":
                    return "s"
                if seq == b"\x1b[D":
                    return "a"
                if seq == b"\x1b[C":
                    return "d"
            return "\x1b"
        return b.decode("utf-8", errors="replace")
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def read_line_fallback(prompt: str) -> str:
    return input(prompt).strip().lower()


def wrap_terminal_ui(line_mode: bool, color: bool):
    """進入遊戲前隱藏游標，離開時還原（僅互動 + 非 line_mode）。"""
    ctx = types.SimpleNamespace(line_mode=line_mode, color=color, _armed=False)

    def enter() -> None:
        if line_mode or not sys.stdin.isatty():
            return
        cursor_hide()
        ctx._armed = True

    def leave() -> None:
        if ctx._armed:
            cursor_show()
            ctx._armed = False

    ctx.enter = enter
    ctx.leave = leave
    return ctx


def hp_color(hp: int, max_hp: int) -> str:
    ratio = hp / max(max_hp, 1)
    if ratio > 0.6:
        return TermStyle.HP_GOOD
    if ratio > 0.3:
        return TermStyle.HP_MED
    return TermStyle.HP_LOW


def format_hp_bar(hp: int, max_hp: int, *, color: bool, width: int = 20) -> str:
    filled = int(width * hp / max(max_hp, 1))
    if filled < 0:
        filled = 0
    bar = "█" * filled + "░" * (width - filled)
    label = f"HP {hp}/{max_hp}"
    if color:
        c = hp_color(hp, max_hp)
        return f"{c}{label} [{bar}]{TermStyle.RESET}"
    return f"{label} [{bar}]"


def format_map_lines(
    buf: list[int],
    w: int,
    h: int,
    px: int,
    py: int,
    *,
    color: bool,
    monsters: list[tuple[int, int, int]] | None = None,
) -> list[str]:
    mon_set: dict[tuple[int, int], int] = {}
    if monsters:
        for mx, my, mhp in monsters:
            mon_set[(mx, my)] = mhp

    lines: list[str] = []
    R = TermStyle.RESET
    for y in range(h):
        parts: list[str] = []
        for x in range(w):
            is_p = x == px and y == py
            is_m = (x, y) in mon_set
            t = buf[y * w + x]
            if is_p:
                sym = "@"
                if color:
                    parts.append(f"{TermStyle.PLAYER}{sym}{R}")
                else:
                    parts.append(sym)
                continue
            if is_m:
                sym = "E"
                if color:
                    parts.append(f"{TermStyle.MONSTER}{sym}{R}")
                else:
                    parts.append(sym)
                continue
            if t == 0:
                sym = "█" if color else "#"
                if color:
                    parts.append(f"{TermStyle.WALL}{sym}{R}")
                else:
                    parts.append("#")
            elif t == 1:
                sym = "·" if color else "."
                if color:
                    parts.append(f"{TermStyle.FLOOR}{sym}{R}")
                else:
                    parts.append(".")
            elif t == 2:
                sym = ">"
                if color:
                    parts.append(f"{TermStyle.STAIRS}{sym}{R}")
                else:
                    parts.append(">")
            else:
                parts.append("?")
        lines.append("".join(parts))
    return lines
