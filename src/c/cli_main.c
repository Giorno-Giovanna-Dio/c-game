#include "roguecore.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#define RC_TTY_UI 1
#endif

static int g_line_mode = 0;
static int g_use_color = 1;
#ifdef RC_TTY_UI
static struct termios g_saved_termios;
static int g_tty_saved = 0;
#endif

static const char *ANSI_RESET = "\033[0m";
static const char *ANSI_TITLE = "\033[1;97m";
static const char *ANSI_WALL = "\033[38;5;237m";
static const char *ANSI_FLOOR = "\033[38;5;245m";
static const char *ANSI_PLAYER = "\033[1;93m";
static const char *ANSI_STAIRS = "\033[1;92m";
static const char *ANSI_MONSTER = "\033[1;91m";
static const char *ANSI_HUD = "\033[36m";
static const char *ANSI_DIM = "\033[2m";
static const char *ANSI_HP_GOOD = "\033[1;92m";
static const char *ANSI_HP_MED = "\033[1;33m";
static const char *ANSI_HP_LOW = "\033[1;91m";
static const char *ANSI_WARN = "\033[1;91m";
static const char *ANSI_FOG_EXPLORED = "\033[38;5;239m";
static const char *ANSI_FOG_UNSEEN = "\033[38;5;233m";
static const char *ANSI_ITEM_POTION = "\033[1;95m";
static const char *ANSI_ITEM_BLINK = "\033[1;96m";
static const char *ANSI_ITEM_MAP = "\033[1;33m";

static int g_mon_xs[RC_MAX_MONSTERS], g_mon_ys[RC_MAX_MONSTERS];
static int g_mon_count = 0;
static int g_itm_xs[RC_MAX_ITEMS], g_itm_ys[RC_MAX_ITEMS], g_itm_types[RC_MAX_ITEMS];
static int g_itm_count = 0;
static uint8_t g_vis[RC_GAME_WIDTH * RC_GAME_HEIGHT];

static void usage(const char *argv0) {
    fprintf(stderr, "用法: %s [--seed N] [--line-mode] [--no-color]\n", argv0);
    fprintf(stderr, "  預設：互動 TTY 單鍵（免 Enter）、ANSI 色彩；管道或非 TTY 自動 --line-mode\n");
    fprintf(stderr, "  鍵: w/a/s/d 或方向鍵, ? 說明, q 離開\n");
}

static void print_help(void) {
    puts("【怎麼玩】");
    puts("  @ = 玩家  E = 怪物（走進牠就攻擊）");
    puts("  ! = 治療藥水（走上去拾取，回復 HP）");
    puts("  ~ = 閃現卷軸（走上去拾取，隨機瞬移）");
    puts("  % = 地圖卷軸（走上去拾取，揭示全層地形）");
    puts("  # = 牆  . = 地板  > = 樓梯");
    puts("  互動模式：直接按鍵不必 Enter；--line-mode 則每行一個字母再 Enter");
    puts("目標：避開或擊殺怪物，在步數耗盡前走到樓梯。善用道具生存！");
    puts("");
}

static int is_monster_at(int x, int y) {
    for (int i = 0; i < g_mon_count; i++) {
        if (g_mon_xs[i] == x && g_mon_ys[i] == y) return 1;
    }
    return 0;
}

static void refresh_monsters(void *g) {
    int buf[RC_MAX_MONSTERS * 3];
    g_mon_count = rc_game_monsters(g, buf, RC_MAX_MONSTERS * 3);
    for (int i = 0; i < g_mon_count; i++) {
        g_mon_xs[i] = buf[i * 3];
        g_mon_ys[i] = buf[i * 3 + 1];
    }
}

static int is_item_at(int x, int y, int *out_type) {
    for (int i = 0; i < g_itm_count; i++) {
        if (g_itm_xs[i] == x && g_itm_ys[i] == y) {
            if (out_type) *out_type = g_itm_types[i];
            return 1;
        }
    }
    return 0;
}

static void refresh_items(void *g) {
    int buf[RC_MAX_ITEMS * 3];
    g_itm_count = rc_game_items(g, buf, RC_MAX_ITEMS * 3);
    for (int i = 0; i < g_itm_count; i++) {
        g_itm_xs[i] = buf[i * 3];
        g_itm_ys[i] = buf[i * 3 + 1];
        g_itm_types[i] = buf[i * 3 + 2];
    }
}

static void print_hp_bar(int hp, int max_hp) {
    const char *c = ANSI_HP_GOOD;
    float ratio = (float)hp / (max_hp > 0 ? max_hp : 1);
    if (ratio <= 0.3f) c = ANSI_HP_LOW;
    else if (ratio <= 0.6f) c = ANSI_HP_MED;
    int filled = (int)(20.0f * hp / (max_hp > 0 ? max_hp : 1));
    if (filled < 0) filled = 0;
    if (filled > 20) filled = 20;
    if (g_use_color) fputs(c, stdout);
    printf("HP %d/%d [", hp, max_hp);
    for (int i = 0; i < 20; i++) putchar(i < filled ? '#' : '-');
    putchar(']');
    if (g_use_color) fputs(ANSI_RESET, stdout);
}

#ifdef RC_TTY_UI
static void tty_restore(void) {
    if (g_tty_saved) {
        tcsetattr(STDIN_FILENO, TCSADRAIN, &g_saved_termios);
        g_tty_saved = 0;
    }
    fputs("\033[?25h", stdout);
    fflush(stdout);
}

static int tty_setup_raw(void) {
    if (!isatty(STDIN_FILENO)) {
        return -1;
    }
    if (tcgetattr(STDIN_FILENO, &g_saved_termios) != 0) {
        return -1;
    }
    struct termios raw = g_saved_termios;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSADRAIN, &raw) != 0) {
        return -1;
    }
    g_tty_saved = 1;
    fputs("\033[?25l", stdout);
    fflush(stdout);
    return 0;
}

static int read_key_raw(void) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1) {
        return EOF;
    }
    if (c == 27) {
        fd_set fds;
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            unsigned char b[2];
            if (read(STDIN_FILENO, b, 2) == 2 && b[0] == '[') {
                switch (b[1]) {
                case 'A':
                    return 'w';
                case 'B':
                    return 's';
                case 'D':
                    return 'a';
                case 'C':
                    return 'd';
                default:
                    break;
                }
            }
        }
        return '?';
    }
    return (int)c;
}
#endif

static void put_tile(char ch, const char *ansi) {
    if (g_use_color) { fputs(ansi, stdout); putchar(ch); fputs(ANSI_RESET, stdout); }
    else putchar(ch);
}

static void print_map(const uint8_t *tiles, int w, int h, int px, int py) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            uint8_t v = g_vis[i];
            uint8_t t = tiles[i];
            if (v == RC_VIS_UNSEEN) {
                if (g_use_color) { fputs(ANSI_FOG_UNSEEN, stdout); putchar(' '); fputs(ANSI_RESET, stdout); }
                else putchar(' ');
                continue;
            }
            if (v == RC_VIS_EXPLORED) {
                const char *c = g_use_color ? ANSI_FOG_EXPLORED : "";
                if (g_use_color) fputs(c, stdout);
                if (t == RC_TILE_WALL) putchar('#');
                else if (t == RC_TILE_FLOOR) putchar('.');
                else if (t == RC_TILE_STAIRS) putchar('>');
                else putchar('?');
                if (g_use_color) fputs(ANSI_RESET, stdout);
                continue;
            }
            if (x == px && y == py) { put_tile('@', ANSI_PLAYER); continue; }
            if (is_monster_at(x, y)) { put_tile('E', ANSI_MONSTER); continue; }
            {
                int itype;
                if (is_item_at(x, y, &itype)) {
                    char sym = '?';
                    const char *ic = ANSI_FLOOR;
                    if (itype == RC_ITEM_POTION) { sym = '!'; ic = ANSI_ITEM_POTION; }
                    else if (itype == RC_ITEM_BLINK) { sym = '~'; ic = ANSI_ITEM_BLINK; }
                    else if (itype == RC_ITEM_MAP) { sym = '%'; ic = ANSI_ITEM_MAP; }
                    put_tile(sym, ic);
                    continue;
                }
            }
            if (t == RC_TILE_WALL) put_tile('#', ANSI_WALL);
            else if (t == RC_TILE_FLOOR) put_tile('.', ANSI_FLOOR);
            else if (t == RC_TILE_STAIRS) put_tile('>', ANSI_STAIRS);
            else putchar('?');
        }
        putchar('\n');
    }
}

static void clear_screen(void) { fputs("\033[2J\033[H", stdout); }

int main(int argc, char **argv) {
    uint32_t seed = 1u;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--line-mode") == 0) {
            g_line_mode = 1;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            g_use_color = 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

#ifdef RC_TTY_UI
    if (!g_line_mode && !isatty(STDIN_FILENO)) {
        g_line_mode = 1;
    }
    if (g_use_color && !isatty(STDOUT_FILENO)) {
        g_use_color = 0;
    }
#endif

    void *g = rc_game_create(seed);
    if (!g) {
        fprintf(stderr, "rc_game_create 失敗\n");
        return 1;
    }

    int w = rc_game_width(g);
    int h = rc_game_height(g);
    size_t bufn = (size_t)w * (size_t)h;
    uint8_t *tiles = (uint8_t *)malloc(bufn);
    if (!tiles) {
        rc_game_destroy(g);
        return 1;
    }

#ifdef RC_TTY_UI
    int tty_ok = (!g_line_mode && tty_setup_raw() == 0);
    if (!g_line_mode && !tty_ok) {
        g_line_mode = 1;
    }
    atexit(tty_restore);
#endif

    const char *status = "WASD / 方向鍵 移動，? 說明，q 離開";

    for (;;) {
        if (!g_line_mode) {
            clear_screen();
        }
        int fl = rc_game_floor(g);
        int steps = rc_game_steps_left(g);
        int steps_max = rc_game_steps_max(g);
        if (g_use_color) {
            printf("%srogue_cli%s  B%dF  ", ANSI_TITLE, ANSI_RESET, fl);
        } else {
            printf("rogue_cli  B%dF  ", fl);
        }
        print_hp_bar(rc_game_player_hp(g), rc_game_player_max_hp(g));
        int step_warn = (steps <= steps_max / 4);
        if (g_use_color) {
            printf("  %s步數 %d/%d%s\n\n", step_warn ? ANSI_WARN : ANSI_DIM, steps, steps_max, ANSI_RESET);
        } else {
            printf("  步數 %d/%d\n\n", steps, steps_max);
        }

        refresh_monsters(g);
        refresh_items(g);
        rc_game_visibility(g, g_vis, sizeof g_vis);
        if (rc_game_tiles(g, tiles, bufn) < 0) {
            fprintf(stderr, "rc_game_tiles 失敗\n");
            break;
        }
        int px, py;
        rc_game_player(g, &px, &py);
        print_map(tiles, w, h, px, py);

        int alive = rc_game_monster_count(g);
        if (g_use_color) {
            printf("\n%s# 牆  . 地板  > 樓梯  @ 你  E 怪物(%d)  ! 藥水  ~ 閃現  %% 地圖(%d)%s\n", ANSI_HUD, alive, g_itm_count, ANSI_RESET);
            printf("%s%s%s\n", ANSI_DIM, status, ANSI_RESET);
        } else {
            printf("\n# 牆  . 地板  > 樓梯  @ 你  E 怪物(%d)  ! 藥水  ~ 閃現  %% 地圖(%d)\n", alive, g_itm_count);
            puts(status);
        }

        if (rc_game_dead(g)) {
            if (g_use_color) {
                printf("\n%s你被怪物擊殺，遊戲結束！%s\n", ANSI_WARN, ANSI_RESET);
            } else {
                puts("\n你被怪物擊殺，遊戲結束！");
            }
            break;
        }
        if (rc_game_won(g)) {
            if (g_use_color) {
                printf("\n%s你征服了全部地城，勝利！%s\n", ANSI_TITLE, ANSI_RESET);
            } else {
                puts("\n你征服了全部地城，勝利！");
            }
            break;
        }
        {
            int desc = rc_game_descend(g);
            if (desc == 2) {
                const char *dm = rc_game_last_message(g);
                status = (dm && dm[0]) ? dm : "通關！";
                continue;
            } else if (desc == 1) {
                const char *dm = rc_game_last_message(g);
                status = (dm && dm[0]) ? dm : "下一層！";
                continue;
            }
        }

        int ci;
#ifdef RC_TTY_UI
        if (!g_line_mode) {
            fflush(stdout);
            ci = read_key_raw();
            if (ci == EOF) {
                break;
            }
        } else
#endif
        {
            printf("移動 (w/a/s/d，? 說明，q 離開) > ");
            fflush(stdout);
            char line[32];
            if (!fgets(line, sizeof line, stdin)) {
                break;
            }
            ci = (unsigned char)line[0];
        }

        char c = (char)tolower((unsigned char)ci);
        if (c == '?' || c == 'h') {
            if (!g_line_mode) {
                clear_screen();
            }
            print_help();
#ifdef RC_TTY_UI
            if (g_line_mode) {
                fputs("按 Enter 回到遊戲… ", stdout);
                fflush(stdout);
                char buf[8];
                if (fgets(buf, sizeof buf, stdin)) {
                }
            } else {
                puts("按任意鍵回到遊戲…");
                fflush(stdout);
                read_key_raw();
            }
#else
            fputs("按 Enter 回到遊戲… ", stdout);
            fflush(stdout);
            char buf[8];
            if (fgets(buf, sizeof buf, stdin)) {
            }
#endif
            status = "說明已關閉";
            continue;
        }

        int dx = 0, dy = 0;
        if (c == 'w') {
            dy = -1;
        } else if (c == 's') {
            dy = 1;
        } else if (c == 'a') {
            dx = -1;
        } else if (c == 'd') {
            dx = 1;
        } else if (c == 'q') {
            break;
        } else if (c == 3) {
            break;
        } else {
#ifdef RC_TTY_UI
            if (!g_line_mode) {
                status = "請用 WASD、方向鍵、?、q";
            }
#endif
            continue;
        }

        int mv = rc_game_move(g, dx, dy);
        const char *cmsg = rc_game_last_message(g);
        if (mv == 1) {
            status = "撞牆";
        } else if (mv == 2 || mv == 3) {
            status = (cmsg && cmsg[0]) ? cmsg : "戰鬥！";
        } else if (mv < 0) {
            status = "無效移動";
        } else {
            status = (cmsg && cmsg[0]) ? cmsg : "WASD / 方向鍵 移動，? 說明，q 離開";
        }
    }

#ifdef RC_TTY_UI
    tty_restore();
#endif
    free(tiles);
    rc_game_destroy(g);
    return 0;
}
