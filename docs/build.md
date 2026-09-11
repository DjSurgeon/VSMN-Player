# 🛠️ Build System & CI/CD Pipeline

VSMN-Player uses an industrial-grade CI/CD pipeline integrated directly with GitHub Actions. Our infrastructure ensures every commit is linted, analyzed, built, and aggressively tested before merging.

---

## 🏗️ Build Infrastructure

The project uses **CMake** as the build generator and **Conan 2** as the C++ package manager.

### Local Development (DevContainers)

We provide a zero-friction developer experience via Docker DevContainers. The container is pre-loaded with:

* Clang & GCC toolchains (C++20)
* Conan 2 for dependencies (`spdlog`, `libcurl`, `gtest`)
* Code formatters (`clang-format`, `clang-tidy`)

```bash
# Example: Running CMake build in the container
docker-compose exec dev bash -c "cmake --build build/Debug -j4"
```

---

## 🛡️ Static Analysis (Linting)

Our pipeline strictly enforces the **Google C++ Style Guide** and modern C++ best practices.

* **`clang-format`**: Ensures all code conforms to our `.clang-format` rules. If a single brace is out of place, the CI pipeline will fail.
* **`clang-tidy`**: Performs deep static analysis to catch bugs, enforce `readability-braces-around-statements`, detect pessimizings moves, and suggest C++20 modernizations.

---

## 🦠 Sanitizers (ASan & TSan)

Because VSMN-Player handles raw memory (Zero-Copy) and multi-threading, standard unit tests aren't enough. We compile our tests with Google's Sanitizers:

### AddressSanitizer (ASan) & UndefinedBehaviorSanitizer (UBSan)

* **What it does:** Replaces `malloc`/`free` to track memory allocations.
* **Catches:** Memory leaks, Use-After-Free, buffer overflows, and undefined behavior.

### ThreadSanitizer (TSan)

* **What it does:** Instruments memory accesses to track threads.
* **Catches:** Data races, deadlocks, and unsafe thread operations.

*Note: ASan and TSan are mutually exclusive and are run in parallel matrix jobs in the CI pipeline.*

---

## 🚀 GitHub Actions Workflow (`ci-linux.yml`)

Our automated CI pipeline consists of 6 core stages:

1. **Static Analysis:** `clang-format`, `clang-tidy`, `markdownlint`, and `shellcheck`.
2. **Code Coverage:** Generates HTML reports using `gcovr` to ensure our test suite covers critical paths.
3. **Build & Test:** Compiles in Debug mode (with ASan/UBSan) and executes `ctest`.
4. **Thread Safety:** Compiles and tests specifically with TSan enabled.
5. **Release Build:** Compiles with `-O3 -march=native` to produce the highly-optimized production binary.
6. **Docker Build:** Packages the release binary into a lightweight production image (`Dockerfile.prod`).
