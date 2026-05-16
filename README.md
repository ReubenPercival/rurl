# rurl - Minimal raw URL fetcher

By [Socialist Software Foundation](https://codeberg.org/SocialistSofwareFoundation)

Fetch a URL and pipe the raw body to stdout. Nothing more.

## Usage

```
rurl URL
```

## Example

```sh
rurl https://example.com
rurl https://api.github.com/users/example | jq .
```

## Build

```sh
make
```

Requires libcurl development headers. On Debian/Ubuntu:
```sh
sudo apt install libcurl4-openssl-dev
```

On Alpine:
```sh
apk add curl-dev
```

## Install

```sh
sudo make install    # installs to /usr/local/bin/rurl
```

## License

GNU General Public License v3.0 - see [LICENSE](LICENSE) for details.