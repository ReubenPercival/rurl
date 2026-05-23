# rurl – Minimal raw URL fetcher

![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)

**rurl** is a tiny command‑line utility that fetches a URL and writes the raw response body to *stdout*. It uses only raw sockets and OpenSSL – no external HTTP libraries.

---

## Features

- Small, self‑contained binary (~20 KB on Linux)
- Supports HTTP and HTTPS
- Customisable User‑Agent string
- Configurable maximum redirects and timeout
- Optional insecure mode to skip TLS verification

---

## Installation

```sh
# Build the binary
make

# Install system‑wide (requires root)
sudo make install
```

The binary is installed to **/usr/local/bin/rurl**.

---

## Usage

```sh
rurl [-A USERAGENT] [-k] [-n MAXREDIRS] [-t TIMEOUT] URL
```

### Options

| Flag | Description |
|------|-------------|
| `-A USERAGENT` | Set a custom *User‑Agent* string (default: `rurl/0.1.3`) |
| `-k` | Allow insecure SSL connections (skip certificate verification) |
| `-n MAXREDIRS` | Maximum number of redirects (default: 20) |
| `-t TIMEOUT` | Timeout in seconds (default: 30) |
| `-h` | Show help message |
| `-V` | Show version |

### Examples

```sh
# Simple GET request
rurl https://example.com

# Pipe JSON output to jq
rurl https://api.github.com/users/example | jq .
```

---

## Build requirements

- **gcc** (or compatible C compiler)
- OpenSSL development headers

On Debian/Ubuntu:

```sh
sudo apt install build-essential libssl-dev
```

On Alpine Linux:

```sh
apk add build-base openssl-dev
```

---

## License

GPL‑3.0‑only. See the [LICENSE](LICENSE) file for details.
