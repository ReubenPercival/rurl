# rurl – Minimal Raw URL Fetcher

`rurl` is a tiny, self-contained command-line HTTP/HTTPS client. Designed to be fast and lightweight, it performs raw GET requests using raw sockets and OpenSSL without the weight of larger HTTP libraries like `libcurl`.

## Key Features

- **Tiny Binary**: Under ~20 KB on standard Linux environments.
- **Protocols**: Supports both `http://` and secure `https://` URLs.
- **No External HTTP Libraries**: Built directly on POSIX sockets and native OpenSSL functions.
- **Configurable**: Easily set custom User-Agents, maximum redirect counts, and network timeouts.
- **Developer-Friendly**: Features an optional insecure mode (`-k`) to skip SSL/TLS certificate validation for local development or testing.

---

## Quick Start

### Build & Install

Ensure you have a C compiler and the OpenSSL development headers installed, then build:

```sh
make
sudo make install
```

For more detailed compilation instructions, see [documentation/BUILDING.md](documentation/BUILDING.md).

### Basic Usage

Fetch raw HTML/JSON directly to stdout:

```sh
# Fetch a webpage
rurl https://example.com

# Fetch API payload and pretty-print with jq
rurl https://api.github.com/users/ReubenPercival | jq .
```

For more usage patterns and flag options, see [documentation/USAGE.md](documentation/USAGE.md).

---

## Documentation Index

- [BUILDING.md](documentation/BUILDING.md) – Compiler requirements and system installation guides.
- [USAGE.md](documentation/USAGE.md) – CommandLine flags, switches, and usage patterns.
- [CONTRIBUTING.md](documentation/CONTRIBUTING.md) – Contribution guidelines and project coding styles.
- [CHANGELOG.md](documentation/CHANGELOG.md) – Feature progression and history of versions.
- [LICENSE.md](documentation/LICENSE.md) – GPL-3.0 licensing details.

---

## License

This project is licensed under the GPL-3.0 License. See the [LICENSE](LICENSE) file for the complete license text.
