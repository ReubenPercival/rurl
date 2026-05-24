# Usage Guide

```sh
rurl [options] URL
```

## Options
- `-A USERAGENT` – set the `User‑Agent` header (default `rurl/0.1.3`).
- `-k` – allow insecure SSL (skip certificate verification).
- `-n MAXREDIRS` – maximum number of redirects (default 20).
- `-t TIMEOUT` – socket timeout in seconds (default 30).
- `-h` – show help.
- `-V` – show version.

## Examples
```sh
# Basic request
rurl https://example.com

# Custom user‑agent
rurl -A "MyCrawler/1.0" https://example.com

# Follow up to 5 redirects only
rurl -n 5 https://short.url/abc

# Insecure HTTPS (self‑signed certs)
rurl -k https://self‑signed.local
```
