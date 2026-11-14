# Building rurl

`rurl` is a small C utility split into focused source modules under `src/`, with zero dependencies outside of OpenSSL.

## Prerequisites

- **C compiler**: `clang` (the project's standard compiler). GCC is not
  supported as a build target, though the source is plain C11.
- **OpenSSL development headers** (e.g., `libssl-dev` on Debian/Ubuntu, `openssl-dev` on Alpine Linux)
- **POSIX environment** (Linux or macOS)

## Compiling

You can compile `rurl` using the provided `Makefile` or build it directly with `clang`. Clang is the standard compiler for this project; the `Makefile` uses it by default.

### Using Makefile (Recommended)

Run `make` at the root of the repository to compile:

```sh
make
```

To install the built binary globally (defaults to `/usr/local/bin/`):

```sh
sudo make install
```

### Compiling Directly

If you prefer to compile manually:

```sh
clang -Wall -Wextra -Os -Isrc -o rurl \
    src/main.c src/util.c src/url.c src/net.c src/http.c src/response.c \
    -lssl -lcrypto
```

## Troubleshooting Build Failures

If compilation fails with an error like `openssl/ssl.h: No such file or directory`, ensure you have installed the OpenSSL development package:

- **Debian / Ubuntu**: `sudo apt install clang libssl-dev`
- **Alpine Linux**: `apk add clang openssl-dev`
- **macOS**: `brew install openssl` (you may need to append brew's OpenSSL path to `CFLAGS` and `LDFLAGS` if it is not linked automatically)
