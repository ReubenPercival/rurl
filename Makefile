# rurl - Minimal raw URL fetcher
# Suckless-style build: minimal, fast, small binary

CC       := clang
CFLAGS   := -Wall -Wextra -Os -pedantic
CPPFLAGS := -Isrc
LDFLAGS  := -s

TARGET   := rurl
SRC      := src/main.c src/util.c src/url.c src/net.c src/http.c src/response.c
OBJ      := $(SRC:.c=.o)
DEPS     := src/rurl.h src/http_internal.h

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) -lssl -lcrypto $(LDFLAGS)

src/%.o: src/%.c $(DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/
