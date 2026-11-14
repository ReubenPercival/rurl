# Contributing to rurl

Thank you for your interest in contributing! We welcome bug fixes, documentation improvements, and feature contributions.

## Steps to Contribute

1. **Fork** the repository on GitHub.
2. **Create a branch** for your changes:
   ```sh
   git checkout -b feature/my-new-feature
   ```
3. **Make your changes** in the relevant `src/` module or the documentation files. Adhere to the existing code style.
4. **Compile and test** to verify your changes work:
   ```sh
   make
   ./rurl https://example.com
   ```
5. **Commit your changes** with clear and descriptive messages:
   ```sh
   git commit -m "feat: add support for custom request headers"
   ```
6. **Push to your branch**:
   ```sh
   git push origin feature/my-new-feature
   ```
7. Open a **Pull Request (PR)** against our main branch.

## Code Style Guidelines

- **Formatting**: Indent with standard tabs (width 8) matching the existing `src/` layout.
- **Simplicity**: Keep functions focused, clean, and under 100 lines where practical.
- **Safety**: Ensure you do not introduce memory leaks (e.g. always cleanup buffers and free structures on all error paths).
- **Compiler**: Clang is the project standard and the default `CC` in the
  `Makefile`. GCC is not a supported build target.
- **Warnings**: Code must compile without warnings with `-Wall -Wextra -Wpedantic` enabled.
- **Standard**: The source targets C11 with POSIX.1-2008 interfaces.
- **Error paths**: Free every allocation and close every socket/SSL object on
  all exits. Prefer a single cleanup label in functions that acquire multiple
  resources.

## Source Layout

| File | Responsibility |
| :--- | :--- |
| `src/main.c` | Option parsing, usage/version output, and global configuration. |
| `src/rurl.h` | Shared constants, the `url_parts` and `rurl_config` types, and module declarations. |
| `src/util.c` | Allocation helpers, integer parsing, header-value validation, and sanitized diagnostics. |
| `src/url.c` | URL parsing, dot-segment normalization, redirect resolution, and `Host` header construction. |
| `src/net.c` | Name resolution, non-blocking connect with timeout, TLS setup, and raw request/response I/O. |
| `src/http.c` | Request orchestration and redirect handling. |
| `src/response.c` | HTTP status/header parsing, response framing, and body streaming. |
| `src/http_internal.h` | Private interfaces shared by the HTTP transaction and response modules. |

Keep shared module interfaces in `src/rurl.h`, put private cross-module
interfaces in `src/http_internal.h`, and make single-module helpers `static`
to the file that uses them.

## Testing Changes

At minimum, build with warnings as errors and run a handful of real requests:

```sh
make clean
make CC=clang CFLAGS="-O2 -std=c11 -Wall -Wextra -Wpedantic -Werror"
./rurl https://example.com
./rurl -k https://localhost:8443/
./rurl -n 0 https://httpbin.org/redirect/1
```

For memory and undefined-behavior checks:

```sh
clang -std=c11 -O1 -g -fsanitize=address,undefined \
    -fno-omit-frame-pointer -Isrc -o rurl-asan \
    src/main.c src/util.c src/url.c src/net.c src/http.c src/response.c \
    -lssl -lcrypto
./rurl-asan https://example.com
```

Also exercise malformed responses with a local test server where practical:
truncated `Content-Length` bodies, chunked bodies, redirects, `204`/`205`/`304`
responses, and informational `1xx` responses.
