# Contributing to rurl

We welcome contributions! Follow these steps:

1. **Fork** the repository.
2. **Create a branch** for your change:
   ```sh
   git checkout -b <feature-or-fix-name>
   ```
3. **Make your changes** – adhere to the existing coding style (`-Wall -Wextra`).
4. **Run the build** and ensure the binary works:
   ```sh
   make && ./rurl https://example.com
   ```
5. **Add tests** if you modify behavior (currently none – consider adding a CI script).
6. **Commit** with a clear message and push:
   ```sh
   git commit -m "Brief description of change"
   git push origin <branch>
   ```
7. Open a **Pull Request** – describe what the PR does and reference any related issues.

### Code style
- Use spaces (4 per indent).
- Keep functions under 80 lines where possible.
- Add comments for non‑obvious logic.
- Run `clang-format -i` if a style file is provided.
