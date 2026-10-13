# Contributing to VSMN-Player

First off, thank you for considering contributing to VSMN-Player! It's people like you that make this tool great.

## Code of Conduct

By participating in this project, you are expected to uphold our standards of professionalism and engineering excellence. We believe in constructive code reviews, empathetic communication, and rigorous technical debates.

## Branching Strategy

We follow a strict Git Flow model:

- **`main`**: The sacred branch. Always stable, always passing CI. Contains the latest release. Direct commits are forbidden.
- **`develop`**: The integration branch. All feature branches merge here first.
- **`feature/<name>`**: For new features. Branched from `develop`.
- **`fix/<name>`**: For bug fixes. Branched from `develop` (or `main` if it's a hotfix).

## Pull Request Process

1. Ensure your code complies with C++20 standards.
2. Run `clang-format` and `clang-tidy` before committing (we recommend using our `./scripts/install-hooks.sh`).
3. Ensure all unit tests pass locally (`ctest`) and code coverage remains above 90%.
4. **The Devil Protocol**: For any risky, architectural, or irreversible changes (e.g. public API modifications, memory pooling, multi-threading), you must run the `/deal` workflow with the `devil` agent before writing code. Ensure you document the Devil's verdict (PROCEED-WITH-CONDITIONS) in your PR description.
5. Update the `README.md` or `docs/` with details of changes to the interface.
6. You may merge the Pull Request in once you have the sign-off of at least one core maintainer and the CI pipeline (ASan, TSan, Coverage) gives a green light.

## Agent Guidelines
We heavily utilize Google Antigravity agents (`opencode`, `builder`, `devil`, etc.).
- **Zero Mocks:** Agents are strictly instructed to avoid FFmpeg mocks.
- **Caveman Mode:** Code reviews and commit messages follow the ultra-compressed "Caveman" style (terse, factual, no fluff).

## Setting up your environment

Please refer to the [DEVELOPMENT.md](DEVELOPMENT.md) and [DOCKER.md](DOCKER.md) files for zero-friction setup using VSCode DevContainers.
