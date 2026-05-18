# rurl - Minimal raw URL fetcher

By [Socialist Software Foundation](https://codeberg.org/SocialistSofwareFoundation)

Fetch a URL and pipe the raw body to stdout. Nothing more.

Built with raw sockets + OpenSSL (no libcurl).

## Usage

```
rurl [-A USERAGENT] [-k] [-n MAXREDIRS] [-t TIMEOUT] URL
```

### Options

- `-A USERAGENT`  Set custom User-Agent string (default: rurl/1.0)
- `-k`           Allow insecure SSL connections (skip certificate verification)
- `-n MAXREDIRS` Maximum number of redirects (default: 20)
- `-t TIMEOUT`   Timeout in seconds (default: 30)
- `-h`           Show this help message

## Example

```sh
rurl https://example.com
rurl https://api.github.com/users/example | jq .
```

## Build

```sh
make
```

Requires OpenSSL development headers. On Debian/Ubuntu:
```sh
sudo apt install libssl-dev
```

On Alpine:
```sh
apk add openssl-dev
```

## Install

```sh
sudo make install    # installs to /usr/local/bin/rurl
```

## License

GNU General Public License v3.0 - see [LICENSE](LICENSE) for details.