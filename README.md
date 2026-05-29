# rurl – Minimal Raw URL Fetcher

![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)

`rurl` is a tiny, self-contained command-line utility that fetches a URL and writes the raw response body to *stdout*. It is implemented directly using raw POSIX sockets and native OpenSSL functions—avoiding heavy dependency footprints like `libcurl`.

---

## Features

- **Extremely Small**: ~20 KB lightweight binary on typical Linux distributions.
- **Protocols**: Seamlessly handles both custom `http://` and secure `https://` URLs.
- **Configurable**: Define custom User-Agents, maximum redirect rules, and connection timeouts.
- **Testing-Friendly**: Optional insecure mode (`-k`) allows skipping certificate verification when testing local/self-signed deployments.

---

## Installation

Ensure you have a C compiler and OpenSSL headers installed before building.

```sh
# Build the binary
make

# Install globally (requires administrative/root privileges)
sudo make install
```

The binary is installed to **/usr/local/bin/rurl** by default.

---

## Usage

```sh
rurl [-A USERAGENT] [-k] [-n MAXREDIRS] [-t TIMEOUT] URL
```

### Options Reference

| Flag | Argument | Default | Description |
| :--- | :--- | :--- | :--- |
| `-A` | `USERAGENT` | `rurl/0.1.3` | Set a custom HTTP *User-Agent* request header. |
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

---

## Build Prerequisites

To compile the codebase successfully, your environment requires a C compiler and the OpenSSL development files:

- **Debian / Ubuntu**: `sudo apt install build-essential libssl-dev`
- **Alpine Linux**: `apk add build-base openssl-dev`
- **macOS**: `brew install openssl`

---

## Detailed Documentation

More comprehensive guides and resources can be found in our dedicated [documentation/](documentation/) folder:

- [BUILDING.md](documentation/BUILDING.md) – Thorough compiler setups and troubleshooting.
- [USAGE.md](documentation/USAGE.md) – Full CLI options, parameters, and examples.
- [CONTRIBUTING.md](documentation/CONTRIBUTING.md) – Contribution pathways, coding standards, and developer guide.
- [CHANGELOG.md](documentation/CHANGELOG.md) – Release iterations, version updates, and history.
- [LICENSE.md](documentation/LICENSE.md) – GPL-3.0 licensing details.

---

## License

This project is licensed under the GPL-3.0 License. See the [LICENSE](LICENSE) file for the complete license text.
