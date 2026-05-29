# Contributing to rurl

Thank you for your interest in contributing! We welcome bug fixes, documentation improvements, and feature contributions.

## Steps to Contribute

1. **Fork** the repository on GitHub.
2. **Create a branch** for your changes:
   ```sh
   git checkout -b feature/my-new-feature
   ```
3. **Make your changes** in `rurl.c` or the documentation files. Adhere to the existing code style.
4. **Compile and test** to verify your changes work:
   ```sh
   make
   ./rurl https://example.com
   ```
5. **Commit your changes** with clear and descriptive messages:
   ```sh
   git commit -m "feat: add support for custom request headers"
   ```
6. **Push to your branch**:
   ```sh
   git push origin feature/my-new-feature
   ```
7. Open a **Pull Request (PR)** against our main branch.

## Code Style Guidelines

- **Formatting**: Indent with standard tabs (width 8) matching the existing `rurl.c` layout.
- **Simplicity**: Keep functions focused, clean, and under 100 lines where practical.
- **Safety**: Ensure you do not introduce memory leaks (e.g. always cleanup buffers and free structures on all error paths).
- **Warnings**: Code must compile without warnings with `-Wall -Wextra` flags enabled.
