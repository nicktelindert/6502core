CC = gcc
CFLAGS = -O3 -g -Wall -Wextra -std=c11
LDFLAGS = -lSDL2

SRCS = main.c cpu6507.c addressing.c bus.c tia.c riot.c cartridge.c audio.c
TARGET = 6502core

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)