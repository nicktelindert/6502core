CC = gcc
CFLAGS = -O0 -g -Wall -Wextra -std=c11
CFLAGS_release = -O0 -Wall -Wextra -std=c11

LDFLAGS = -lSDL2

CORE_SRCS = cpu6507.c addressing.c bus.c tia.c riot.c cartridge.c audio.c
APP_SRCS = main.c $(CORE_SRCS)
TEST_BIN = tests/cpu_test
CART_TEST_BIN = tests/cartridge_test
TIA_TEST_BIN = tests/tia_test
RIOT_TEST_BIN = tests/riot_test
RELOAD_TEST_BIN = tests/rom_reload_test

.PHONY: all test clean

all: 6502core

6502core_rel: $(APP_SRCS)
	$(CC) $(CFLAGS_release) -o $@ $(APP_SRCS) $(LDFLAGS)
6502core: $(APP_SRCS)
	$(CC) $(CFLAGS) -o $@ $(APP_SRCS) $(LDFLAGS)

$(TEST_BIN): tests/cpu.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/cpu.c $(CORE_SRCS) $(LDFLAGS)

$(CART_TEST_BIN): tests/cartridge.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/cartridge.c $(CORE_SRCS) $(LDFLAGS)

$(TIA_TEST_BIN): tests/tia.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/tia.c $(CORE_SRCS) $(LDFLAGS)

$(RIOT_TEST_BIN): tests/riot.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/riot.c $(CORE_SRCS) $(LDFLAGS)

$(RELOAD_TEST_BIN): tests/rom_reload.c main.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/rom_reload.c $(CORE_SRCS) $(LDFLAGS)

test: $(TEST_BIN) $(CART_TEST_BIN) $(TIA_TEST_BIN) $(RIOT_TEST_BIN) $(RELOAD_TEST_BIN)
	./$(TEST_BIN)
	./$(CART_TEST_BIN)
	./$(TIA_TEST_BIN)
	./$(RIOT_TEST_BIN)
	./$(RELOAD_TEST_BIN)

clean:
	rm -f 6502core $(TEST_BIN) $(CART_TEST_BIN) $(TIA_TEST_BIN) $(RIOT_TEST_BIN) $(RELOAD_TEST_BIN)
