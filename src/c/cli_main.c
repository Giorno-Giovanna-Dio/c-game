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
static const char *ANSI_HUD = "\033[36m";
static const char *ANSI_DIM = "\033[2m";

static void usage(const char *argv0) {
    fprintf(stderr, "用法: %s [--seed N] [--line-mode] [--no-color]\n", argv0);
    fprintf(stderr, "  預設：互動 TTY 單鍵（免 Enter）、ANSI 色彩；管道或非 TTY 自動 --line-mode\n");
    fprintf(stderr, "  鍵: w/a/s/d 或方向鍵, ? 說明, q 離開\n");
}

static void print_help(void) {
    puts("【怎麼玩】");
    puts("  @ = 玩家  # = 牆  . = 地板  > = 樓梯（走到這格就贏）");
    puts("  互動模式：直接按鍵不必 Enter；--line-mode 則每行一個字母再 Enter");
    puts("目標：把 @ 移到地圖上的 > 。");
    puts("");
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

static void print_map(const uint8_t *tiles, int w, int h, int px, int py) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (x == px && y == py) {
                if (g_use_color) {
                    fputs(ANSI_PLAYER, stdout);
                    putchar('@');
                    fputs(ANSI_RESET, stdout);
                } else {
                    putchar('@');
                }
                continue;
            }
            uint8_t t = tiles[y * w + x];
            if (t == RC_TILE_WALL) {
                if (g_use_color) {
                    fputs(ANSI_WALL, stdout);
                    putchar('#');
                    fputs(ANSI_RESET, stdout);
                } else {
                    putchar('#');
                }
            } else if (t == RC_TILE_FLOOR) {
                if (g_use_color) {
                    fputs(ANSI_FLOOR, stdout);
                    putchar('.');
                    fputs(ANSI_RESET, stdout);
                } else {
                    putchar('.');
                }
            } else if (t == RC_TILE_STAIRS) {
                if (g_use_color) {
                    fputs(ANSI_STAIRS, stdout);
                    putchar('>');
                    fputs(ANSI_RESET, stdout);
                } else {
                    putchar('>');
                }
            } else {
                putchar('?');
            }
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
        if (g_use_color) {
            printf("%srogue_cli%s  C 核心  seed=%u\n\n", ANSI_TITLE, ANSI_RESET, (unsigned)seed);
        } else {
            printf("rogue_cli  C 核心  seed=%u\n\n", (unsigned)seed);
        }

        if (rc_game_tiles(g, tiles, bufn) < 0) {
            fprintf(stderr, "rc_game_tiles 失敗\n");
            break;
        }
        int px, py;
        rc_game_player(g, &px, &py);
        print_map(tiles, w, h, px, py);

        if (g_use_color) {
            printf("\n%s# 牆  . 地板  > 樓梯  @ 你%s\n", ANSI_HUD, ANSI_RESET);
            printf("%s%s%s\n", ANSI_DIM, status, ANSI_RESET);
        } else {
            puts("\n# 牆  . 地板  > 樓梯  @ 你");
            puts(status);
        }

        if (rc_game_done(g)) {
            if (g_use_color) {
                printf("\n%s你抵達樓梯，勝利！%s\n", ANSI_TITLE, ANSI_RESET);
            } else {
                puts("\n你抵達樓梯，勝利！");
            }
            break;
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
        if (mv == 1) {
            status = "撞牆";
        } else if (mv < 0) {
            status = "無效移動";
        } else {
            status = "WASD / 方向鍵 移動，? 說明，q 離開";
        }
    }

#ifdef RC_TTY_UI
    tty_restore();
#endif
    free(tiles);
    rc_game_destroy(g);
    return 0;
}
