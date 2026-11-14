# Contributing to rurl

Thank you for your interest in contributing! We welcome bug fixes, documentation improvements, and feature contributions.

## Steps to Contribute

1. **Fork** the repository on GitHub.
2. **Create a branch** for your changes:
   ```sh
   git checkout -b feature/my-new-feature
   ```
3. **Make your changes** in `rurl.c` or the documentation files. Adhere to the existing code style.
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

- **Formatting**: Indent with standard tabs (width 8) matching the existing `rurl.c` layout.
- **Simplicity**: Keep functions focused, clean, and under 100 lines where practical.
- **Safety**: Ensure you do not introduce memory leaks (e.g. always cleanup buffers and free structures on all error paths).
- **Warnings**: Code must compile without warnings with `-Wall -Wextra -Wpedantic` enabled.
- **Standard**: The source targets C11 with POSIX.1-2008 interfaces.
- **Error paths**: Free every allocation and close every socket/SSL object on
  all exits. Prefer a single cleanup label in functions that acquire multiple
  resources.

## Testing Changes

At minimum, build with warnings as errors and run a handful of real requests:

```sh
make clean
make CFLAGS="-O2 -std=c11 -Wall -Wextra -Wpedantic -Werror"
./rurl https://example.com
./rurl -k https://localhost:8443/
./rurl -n 0 https://httpbin.org/redirect/1
```

For memory and undefined-behavior checks:

```sh
gcc -std=c11 -O1 -g -fsanitize=address,undefined \
    -fno-omit-frame-pointer -o rurl-asan rurl.c -lssl -lcrypto
./rurl-asan https://example.com
```

Also exercise malformed responses with a local test server where practical:
truncated `Content-Length` bodies, chunked bodies, redirects, `204`/`205`/`304`
responses, and informational `1xx` responses.
