# Changelog

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
