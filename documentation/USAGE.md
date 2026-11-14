# Usage Guide

`rurl` is a fast, terminal-friendly tool for fetching URLs. Below is a detailed reference of its command-line flags, behavior, and examples.

## Synopsis

```sh
rurl [-A USERAGENT] [-k] [-n MAXREDIRS] [-t TIMEOUT] URL
```

## Options & Flags

| Flag | Argument | Default | Description |
| :--- | :--- | :--- | :--- |
| `-A` | `USERAGENT` | `rurl/0.1.6` | Set a custom HTTP `User-Agent` request header. |
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

## Behavior

- `rurl` writes the raw response body to standard output and diagnostics to
  standard error.
- Bodies are streamed, so large responses do not need to fit in memory.
- `Content-Length`, chunked transfer encoding, and close-delimited bodies are
  supported. `Content-Encoding` is **not** decoded; the response is written
  exactly as received and the request asks for `Accept-Encoding: identity`.
- Only `301`, `302`, `303`, `307`, and `308` responses with a non-empty
  `Location` header are followed. Relative, root-relative, query-only,
  scheme-relative, and absolute redirect targets are resolved. Dot segments
  (`.` and `..`) are removed from the initial URL and from redirect targets.
- Informational responses (`100`, `102`, `103`, and other `1xx` codes except
  `101`) are skipped. A `101 Switching Protocols` response is rejected because
  `rurl` does not support protocol upgrades.
- The timeout applies to connection establishment and to each socket read or
  write. It is not a total deadline for the whole transfer.
- DNS resolution is performed by the operating system and is not covered by
  the timeout.

## Exit Status

| Status | Meaning |
| :--- | :--- |
| `0` | The final response had a `2xx` status and the body was written successfully. |
| `1` | Usage error, invalid URL, network/TLS error, malformed response, truncated body, write failure, or a final non-`2xx` status. |

For a final non-`2xx` response (one that is not followed as a redirect), the
body is still written to standard output before the error is reported,
matching the behavior of a plain HTTP client. Redirect response bodies are
discarded because the redirect target is fetched instead.

## Security Notes

- TLS certificate and hostname verification are enabled by default.
- `-k` disables both certificate and hostname verification. Do not use it in
  production.
- C0 control characters (except tab in `-A`) and DEL are rejected in
  request-relevant parts of URLs and in `-A` values to prevent request header
  injection.
- Server-controlled URLs and host names are escaped before being printed in
  diagnostics so they cannot inject terminal control sequences.
- `rurl` does not execute response content, follow HTML/CSS/JavaScript
  references, or store cookies.

## Limitations

- `rurl` performs unauthenticated `GET` requests only. It does not support
  request bodies, custom headers beyond `-A`, proxies, authentication,
  cookies, HTTP/2, or response decompression.
- Internationalized domain names must be supplied in their ASCII
  (punycode/IDNA) form.
- Only `http://` and `https://` URLs are accepted.
