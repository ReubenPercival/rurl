# rurl – Minimal Raw URL Fetcher

![License: EUPL v1.2](https://img.shields.io/badge/License-EUPL_1.2-blue.svg)

`rurl` is a tiny command-line utility that fetches a URL and writes the raw response body to *stdout*. It is implemented directly using raw POSIX sockets and native OpenSSL functions—avoiding heavy dependency footprints like `libcurl`. The source is split into focused modules under [`src/`](src/).

---

## Features

- **Extremely Small**: ~27 KB lightweight binary on typical Linux distributions.
- **Protocols**: Seamlessly handles both custom `http://` and secure `https://` URLs.
- **Configurable**: Define custom User-Agents, maximum redirect rules, and connection timeouts.
- **Testing-Friendly**: Optional insecure mode (`-k`) allows skipping certificate verification when testing local/self-signed deployments.

---

## Installation

Clang is the standard compiler for this project. Ensure you have `clang` and the OpenSSL headers installed before building.
```sh
# Build the binary
make

# Install globally (requires administrative/root privileges)
sudo make install
```

The binary is installed to **/usr/local/bin/rurl** by default.

### Source Layout

| File | Responsibility |
| :--- | :--- |
| `src/main.c` | Command-line option parsing, usage output, and program configuration. |
| `src/rurl.h` | Shared constants, configuration, URL types, and module interfaces. |
| `src/http_internal.h` | Private interfaces shared by the HTTP transaction and response modules. |
| `src/util.c` | String allocation helpers, argument parsing, and safe diagnostic output. |
| `src/url.c` | URL parsing, path normalization, redirect resolution, and `Host` header construction. |
| `src/net.c` | DNS resolution, socket connections, timeouts, TLS setup, and request/response I/O. |
| `src/http.c` | Request orchestration and redirect handling. |
| `src/response.c` | HTTP status/header parsing, response framing, and body streaming. |

---

## Usage

```sh
rurl [-A USERAGENT] [-k] [-n MAXREDIRS] [-t TIMEOUT] URL
```

### Options Reference

| Flag | Argument | Default | Description |
| :--- | :--- | :--- | :--- |
| `-A` | `USERAGENT` | `rurl/0.1.6` | Set a custom HTTP *User-Agent* request header. |
| `-k` | *(None)* | Disabled | Allow insecure SSL connections (skips certificate verification). |
| `-n` | `MAXREDIRS` | `20` | Set the maximum limit for HTTP redirects to follow. |
| `-t` | `TIMEOUT` | `30` | Socket timeout duration in seconds. |
| `-h` | *(None)* | - | Show a brief help message with available flags and exit. |
| `-V` | *(None)* | - | Show the current version of `rurl` and exit. |

### Practical Examples

```sh
# Perform a simple GET request
rurl https://example.com

# Pipeline API payloads to json processing utilities
rurl https://api.github.com/users/ReubenPercival | jq .

# Overriding request parameters
rurl -A "MyCustomCrawler/1.0" -t 5 -n 3 https://example.com
```

### Combining Flags

Multiple options can be supplied in a single invocation. Both the separate and
clustered short-option forms are supported:

```sh
# Separate flags: user agent, timeout, and redirect limit together
rurl -A "Mozilla/5.0" -t 5 -n 3 https://example.com

# Clustered short flags: -k (insecure) and -t 5 combined
rurl -kt 5 https://example.com
```

---

## Build Prerequisites

To compile the codebase successfully, your environment requires Clang (the project's standard compiler) and the OpenSSL development files:

- **Debian / Ubuntu**: `sudo apt install clang libssl-dev`
- **Alpine Linux**: `apk add clang openssl-dev`
- **macOS**: `xcode-select --install` (provides Clang) and `brew install openssl`

---

## Detailed Documentation

More comprehensive guides and resources can be found in our dedicated [documentation/](documentation/) folder:

- [BUILDING.md](documentation/BUILDING.md) – Thorough compiler setups and troubleshooting.
- [USAGE.md](documentation/USAGE.md) – Full CLI options, parameters, and examples.
- [CONTRIBUTING.md](documentation/CONTRIBUTING.md) – Contribution pathways, coding standards, and developer guide.
- [CHANGELOG.md](documentation/CHANGELOG.md) – Release iterations, version updates, and history.
- [LICENSE.md](documentation/LICENSE.md) – EUPL-1.2 licensing details.

---

## License

This project is licensed under the EUPL-1.2 License. See the [LICENSE](LICENSE) file for the complete license text.
