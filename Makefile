CC = gcc
CFLAGS = -O0 -g -Wall -Wextra -std=c11
CFLAGS_release = -O2 -Wall -Wextra -std=c11

LDFLAGS = -lSDL2

CORE_SRCS = cpu6502.c addressing.c system2600.c tia.c riot.c cartridge.c audio.c
CORE_HEADERS = cpu6502.h addressing.h system2600.h tia.h riot.h cartridge.h detectchip.h audio.h video.h
APP_SRCS = main.c rom_browser.c system5200.c antic5200.c gtia5200.c pokey5200.c $(CORE_SRCS)
CHIPS5200 = system5200.c antic5200.c gtia5200.c pokey5200.c
HEADERS5200 = system5200.h antic5200.h gtia5200.h pokey5200.h
TEST_BIN = tests/cpu_test
CART_TEST_BIN = tests/cartridge_test
TIA_TEST_BIN = tests/tia_test
VIDEO2600_TEST_BIN = tests/video2600_test
TIMING2600_TEST_BIN = tests/timing2600_test
RIOT_TEST_BIN = tests/riot_test
RELOAD_TEST_BIN = tests/rom_reload_test
CPU_BUS_TEST_BIN = tests/cpu_bus_test
CPU_FUNCTIONAL_TEST_BIN = tests/cpu_functional_test
SYSTEM_TEST_BIN = tests/system2600_test
SYSTEM5200_TEST_BIN = tests/system5200_test
ANTIC5200_TEST_BIN = tests/antic5200_test
VIDEO5200_TEST_BIN = tests/video5200_test
POKEY5200_TEST_BIN = tests/pokey5200_test
GTIA5200_TEST_BIN = tests/gtia5200_test
BROWSER_TEST_BIN = tests/rom_browser_test

.PHONY: all test test-functional clean

all: 6502core

6502core_rel: $(APP_SRCS) $(CORE_HEADERS) rom_browser.h $(HEADERS5200)
	$(CC) $(CFLAGS_release) -o $@ $(APP_SRCS) $(LDFLAGS)
6502core: $(APP_SRCS) $(CORE_HEADERS) rom_browser.h $(HEADERS5200)
	$(CC) $(CFLAGS) -o $@ $(APP_SRCS) $(LDFLAGS)

$(TEST_BIN): tests/cpu.c $(CORE_SRCS) $(CORE_HEADERS)
	$(CC) $(CFLAGS) -o $@ tests/cpu.c $(CORE_SRCS) $(LDFLAGS)

$(CART_TEST_BIN): tests/cartridge.c $(CORE_SRCS) $(CORE_HEADERS)
	$(CC) $(CFLAGS) -o $@ tests/cartridge.c $(CORE_SRCS) $(LDFLAGS)

$(TIA_TEST_BIN): tests/tia.c $(CORE_SRCS) $(CORE_HEADERS)
	$(CC) $(CFLAGS) -o $@ tests/tia.c $(CORE_SRCS) $(LDFLAGS)

$(VIDEO2600_TEST_BIN): tests/video2600.c tests/demo2600_pattern.h $(CORE_SRCS) $(CORE_HEADERS)
	$(CC) $(CFLAGS) -o $@ tests/video2600.c $(CORE_SRCS) $(LDFLAGS)

$(TIMING2600_TEST_BIN): tests/timing2600.c $(CORE_SRCS) $(CORE_HEADERS)
	$(CC) $(CFLAGS) -o $@ tests/timing2600.c $(CORE_SRCS) $(LDFLAGS)

$(RIOT_TEST_BIN): tests/riot.c $(CORE_SRCS) $(CORE_HEADERS)
	$(CC) $(CFLAGS) -o $@ tests/riot.c $(CORE_SRCS) $(LDFLAGS)

$(RELOAD_TEST_BIN): tests/rom_reload.c main.c rom_browser.c rom_browser.h $(CHIPS5200) $(HEADERS5200) $(CORE_SRCS) $(CORE_HEADERS)
	$(CC) $(CFLAGS) -o $@ tests/rom_reload.c rom_browser.c $(CHIPS5200) $(CORE_SRCS) $(LDFLAGS)

$(BROWSER_TEST_BIN): tests/rom_browser.c rom_browser.c rom_browser.h
	$(CC) $(CFLAGS) -o $@ tests/rom_browser.c rom_browser.c

$(CPU_BUS_TEST_BIN): tests/cpu_bus.c cpu6502.c addressing.c cpu6502.h addressing.h
	$(CC) $(CFLAGS) -o $@ tests/cpu_bus.c cpu6502.c addressing.c

$(CPU_FUNCTIONAL_TEST_BIN): tests/cpu_functional.c cpu6502.c addressing.c cpu6502.h addressing.h
	$(CC) $(CFLAGS) -o $@ tests/cpu_functional.c cpu6502.c addressing.c

$(SYSTEM_TEST_BIN): tests/system2600.c $(CORE_SRCS) $(CORE_HEADERS)
	$(CC) $(CFLAGS) -o $@ tests/system2600.c $(CORE_SRCS) $(LDFLAGS)

$(SYSTEM5200_TEST_BIN): tests/system5200.c $(CHIPS5200) $(HEADERS5200) cpu6502.c addressing.c cpu6502.h addressing.h
	$(CC) $(CFLAGS) -o $@ tests/system5200.c $(CHIPS5200) cpu6502.c addressing.c

$(ANTIC5200_TEST_BIN): tests/antic5200.c $(CHIPS5200) $(HEADERS5200) cpu6502.c addressing.c
	$(CC) $(CFLAGS) -o $@ tests/antic5200.c $(CHIPS5200) cpu6502.c addressing.c

$(VIDEO5200_TEST_BIN): tests/video5200.c $(CHIPS5200) $(HEADERS5200) cpu6502.c addressing.c
	$(CC) $(CFLAGS) -o $@ tests/video5200.c $(CHIPS5200) cpu6502.c addressing.c

$(POKEY5200_TEST_BIN): tests/pokey5200.c $(CHIPS5200) $(HEADERS5200) cpu6502.c addressing.c
	$(CC) $(CFLAGS) -o $@ tests/pokey5200.c $(CHIPS5200) cpu6502.c addressing.c

$(GTIA5200_TEST_BIN): tests/gtia5200.c $(CHIPS5200) $(HEADERS5200) cpu6502.c addressing.c
	$(CC) $(CFLAGS) -o $@ tests/gtia5200.c $(CHIPS5200) cpu6502.c addressing.c

5200-demo: tests/demo5200.c $(CHIPS5200) $(HEADERS5200) cpu6502.c addressing.c
	$(CC) $(CFLAGS_release) -o $@ tests/demo5200.c $(CHIPS5200) cpu6502.c addressing.c $(LDFLAGS)

2600-demo: tests/demo2600.c tests/demo2600_pattern.h $(CORE_SRCS) $(CORE_HEADERS)
	$(CC) $(CFLAGS_release) -o $@ tests/demo2600.c $(CORE_SRCS) $(LDFLAGS)

test-functional: $(CPU_FUNCTIONAL_TEST_BIN)
	./$(CPU_FUNCTIONAL_TEST_BIN) "$(FUNCTIONAL_ROM)"

test: $(TEST_BIN) $(CART_TEST_BIN) $(TIA_TEST_BIN) $(VIDEO2600_TEST_BIN) $(TIMING2600_TEST_BIN) $(RIOT_TEST_BIN) $(RELOAD_TEST_BIN) $(CPU_BUS_TEST_BIN) $(SYSTEM_TEST_BIN) $(SYSTEM5200_TEST_BIN) $(ANTIC5200_TEST_BIN) $(VIDEO5200_TEST_BIN) $(POKEY5200_TEST_BIN) $(GTIA5200_TEST_BIN) $(BROWSER_TEST_BIN)
	./$(TEST_BIN)
	./$(CART_TEST_BIN)
	./$(TIA_TEST_BIN)
	./$(VIDEO2600_TEST_BIN)
	./$(TIMING2600_TEST_BIN)
	./$(RIOT_TEST_BIN)
	./$(RELOAD_TEST_BIN)
	./$(CPU_BUS_TEST_BIN)
	./$(SYSTEM_TEST_BIN)
	./$(SYSTEM5200_TEST_BIN)
	./$(ANTIC5200_TEST_BIN)
	./$(VIDEO5200_TEST_BIN)
	./$(POKEY5200_TEST_BIN)
	./$(GTIA5200_TEST_BIN)
	./$(BROWSER_TEST_BIN)

clean:
	rm -f 6502core 2600-demo 5200-demo $(TEST_BIN) $(CART_TEST_BIN) $(TIA_TEST_BIN) $(VIDEO2600_TEST_BIN) $(TIMING2600_TEST_BIN) $(RIOT_TEST_BIN) $(RELOAD_TEST_BIN) $(CPU_BUS_TEST_BIN) $(CPU_FUNCTIONAL_TEST_BIN) $(SYSTEM_TEST_BIN) $(SYSTEM5200_TEST_BIN) $(ANTIC5200_TEST_BIN) $(VIDEO5200_TEST_BIN) $(POKEY5200_TEST_BIN) $(GTIA5200_TEST_BIN) $(BROWSER_TEST_BIN)
