# Building rurl

rurl is a single‑file C program. It depends on OpenSSL for HTTPS support.

## Prerequisites
- gcc/clang
- OpenSSL development headers (`libssl-dev` on Debian/Ubuntu)
- POSIX environment (Linux/macOS)

## Compile
```sh
# Simple compilation
gcc -Wall -Wextra -O2 -o rurl rurl.c -lssl -lcrypto
```

If you prefer a Makefile:
```makefile
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lssl -lcrypto

all: rurl

rurl: rurl.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f rurl
```

## Verify
```sh
./rurl https://example.com
```
