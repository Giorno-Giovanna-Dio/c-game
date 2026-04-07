# Ubuntu 24.04 / Linux：產生 libroguecore.so 與 rogue_cli
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -O2 -fPIC -Iinclude -Isrc/c
LDFLAGS_SO := -shared
SRCDIR := src/c
OBJ := $(SRCDIR)/rng.o $(SRCDIR)/dungeon.o $(SRCDIR)/game.o
LIB := libroguecore.so
CLI := rogue_cli

.PHONY: all clean test

all: $(LIB) $(CLI)

$(SRCDIR)/rng.o: $(SRCDIR)/rng.c $(SRCDIR)/rng.h
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/rng.c

$(SRCDIR)/dungeon.o: $(SRCDIR)/dungeon.c $(SRCDIR)/dungeon.h $(SRCDIR)/rng.h
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/dungeon.c

$(SRCDIR)/game.o: $(SRCDIR)/game.c include/roguecore.h $(SRCDIR)/dungeon.h $(SRCDIR)/rng.h
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/game.c

$(LIB): $(OBJ)
	$(CC) $(LDFLAGS_SO) -o $@ $(OBJ)

$(SRCDIR)/cli_main.o: $(SRCDIR)/cli_main.c include/roguecore.h
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/cli_main.c

$(CLI): $(SRCDIR)/cli_main.o $(LIB)
	$(CC) -o $@ $(SRCDIR)/cli_main.o -L. -lroguecore -Wl,-rpath,'$$ORIGIN'

clean:
	rm -f $(OBJ) $(SRCDIR)/cli_main.o $(LIB) $(CLI)

test: all
	./tests/run_smoke.sh
