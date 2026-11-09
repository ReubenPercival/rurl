# Changelog

## 0.1.6 – 2026‑11‑09
- Reject control characters and CR/LF in the `-A` User-Agent value, and in
  URL hosts, to prevent request-header injection.
- Accept URL schemes case-insensitively (`HTTP://`, `HTTPS://`).
- Reject redirects to unsupported URI schemes instead of treating them as
  relative paths.
- Build request lines dynamically so long URLs and User-Agent values are no
  longer truncated at 4096 bytes.
- Reject malformed, negative, or conflicting `Content-Length` headers.
- Treat 204/304 responses as bodyless.
- Reject extra positional arguments.
- Send explicit help and version output to stdout.

## 0.1.5 – 2026‑11‑09
- Enable TLS certificate and hostname verification by default; `-k` now
  actually disables verification.
- Follow relative, root-relative, query-only, and scheme-relative redirects.
- Parse query strings correctly when the URL has no path component.
- Fix an unbounded stack copy for long bracketed (IPv6) hosts.
- Apply the timeout to connection establishment and to sends, not just reads.
- Stream response bodies to stdout instead of buffering the whole body.
- Decode chunked transfer encoding and honour `Content-Length`; detect
  truncated responses.
- Handle interrupted reads/writes and unexpected TLS EOF without corrupting
  the exit status.
- Validate `-n` and `-t` values with `strtol` instead of `atoi`.
- Derive the default User-Agent from `RURL_VERSION`.
- Return exit status 1 (instead of 255) for runtime errors.
- Update the source and documentation to the EUPL-1.2 licence.

## 0.1.4 – 2026‑05‑24
- Bumped version to 0.1.4.
- Added single‑time SSL initialization guard.
- Introduced `RURL_VERSION` macro.
- Unified error‑path cleanup to free all resources and close sockets/SSL.
- Replaced manual case‑sensitive Location header parsing with `strncasecmp`.
- Switched numeric option parsing to `strtol` with validation and bounds checking.
- Added bounds check for IPv6 host buffer copying.
- Made read/write loops robust against `EINTR` and partial writes.
- Added comprehensive documentation folder (README, BUILDING, USAGE, CONTRIBUTING, LICENSE).
- Updated existing documentation with build and usage instructions.
- Minor refactoring and code comments.

## 0.1.3 – 2026‑05‑24
- Added SSL initialization guard.
- Implemented unified cleanup to prevent memory leaks.
- Replaced manual Location header detection with case‑insensitive `strncasecmp`.
- Switched numeric parsing to `strtol` with validation.
- Added version macro `RURL_VERSION`.
- Bounded IPv6 host buffer copy.
- Made read/write loops robust against `EINTR` and partial writes.
- Minor refactoring and documentation updates.

## 0.1.2 – previous
- Initial release with basic fetching, redirects, and TLS support.

