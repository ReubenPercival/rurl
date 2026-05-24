# rurl – Minimal raw URL fetcher

`rurl` is a tiny command‑line HTTP/HTTPS client that performs a raw GET request without pulling in libcurl. It supports:

- Custom User‑Agent (`-A`)
- Insecure SSL (`-k`)
- Redirect handling (`-n` to set max redirects)
- Connection timeout (`-t`)
- Verbose error messages

## Building
```sh
mkdir -p build && cd build
cmake ..   # if a CMakeLists.txt exists, otherwise use the Makefile
make
```
Or simply:
```sh
gcc -Wall -Wextra -o rurl rurl.c -lssl -lcrypto
```

## Usage
```sh
rurl [options] URL
```
Options are listed with `-h`.

## License
GPL‑3.0‑or‑later – see `LICENSE`.
