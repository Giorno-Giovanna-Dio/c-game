"""
終端機 UI 輔助：單鍵輸入（免 Enter）、清屏、ANSI 色彩。
目標環境：Linux / macOS 的互動式終端機（Ubuntu 24.04 課程環境）。
"""
from __future__ import annotations

import os
import re
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
    MONSTER_SLIME = "\033[1;92m"
    MONSTER_HUNTER = "\033[1;95m"
    HUD = "\033[36m"
    WARN = "\033[1;91m"
    TITLE = "\033[1;97m"
    HP_GOOD = "\033[1;92m"
    HP_MED = "\033[1;33m"
    HP_LOW = "\033[1;91m"
    FOG_EXPLORED = "\033[38;5;239m"
    FOG_UNSEEN = "\033[38;5;233m"
    ITEM_POTION = "\033[1;95m"
    ITEM_BLINK = "\033[1;96m"
    ITEM_MAP = "\033[1;33m"


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


VIS_UNSEEN = 0
VIS_EXPLORED = 1
VIS_VISIBLE = 2


ITEM_SYMS = {0: "!", 1: "~", 2: "%"}
ITEM_COLORS = {0: TermStyle.ITEM_POTION, 1: TermStyle.ITEM_BLINK, 2: TermStyle.ITEM_MAP}


MON_SYMS = {0: "E", 1: "S", 2: "H"}
MON_COLORS = {
    0: TermStyle.MONSTER,
    1: TermStyle.MONSTER_SLIME,
    2: TermStyle.MONSTER_HUNTER,
}
MON_NAMES = {0: "殭屍", 1: "史萊姆", 2: "獵人"}
ITEM_NAMES = {0: "治療藥水", 1: "閃現卷軸", 2: "地圖卷軸"}
ITEM_DESC = {
    0: "走上去拾取回血",
    1: "瞬移到安全位置",
    2: "揭示整層地形",
}

# 玩家九宮格內出現對應角色時，右側顯示專屬 ASCII art（執行時裁切置中）
# key: ("mon", type) 或 ("item", type)
NEARBY_ART: dict[tuple[str, int], list[str]] = {}

_ART_ZOMBIE: list[str] = [
    "            ..-*%%%###%#+=.",
    "          :+#--------::---+.",
    "       :##*+*+=-----------:**-.",
    "     .=*++++++**+==--===----=%**##.",
    "     #**++++++==#=##-=#@+*--=%%@@@*",
    "    +#*##++++===+#=**:.   -=@*:.. *",
    "    %****#*++===+*-.....  .##+:.:=.",
    "    @******++=+==%=:.....-+*+=#+:*",
    "    *#*******=*-=+##*++**+=*++%++:",
    "  .+#**##%%*+%+===++++++=@%*=-=+",
    " .%******+*+++#%%*===**#=##==+*#+:",
    " :@**##*****+++%=-=%#.*# %++=%.+",
    "  +#*****####++#-*%+%==%%+%%##==",
    "   .==:..=%#*==---+%%#=%#-==+**=  .+##:      +==*#*-",
    "        .**==+*#*=-*##--=+%++*#+=-=#+:::-=*##=---=--=*#*=:::::.",
    "       .**++======-===-*+=++*%###+=--:::::--=--==+==##*=*+=++===+:",
    "       :#*++++==+++++=#@#+++=-----------------*#+%--+#=--*:-@---:=.",
    "       :#***++++++++++%%%%#****=-=-=---+**==-.   *#---+*-++=#--*=-*",
    "        *++++#%#*+++++%%#@=%%#*+-.#@%%+.          -@*---*-@****:*==%",
    "        -@**+==+####**##%#=%                        #==*=-.=-   *=-=*",
    "         :#++=-==+++*#%%==-+:                    ++ *--*=*      *=-=*:",
    "          +*++========+===--#                   -+=*-=+:*++ +=:==-+#+=",
    "          :#+++=========+=--*                     :++#*=#=  *+===#=%**",
    "          =#*+++==========-+=                          :     :%=#==%=.",
    "           =#*++===========-*:                                 ===-",
    "            -##+==#=========-#:",
    "            +#*+==-+========--*:",
    "            %+*++==*=-==*==-=+*",
    "           =***++==*=+=+#%#%*-               .:::",
    "             :%%+=-*-+%@#++#..               *=+*+",
    "             +%#%%+#*******##.              :+-=**.",
    "             ###*#********+#=              -+-=**=+==",
    "             ###*********+*%              *+---==-+**%",
    "             +%#*******+*##*-:            -+-------=##",
    "             -%#***+**%@%%#**+*+          :#----=--=+",
    "             ##***+#**%@@#%#***+*#-::.    .#------=#-",
    "            -#****%.  .:+%%%##******%#-   .#----=%=:",
    "           =%#***#*       +##%#*****=#%*:.-#----%",
    "           %##****          --%##%%###+*-=+=---+-",
    "          +#*%**@=             +#%@%%# .%+===--++",
    "        -*##***%-               :+*=-    -+==---%",
    "      -%#******=                          :+===+*",
    "      .*##%#**#:                           --+*+.",
    "       =*+*%-#:                              :.",
    "    ..**=+=",
    "   -%=--=#+.",
    " .#=%=-----=#=.    ..",
    "-#+=-------------==++--.-",
    " .=++===+%+--------#+*+*+",
    "          *++---==**--=-",
    "           ....*%#*=",
]

_ART_SLIME: list[str] = [
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%%##****##%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@%#*----------------::=*%%@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@%=-------------------------:.=%@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@%+----------------------------:...:-#@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@*=-------------------------------:.....-#@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@%------------------------------------:.....:#@@@@@@@@@@@@",
    "@@@@@@@@@@@@=----------------------------------------:... =@@@@@@@@@@@",
    "@@@@@@@@@@%---------------------------------------:..:---.:-@@@@@@@@@@",
    "@@@@@@@@@#-====------+##*----------------##*=-------::--===--@@@@@@@@@",
    "@@@@@@@@#-=====-----=@@@-#--------------@@@-#-----------====--@@@@@@@@",
    "@@@@@@@#-=====------=@@@@%---=*%@@@@%=--#@@@*----------======-*@@@@@@@",
    "@@@@@@@-======-------====----========----===-----------====++=-%@@@@@@",
    "@@@@@@+======------------------------------------------=====++=*@@@@@@",
    "@@@@@@========-----------------------------------------======+==@@@@@@",
    "@@@@@*========----------------------------------------=======++=*@@@@@",
    "@@@@*-=========-------------------------------------=========++=-*@@@@",
    "@@@+.===========--------------------------------=============+++=-*@@@",
    "@@@.-=============---------------------====================++++++--@@@",
    "@@@.================--------------=======================++++++++=:@@@",
    "@@@=:==================-------========================++++++++++=:*@@@",
    "@@@@*:============================================+++++++++++++-:#@@@@",
    "@@@@@@*+--=======+++=++++++++==============++++++++++++++++=--+#@@@@@@",
    "@@@@@@@@@@#+=========++++++++++==========++++++++++========+%@@@@@@@@@",
    "@@@@@@@@@@@@@@@@%#+---===========++=============++*##%%@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
]

_ART_HUNTER: list[str] = [
    "               .:-:--:",
    "             .-:::::::::",
    "            .---=++++**+:..",
    "            =+**+=--:-==+--=",
    "            ++=:--++=:-==:.                 ...",
    "          .---++#--:=--=:   ::::....-=::::=**##            .",
    "         .-:-===*--==-::-.  #****%****+**@#**##  .....:--==##*",
    "            .++--+++=+=+*=  +*+==**++********####+=--:..",
    "              -=-=--::--:=##############--*%%%%#**",
    "       :----+**##*#*=:**####%%%%##*+--====..",
    "     :+=-------===--:::-+***:      ::-:..",
    "    :==::::::::---==-:-===*==:.   ::-.",
    "    -===---===++====+==++*+++====::::",
    "      -=+++=+#*=+=+==*==+=+++===-::-",
    "          :*++++=+===+=++==*+==-::::",
    "            +=*+=+===++==+= :------",
    "            -+=+=========+=",
    "             +*++====+*++++-..:::----:",
    "             +*=+==++++=*##************:",
    "              ####*###**#*##************",
    "             *%%##**#**###%#*****#******",
    "             =%#******##%%#**###%###***#.",
    "             +#********#%%%%%#%**##***#*",
    "             ######*****==::..  -%#***#*",
    " ::::===-.   %###%#****:        -###***+",
    "-****#####=--#*#%#*****        .*%#####*",
    "++******####%%####****:        +#*#***++.",
    "=+*++++*#**####******+         +###*++++.",
    " ++*+++##****#*******         .*###**++*+++=",
    ".++*****##*********+          .***++++++++++-",
    " ++++++**:.:::-::::...........=**+=++++++++==",
    " .-====-:.....................:--:.:-::::.....",
    "            .....................",
]

_ART_POTION: list[str] = [
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ ::-==+*%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@:.:-+*#%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@:::=*#%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@+==*#%%@@%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@*+=-::===+++**@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@++=--+**##*+#*@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*=-:+*#%%#=#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@-::=+***=-@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@:..::::.::@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@::.------.@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ :-:==++*+:@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@==::--+**+-:@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@--:.::-=+**+-:@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@::-:..::-=+*#++--:@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@:-:....::-=+*###*=:-@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@:.-:....::--=+*####**=--@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@ .-:.....:::--=+***####**+:@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@-:::.  ..::----==+++*######+=-@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@----:   :::----=--==++*##%#%%*+-@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@:--:.  .::---==--===++*##%%%%#*=@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@=:---:...:---------=++**##%%%%%#+=@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@::==--::..::--------=++###%@%%%%*=@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@-:==+=--:::::::--:-:-***#%%%%%%%*=@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@-*+=+=-::----:::.:::-+*%##%%@%%+@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@-=**+=-====-==+++++++#*#%%%%#%**@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@+*=**#%%***+===++******#%@@@%*#+@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@+=*+*#%%%##*******#%%%%%##*#@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@*##%#**#%%%%%%%%%%%%%@@@%%@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@##******++**#########%%%@@*@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@%#%%%####%%%%%%%%%*##%%%@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
]

_ART_BLINK: list[str] = [
    "                                                ..",
    "                                              +%@@@#-",
    "                                             %@@@@@@@:",
    "                                             @@@@@%@@+",
    "                                             -%%@%@%#",
    "                                              .-++=:",
    "                                :+++++++++++++++=.",
    "                               %%@@@@@@@@@@@@@@@@%-",
    "                             -%%@%=---#@@@@@@%@@@@@+",
    "                            +%@@#:   -@@@@@@@@%@@@@@*",
    "                          .%@%%*    =@@@@@@@@@%-*@@@@#.......",
    "                          %@@%:    +@@@@@@@@@%   +@@%@%%%%%%%@.",
    "                          .=-.    =%@@@@@@@%#.    -%@@@@@@%@@*",
    "                                  %@@@@@%@@#.        .......",
    "               .+***********-     -%@@@@@@@-",
    "                *###########-   %# :%%@@%%@%@*",
    "                              :%@@%+:.-#@@@@@@%#+",
    "           .@@@@@%@@@@@@%    -%@@@@%:   .-*%@@@@@%=",
    "            .:::::::::::    =@%@%%*      .:+%@@@@@%",
    "         ...........       +@%@@%-    .+%@@@@@@%%%.",
    "       .@@@@@@@@@@@@%     *@@@@#.   +%@@@@@@%+=:.",
    "         ...........    .#@@@%=     -##*=-:",
    "                       .%@@@#:",
    "                      :@@@%*",
    "                     =%@@%:",
    "                     +@@*.",
    "                      ..",
]

_ART_MAP: list[str] = [
    "   ..                                             ..",
    "              #*+=",
    "             .@%#+@#.",
    "             ##=:-++%#-:",
    "            -@-=:::::+*%#:",
    "            +*+::::::::++@%-::::=+++*####%#*#*+::.",
    "   ..       @-+::::::::::==@%%%#*+**++*-====**%%*+*++:",
    "   .       +#%%*#+:-:::::::+::::::%****+++==@**+=***-%@%-",
    "           @#*****+-::+*#%%+==+*#*#***+:++*##++++*=::=*=%%*+=.",
    "          -@=#****##=:****#***********=:*%-*::::=::::#==+=-%@*",
    "          +%*++=**#**##***%*********#*:-*--=:::==-:::***+%**@=",
    "          @:#***:+#*******#********%=-***:-=::###*#::::-:+*#@",
    "         :%##***##**#****%*******#=--#**=:+::*#****::::::::#=",
    "         %+**#*****#-:-%*%***********--:::*:=#****#+:::::::@",
    "        .@#:-##****+:=::#*#**********+:::=:=#*%%%%*=::#*::+*",
    "        *@=-::##*#=-+*::+-%****##*****+::#::--+*=:::::+#*+@+",
    "        .#@===::::::=:::-:+#**@::#****+::=::::+%%::::::#*#@:",
    "          .*#*#==:::::::%##=*-:::=****=:=-:::#**#-:-+###*%@",
    "           ..:-#@=*:::::#*=::::::#**+-::=-:::************@=",
    "                .#%=+-:+:::::::::*#*=:::=:::::#***++****#@",
    "                  :=#*=*+=--::...:=----=*=+-:*#**#++*++#%*",
    "                     .#@@@@@@@@@@%#****+@@%+-%#****+**%-@-",
    "                                           -+@#+*%*+==:+%.       ..",
    "                                             @:.::*@@%=%#",
    "                                             #.....:=*%*.",
    "                           .                .%:.:*%%=:.",
    "           .               .                :%*%#-",
    "                                            .--",
]

NEARBY_ART[("mon", 0)] = _ART_ZOMBIE
NEARBY_ART[("mon", 1)] = _ART_SLIME
NEARBY_ART[("mon", 2)] = _ART_HUNTER
NEARBY_ART[("item", 0)] = _ART_POTION
NEARBY_ART[("item", 1)] = _ART_BLINK
NEARBY_ART[("item", 2)] = _ART_MAP

FocusEntity = tuple[str, int, int, int, int]  # kind, etype, x, y, extra (hp or -1)

# 右側面板寬度：對齊各角色 ASCII art 最大寬度（約 70 字元）
SIDE_PANEL_WIDTH = 70
SIDE_GAP = 3
MAP_VISIBLE_WIDTH = 42

_ANSI_RE = re.compile(r"\033\[[0-9;]*m")


def visible_len(text: str) -> int:
    return len(_ANSI_RE.sub("", text))


def pad_visible(text: str, width: int) -> str:
    pad = width - visible_len(text)
    if pad <= 0:
        return text
    return text + " " * pad


def in_nearby_grid(mx: int, my: int, px: int, py: int) -> bool:
    """玩家為中心的 3×3 九宮格（不含玩家自身格）。"""
    if mx == px and my == py:
        return False
    return abs(mx - px) <= 1 and abs(my - py) <= 1


def _crop_line_center(line: str, width: int) -> str:
    if len(line) <= width:
        return line.center(width)
    start = (len(line) - width) // 2
    return line[start : start + width]


def _trim_art_margin(lines: list[str]) -> list[str]:
    """去掉各列共同左側空白，讓圖案盡量撐滿面板。"""
    nonempty = [ln for ln in lines if ln.strip()]
    if not nonempty:
        return lines
    min_lead = min(len(ln) - len(ln.lstrip()) for ln in nonempty)
    if min_lead <= 0:
        return lines
    return [ln[min_lead:] if ln.strip() else ln for ln in lines]


def _fit_art_to_panel(
    raw: list[str],
    width: int,
    height: int,
    *,
    frame: int = 0,
) -> list[str]:
    """垂直／水平置中裁切，frame 可微調垂直偏移做簡易動畫。"""
    lines = _trim_art_margin([ln.rstrip() for ln in raw if ln.strip()])
    if not lines:
        return [" " * width] * height
    vshift = frame % 2
    if len(lines) > height:
        start = (len(lines) - height) // 2 + vshift
        if start + height > len(lines):
            start = len(lines) - height
        if start < 0:
            start = 0
        lines = lines[start : start + height]
    else:
        pad_top = (height - len(lines)) // 2
        lines = [""] * pad_top + lines
        while len(lines) < height:
            lines.append("")
    return [_crop_line_center(ln, width) for ln in lines[:height]]


def _visible_at(visibility: list[int] | None, w: int, x: int, y: int) -> bool:
    if visibility is None:
        return True
    return visibility[y * w + x] == VIS_VISIBLE


def _nearest_in_grid(
    candidates: list[FocusEntity],
    px: int,
    py: int,
) -> FocusEntity | None:
    best: FocusEntity | None = None
    best_dist = 10**9
    for ent in candidates:
        dist = abs(ent[2] - px) + abs(ent[3] - py)
        if dist < best_dist:
            best_dist = dist
            best = ent
    return best


def pick_focus_entity(
    monsters: list[tuple[int, int, int, int]],
    items: list[tuple[int, int, int]],
    px: int,
    py: int,
    *,
    w: int,
    visibility: list[int] | None,
    recent_mon: tuple[int, int, int, int] | None,
    recent_item: tuple[int, int, int] | None = None,
) -> FocusEntity | None:
    """選定右側預覽：九宮格內的怪物或道具（怪物優先於道具）。"""
    nearby_mons: list[FocusEntity] = []
    for mx, my, mhp, mtype in monsters:
        if not in_nearby_grid(mx, my, px, py):
            continue
        if not _visible_at(visibility, w, mx, my):
            continue
        nearby_mons.append(("mon", mtype, mx, my, mhp))

    nearby_items: list[FocusEntity] = []
    for ix, iy, itype in items:
        if not in_nearby_grid(ix, iy, px, py):
            continue
        if not _visible_at(visibility, w, ix, iy):
            continue
        nearby_items.append(("item", itype, ix, iy, -1))

    if not nearby_mons and not nearby_items:
        return None

    if recent_mon is not None:
        for ent in nearby_mons:
            if ent[2] == recent_mon[0] and ent[3] == recent_mon[1] and ent[1] == recent_mon[3]:
                return ent

    if nearby_mons:
        return _nearest_in_grid(nearby_mons, px, py)

    if recent_item is not None:
        for ent in nearby_items:
            if ent[2] == recent_item[0] and ent[3] == recent_item[1] and ent[1] == recent_item[2]:
                return ent

    return _nearest_in_grid(nearby_items, px, py)


def pick_focus_monster(
    monsters: list[tuple[int, int, int, int]],
    px: int,
    py: int,
    *,
    w: int,
    visibility: list[int] | None,
    recent: tuple[int, int, int, int] | None,
) -> tuple[int, int, int, int] | None:
    """相容舊呼叫：僅回傳九宮格內怪物。"""
    ent = pick_focus_entity(
        monsters, [], px, py, w=w, visibility=visibility, recent_mon=recent,
    )
    if ent is None or ent[0] != "mon":
        return None
    return (ent[2], ent[3], ent[4], ent[1])


def _entity_style(kind: str, etype: int, *, color: bool) -> str:
    if not color:
        return ""
    if kind == "mon":
        return MON_COLORS.get(etype, TermStyle.MONSTER)
    return ITEM_COLORS.get(etype, "")


def _entity_art(kind: str, etype: int) -> list[str]:
    return NEARBY_ART.get((kind, etype), NEARBY_ART.get(("mon", 0), []))


def format_entity_panel_lines(
    focus: FocusEntity | None,
    *,
    color: bool,
    frame: int,
    px: int,
    py: int,
    panel_height: int = 22,
) -> list[str]:
    """產生右側 ASCII 區塊（九宮格內依角色類型顯示對應圖）。"""
    R = TermStyle.RESET
    w = SIDE_PANEL_WIDTH
    blank = " " * w
    # 標題 + 圖 + 底部資訊，盡量把高度留給 ASCII art
    art_h = max(panel_height - 2, 1)

    if focus is None:
        empty_title = "── 附近無對象 ──"
        empty_hint = "  走近敵人或道具…  "
        if color:
            filler = [
                pad_visible(f"{TermStyle.DIM}{empty_title}{R}", w),
                blank,
                pad_visible(f"{TermStyle.DIM}{empty_hint}{R}", w),
            ]
        else:
            filler = [empty_title, blank, empty_hint]
        while len(filler) < panel_height:
            filler.append(blank)
        return filler[:panel_height]

    kind, etype, ex, ey, extra = focus
    if kind == "mon":
        name = MON_NAMES.get(etype, "怪物")
        sym = MON_SYMS.get(etype, "?")
    else:
        name = ITEM_NAMES.get(etype, "道具")
        sym = ITEM_SYMS.get(etype, "?")

    mc = _entity_style(kind, etype, color=color)
    art_rows = _fit_art_to_panel(_entity_art(kind, etype), w, art_h, frame=frame)
    dist = abs(ex - px) + abs(ey - py)

    if color:
        title = pad_visible(f"{TermStyle.HUD}┌ {sym} {name} ┐{R}", w)
    else:
        title = pad_visible(f"[ {sym} {name} ]", w)

    if kind == "mon":
        if color:
            footer = pad_visible(f"{mc}HP {extra}{R}  {TermStyle.DIM}距 {dist}{R}", w)
        else:
            footer = pad_visible(f"HP {extra}  距 {dist}", w)
    else:
        desc = ITEM_DESC.get(etype, "")
        if color:
            footer = pad_visible(f"{mc}{desc}{R}  {TermStyle.DIM}距 {dist}{R}", w)
        else:
            footer = pad_visible(f"{desc}  距 {dist}", w)

    lines = [title]
    for row in art_rows:
        if color:
            lines.append(pad_visible(f"{mc}{row}{R}", w))
        else:
            lines.append(pad_visible(row, w))
    lines.append(footer)
    while len(lines) < panel_height:
        lines.append(blank)
    return lines[:panel_height]


def format_monster_panel_lines(
    focus: tuple[int, int, int, int] | None,
    *,
    color: bool,
    frame: int,
    px: int,
    py: int,
    panel_height: int = 22,
) -> list[str]:
    """相容舊呼叫：僅怪物預覽。"""
    ent: FocusEntity | None = None
    if focus is not None:
        ent = ("mon", focus[3], focus[0], focus[1], focus[2])
    return format_entity_panel_lines(
        ent, color=color, frame=frame, px=px, py=py, panel_height=panel_height,
    )


def combine_map_and_panel(
    map_lines: list[str],
    panel_lines: list[str],
    *,
    map_visible_width: int = MAP_VISIBLE_WIDTH,
    gap: int = SIDE_GAP,
) -> list[str]:
    """將地圖列與右側敵人面板合併為同一列輸出。"""
    spacer = " " * gap
    out: list[str] = []
    for y, left in enumerate(map_lines):
        right = panel_lines[y] if y < len(panel_lines) else " " * SIDE_PANEL_WIDTH
        out.append(pad_visible(left, map_visible_width) + spacer + right)
    return out


def format_map_lines(
    buf: list[int],
    w: int,
    h: int,
    px: int,
    py: int,
    *,
    color: bool,
    monsters: list[tuple[int, int, int, int]] | None = None,
    items: list[tuple[int, int, int]] | None = None,
    visibility: list[int] | None = None,
) -> list[str]:
    mon_set: dict[tuple[int, int], int] = {}
    if monsters:
        for mx, my, _mhp, mtype in monsters:
            mon_set[(mx, my)] = mtype
    item_set: dict[tuple[int, int], int] = {}
    if items:
        for ix, iy, itype in items:
            item_set[(ix, iy)] = itype

    lines: list[str] = []
    R = TermStyle.RESET
    for y in range(h):
        parts: list[str] = []
        for x in range(w):
            i = y * w + x
            vis = visibility[i] if visibility else VIS_VISIBLE
            is_p = x == px and y == py
            t = buf[i]

            if vis == VIS_UNSEEN:
                if color:
                    parts.append(f"{TermStyle.FOG_UNSEEN} {R}")
                else:
                    parts.append(" ")
                continue

            if vis == VIS_EXPLORED:
                if color:
                    if t == 0:
                        parts.append(f"{TermStyle.FOG_EXPLORED}#{R}")
                    elif t == 1:
                        parts.append(f"{TermStyle.FOG_EXPLORED}·{R}")
                    elif t == 2:
                        parts.append(f"{TermStyle.FOG_EXPLORED}>{R}")
                    else:
                        parts.append(f"{TermStyle.FOG_EXPLORED}?{R}")
                else:
                    if t == 0:
                        parts.append("#")
                    elif t == 1:
                        parts.append(".")
                    elif t == 2:
                        parts.append(">")
                    else:
                        parts.append("?")
                continue

            if is_p:
                sym = "@"
                if color:
                    parts.append(f"{TermStyle.PLAYER}{sym}{R}")
                else:
                    parts.append(sym)
                continue
            mt = mon_set.get((x, y))
            if mt is not None:
                sym = MON_SYMS.get(mt, "E")
                if color:
                    mc = MON_COLORS.get(mt, TermStyle.MONSTER)
                    parts.append(f"{mc}{sym}{R}")
                else:
                    parts.append(sym)
                continue
            it_type = item_set.get((x, y))
            if it_type is not None:
                sym = ITEM_SYMS.get(it_type, "?")
                if color:
                    c = ITEM_COLORS.get(it_type, "")
                    parts.append(f"{c}{sym}{R}")
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
