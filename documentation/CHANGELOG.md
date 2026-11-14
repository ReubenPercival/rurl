# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## Unreleased

### Fixed
- Handle informational (`1xx`) responses and reject unexpected protocol
  switches (`101`) instead of treating the first response as final.
- Follow only the standard redirect status codes (`301`, `302`, `303`, `307`
  and `308`); no longer treat `300`, `304`, `305` or `306` as redirects.
- Treat `205 Reset Content` responses as bodyless, like `204` and `304`.
- Parse the HTTP status line strictly, including the version and the exact
  three-digit status code.
- Parse `Transfer-Encoding` as a token list and reject messages where
  `chunked` is not the final coding. `Content-Length` is ignored when a
  transfer coding is present, as required by HTTP.
- Validate chunk-size syntax and require the empty line after each chunk.
- Remove dot segments from the initial URL and from absolute redirect
  targets, not just from relative redirects.
- Use the last `@` when separating URL userinfo from the host, so passwords
  containing a literal `@` do not corrupt the host.
- Reject NUL bytes in response headers and chunked framing, and cap the size
  of completed header blocks, including after informational responses.
- Sanitize server-controlled URLs and host names in diagnostics so terminal
  control sequences cannot be injected through a redirect.
- Strip a trailing dot from DNS names when selecting the TLS server name and
  verifying the peer certificate.
- Fail cleanly if socket timeouts cannot be applied instead of potentially
  blocking forever.

### Changed
- Send `Accept: */*` and `Accept-Encoding: identity` with requests.
- Ignore `SIGPIPE` so a closed pipe or reset connection is reported as an
  error instead of terminating the process abruptly.
- Check the final `fflush(stdout)` result so write failures affect the exit
  status.
- Report certificate verification details when a TLS handshake fails, and
  check the return value of `SSL_set_fd`.
- Limit the number of consecutive informational responses.

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

