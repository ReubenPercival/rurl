# rurl - Minimal raw URL fetcher
# Suckless-style build: minimal, fast, small binary

CC      ?= gcc
CFLAGS  := -Wall -Wextra -Os -pedantic
LDFLAGS := -s

TARGET  := rurl
SRC     := rurl.c

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< -lcurl $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/