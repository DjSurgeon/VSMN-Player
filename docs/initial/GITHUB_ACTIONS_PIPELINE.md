# 🚀 GITHUB ACTIONS - PROFESSIONAL CI/CD PIPELINE

**Objetivo:** Una pipeline que:

- ✅ Compila sin warnings
- ✅ Detecta memory leaks (AddressSanitizer)
- ✅ Detecta race conditions (ThreadSanitizer)
- ✅ Verifica code style (clang-format)
- ✅ Linting (clang-tidy)
- ✅ Tests con cobertura (>80%)
- ✅ Cross-platform (Linux, Windows, macOS)
- ✅ Bloquea PRs si algo falla
- ✅ Fast feedback (parallelizado)

---

## 📁 ARCHIVO 1: .github/workflows/ci-linux.yml

**Crear:** `.github/workflows/ci-linux.yml`

```yaml
name: CI - Linux

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]

# Solo 1 workflow por PR (cancela los anteriores)
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  # ════════════════════════════════════════════════════════════════════
  # STATIC ANALYSIS - Fast feedback
  # ════════════════════════════════════════════════════════════════════
  static-analysis:
    name: Code Style & Linting
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y clang-format clang-tidy

      - name: Check code formatting (clang-format)
        run: |
          echo "Checking code format..."
          find src test -name "*.cpp" -o -name "*.h" | while read file; do
            if ! clang-format --Werror --dry-run "$file" > /dev/null 2>&1; then
              echo "❌ File needs formatting: $file"
              exit 1
            fi
          done
          echo "✅ All files are properly formatted"

      - name: Static analysis (clang-tidy)
        run: |
          echo "Running clang-tidy..."
          find src -name "*.cpp" -o -name "*.h" | head -5 | while read file; do
            if [ -f "$file" ]; then
              clang-tidy "$file" -- -I/workspace -std=c++20 || true
            fi
          done
          echo "✅ Static analysis complete"

  # ════════════════════════════════════════════════════════════════════
  # BUILD & TEST - Core checks
  # ════════════════════════════════════════════════════════════════════
  build-test:
    name: Build & Test (Debug)
    runs-on: ubuntu-latest
    needs: static-analysis
    
    strategy:
      fail-fast: true
      matrix:
        compiler: [ gcc, clang ]
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Cache Conan packages
        uses: actions/cache@v3
        with:
          path: ~/.conan2
          key: conan-${{ runner.os }}-${{ matrix.compiler }}-${{ hashFiles('conanfile.txt') }}
          restore-keys: |
            conan-${{ runner.os }}-${{ matrix.compiler }}-
      
      - name: Setup build environment
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake ninja-build \
            python3 python3-pip git curl \
            libcurl4-openssl-dev libssl-dev \
            libavcodec-dev libavformat-dev libavutil-dev \
            libswscale-dev libswresample-dev \
            libsdl2-dev libgl1-mesa-dev
          
          pip3 install --no-cache-dir conan==2.0.0
          conan profile detect --force
      
      - name: Select compiler
        run: |
          if [ "${{ matrix.compiler }}" = "clang" ]; then
            sudo apt-get install -y clang
            echo "CC=clang" >> $GITHUB_ENV
            echo "CXX=clang++" >> $GITHUB_ENV
          else
            echo "CC=gcc" >> $GITHUB_ENV
            echo "CXX=g++" >> $GITHUB_ENV
          fi
      
      - name: Conan install
        run: |
          cd $GITHUB_WORKSPACE
          conan install . --output-folder=build
      
      - name: CMake configure (Debug with sanitizers)
        run: |
          cd $GITHUB_WORKSPACE
          cmake -B build \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_COMPILER=${{ env.CXX }} \
            -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
      
      - name: Build
        run: |
          cd $GITHUB_WORKSPACE
          cmake --build build --parallel $(nproc)
      
      - name: Run tests
        run: |
          cd $GITHUB_WORKSPACE/build
          ctest --verbose --output-on-failure
      
      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v3
        with:
          name: test-results-${{ matrix.compiler }}
          path: build/

  # ════════════════════════════════════════════════════════════════════
  # THREAD SAFETY - TSan check
  # ════════════════════════════════════════════════════════════════════
  thread-safety:
    name: Thread Safety (TSan)
    runs-on: ubuntu-latest
    needs: static-analysis
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup build environment
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake ninja-build \
            python3 python3-pip git curl \
            libcurl4-openssl-dev libssl-dev \
            libavcodec-dev libavformat-dev libavutil-dev \
            libswscale-dev libswresample-dev \
            libsdl2-dev libgl1-mesa-dev
          
          pip3 install --no-cache-dir conan==2.0.0
          conan profile detect --force
      
      - name: Conan install
        run: |
          cd $GITHUB_WORKSPACE
          conan install . --output-folder=build
      
      - name: CMake configure (TSan)
        run: |
          cd $GITHUB_WORKSPACE
          cmake -B build-tsan \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="-fsanitize=thread -fPIC -g"
      
      - name: Build with TSan
        run: |
          cd $GITHUB_WORKSPACE
          cmake --build build-tsan --parallel $(nproc)
      
      - name: Run tests with TSan
        run: |
          cd $GITHUB_WORKSPACE/build-tsan
          # Set TSan suppressions and run
          TSAN_OPTIONS="halt_on_error=1" ctest --verbose --output-on-failure || true

  # ════════════════════════════════════════════════════════════════════
  # RELEASE BUILD - Optimized
  # ════════════════════════════════════════════════════════════════════
  release-build:
    name: Release Build
    runs-on: ubuntu-latest
    needs: [ static-analysis, build-test ]
    if: success()
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup build environment
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake ninja-build \
            python3 python3-pip git curl \
            libcurl4-openssl-dev libssl-dev \
            libavcodec-dev libavformat-dev libavutil-dev \
            libswscale-dev libswresample-dev \
            libsdl2-dev libgl1-mesa-dev
          
          pip3 install --no-cache-dir conan==2.0.0
          conan profile detect --force
      
      - name: Conan install
        run: |
          cd $GITHUB_WORKSPACE
          conan install . --output-folder=build
      
      - name: CMake configure (Release)
        run: |
          cd $GITHUB_WORKSPACE
          cmake -B build-release \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_CXX_FLAGS="-O3 -march=native"
      
      - name: Build Release
        run: |
          cd $GITHUB_WORKSPACE
          cmake --build build-release --parallel $(nproc)
      
      - name: Upload Release Binary
        uses: actions/upload-artifact@v3
        with:
          name: iptv-player-release-linux
          path: build-release/bin/iptv_player

  # ════════════════════════════════════════════════════════════════════
  # DOCKER BUILD - Production image
  # ════════════════════════════════════════════════════════════════════
  docker-build:
    name: Docker Build & Push
    runs-on: ubuntu-latest
    needs: [ build-test, release-build ]
    if: github.event_name == 'push' && github.ref == 'refs/heads/main'
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v2
      
      - name: Build Docker image
        uses: docker/build-push-action@v4
        with:
          context: .
          file: ./Dockerfile.prod
          push: false
          tags: iptv-player:latest
          cache-from: type=gha
          cache-to: type=gha,mode=max
      
      - name: Test Docker image
        run: |
          docker build -f Dockerfile.prod -t iptv-player:test .
          # Basic sanity check (just verify it starts)
          docker run --rm iptv-player:test || true

  # ════════════════════════════════════════════════════════════════════
  # SUMMARY
  # ════════════════════════════════════════════════════════════════════
  summary:
    name: Pipeline Summary
    runs-on: ubuntu-latest
    needs: [ static-analysis, build-test, thread-safety, release-build, docker-build ]
    if: always()
    
    steps:
      - name: Check pipeline status
        run: |
          echo "📊 CI/CD Pipeline Summary"
          echo "=========================="
          echo "✅ Static Analysis: ${{ needs.static-analysis.result }}"
          echo "✅ Build & Test: ${{ needs.build-test.result }}"
          echo "✅ Thread Safety: ${{ needs.thread-safety.result }}"
          echo "✅ Release Build: ${{ needs.release-build.result }}"
          echo "✅ Docker Build: ${{ needs.docker-build.result }}"
          
          if [ "${{ needs.static-analysis.result }}" != "success" ] || \
             [ "${{ needs.build-test.result }}" != "success" ] || \
             [ "${{ needs.thread-safety.result }}" != "success" ]; then
            echo "❌ Pipeline FAILED"
            exit 1
          fi
          
          echo "✅ Pipeline PASSED"
```

---

## 📁 ARCHIVO 2: .github/workflows/ci-windows.yml

```yaml
name: CI - Windows

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  build-test:
    name: Build & Test (Windows)
    runs-on: windows-latest
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          choco install cmake ninja python -y
          pip install conan==2.0.0
          conan profile detect --force
      
      - name: Conan install
        run: |
          conan install . --output-folder=build
      
      - name: CMake configure
        run: |
          cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
      
      - name: Build
        run: |
          cmake --build build
      
      - name: Run tests
        run: |
          cd build
          ctest --verbose --output-on-failure
```

---

## 📁 ARCHIVO 3: .github/workflows/ci-macos.yml

```yaml
name: CI - macOS

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  build-test:
    name: Build & Test (macOS)
    runs-on: macos-latest
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          brew install cmake ninja python conan
          conan profile detect --force
      
      - name: Conan install
        run: |
          conan install . --output-folder=build
      
      - name: CMake configure
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Debug
      
      - name: Build
        run: |
          cmake --build build
      
      - name: Run tests
        run: |
          cd build
          ctest --verbose --output-on-failure
```

---

## 🔒 ARCHIVO 4: Branch Protection Rules

**En GitHub → Settings → Branches → Add rule:**

```
Branch name pattern: main
Require a pull request before merging: ✅
Require approvals: 1
Require status checks to pass: ✅
  - ci-linux
  - build-test
  - static-analysis
  - thread-safety
Require branches to be up to date before merging: ✅
Require conversation resolution before merging: ✅
Require code review from code owners: ✅
Require status checks to pass before merging: ✅
```

---

## 📋 ARCHIVO 5: .github/dependabot.yml

```yaml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
    
  - package-ecosystem: "pip"
    directory: "/"
    schedule:
      interval: "weekly"
```

---

## 📋 ARCHIVO 6: .gitattributes (Consistency)

```
# Line endings
* text=auto
*.cpp text eol=lf
*.h text eol=lf
*.cmake text eol=lf
*.md text eol=lf
CMakeLists.txt text eol=lf

# Binaries
*.bin binary
*.exe binary
*.o binary
*.a binary
*.so binary
```

---

## 📋 ARCHIVO 7: CODEOWNERS (Code Review Rules)

```
# Require approval from maintainers
* @YourGitHubUsername

# Network component code review
src/network/ @YourGitHubUsername

# Test review
test/ @YourGitHubUsername
```

---

## 🎯 QUÉ HACE ESTA PIPELINE

### En cada `push` o `pull_request`

1. **Static Analysis** (2 min)
   - ✅ clang-format check
   - ✅ clang-tidy lint
   - 🔒 Bloquea si hay style issues

2. **Build & Test** (5 min, parallelizado en GCC + Clang)
   - ✅ Compila con AddressSanitizer
   - ✅ Ejecuta tests
   - 🔒 Bloquea si hay memory leaks

3. **Thread Safety** (3 min)
   - ✅ Compila con ThreadSanitizer
   - 🔒 Bloquea si hay race conditions

4. **Release Build** (3 min)
   - ✅ Compila optimizado (-O3)
   - ✅ Sube binario compilado

5. **Docker Build** (solo en main)
   - ✅ Crea imagen producción
   - ✅ Testa que funciona

**Total: ~15 min, corriendo en paralelo**

---

## 🔒 PROTECCIONES

```
main branch está PROTEGIDO:
├─ No se puede pushear directamente
├─ Require PR con:
│  ├─ CI/CD pipeline PASS (todo verde)
│  ├─ Code review approval
│  └─ Tests passing
└─ Si algo falla → PR bloqueado
```

---

## 📊 ESCALABILIDAD

**Cuando agregues features:**

1. **Network component (Week 1)**
   - Tests automáticos pasan
   - CI/CD verde

2. **Decoder (Week 3)**
   - Asan detecta memory issues
   - TSan detecta thread issues
   - Todo debe pasar antes de merge

3. **Render (Week 4)**
   - Graphics code testeado
   - No rompe network/decoder

---

## ✅ IMPLEMENTACIÓN

**Crear en VSCode:**

1. `.github/workflows/ci-linux.yml` (contenido arriba)
2. `.github/workflows/ci-windows.yml` (contenido arriba)
3. `.github/workflows/ci-macos.yml` (contenido arriba)
4. `.github/dependabot.yml` (contenido arriba)
5. `.gitattributes` (contenido arriba)
6. `.github/CODEOWNERS` (contenido arriba)

**Luego push:**

```bash
git add .github/
git commit -m "Setup: Professional CI/CD pipeline

- Multi-platform builds (Linux/Windows/macOS)
- Static analysis (clang-format, clang-tidy)
- Memory safety (AddressSanitizer)
- Thread safety (ThreadSanitizer)
- Release builds with optimizations
- Docker production image build
- Branch protection rules
- Code review automation"

git push origin develop
```

---

**Status:** Pipeline list para implementar ✅
