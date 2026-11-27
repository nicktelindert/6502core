CC = gcc
CFLAGS = -O0 -g -Wall -Wextra -std=c11
LDFLAGS = -lSDL2

CORE_SRCS = cpu6507.c addressing.c bus.c tia.c riot.c cartridge.c audio.c
APP_SRCS = main.c $(CORE_SRCS)
TEST_BIN = tests/cpu_test

.PHONY: all test clean

all: 6502core

6502core: $(APP_SRCS)
	$(CC) $(CFLAGS) -o $@ $(APP_SRCS) $(LDFLAGS)

$(TEST_BIN): tests/cpu.c $(CORE_SRCS)
	$(CC) $(CFLAGS) -o $@ tests/cpu.c $(CORE_SRCS) $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f 6502core $(TEST_BIN)
