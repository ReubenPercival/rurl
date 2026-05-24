# Changelog

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

