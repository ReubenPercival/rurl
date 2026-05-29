# Usage Guide

`rurl` is a fast, terminal-friendly tool for fetching URLs. Below is a detailed reference of its command-line flags and examples.

## Synopsis

```sh
rurl [-A USERAGENT] [-k] [-n MAXREDIRS] [-t TIMEOUT] URL
```

## Options & Flags

| Flag | Argument | Default | Description |
| :--- | :--- | :--- | :--- |
| `-A` | `USERAGENT` | `rurl/0.1.3` | Set a custom HTTP `User-Agent` request header. |
| `-k` | *(None)* | Disabled | Allow insecure SSL connections (skips SSL/TLS certificate verification). |
| `-n` | `MAXREDIRS` | `20` | Set the maximum limit for HTTP redirects to follow. |
| `-t` | `TIMEOUT` | `30` | Socket timeout duration in seconds. |
| `-h` | *(None)* | - | Show a brief help message with available flags and exit. |
| `-V` | *(None)* | - | Show the current version of `rurl` and exit. |

---

## Examples

### 1. Basic Fetch
Fetch the plain-text/HTML payload of any HTTP or HTTPS URL:
```sh
rurl https://example.com
```

### 2. Override the User-Agent
Mimic a modern web browser or use a custom application agent:
```sh
rurl -A "Mozilla/5.0 (Windows NT 10.0; Win64; x64)" https://httpbin.org/user-agent
```

### 3. Handle Self-Signed or Insecure SSL Certificates
Skip certificate authority validation when testing local, self-signed environments:
```sh
rurl -k https://localhost:8443/api
```

### 4. Adjust Redirect Limits
Limit the client from following infinite or excessive redirect loops:
```sh
rurl -n 3 https://httpbin.org/redirect/5
```

### 5. Tighten Timeout Rules
Set a quick response check limit (e.g., 5 seconds) to prevent hung CLI sessions:
```sh
rurl -t 5 https://slow-loading-site.com
```
