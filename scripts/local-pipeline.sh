#!/bin/bash
# =============================================================================
# VSMN-Player — Local Quality Pipeline
# =============================================================================
# Replica exactamente lo que hace GitHub Actions (ci-linux.yml) en tu máquina.
# Ejecútalo dentro del contenedor de desarrollo:
#   docker-compose exec dev bash -c "./scripts/local-pipeline.sh"
# =============================================================================

set -e  # Abortar al primer error

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass() { echo -e "${GREEN}✅ $1 PASSED${NC}"; }
fail() { echo -e "${RED}❌ $1 FAILED${NC}"; exit 1; }
step() { echo -e "\n${YELLOW}🔍 [$1] $2${NC}"; }

cd /workspace

# ---- Step 1: clang-format ----
step "1/7" "clang-format (code formatting)"
if find src include tests benchmarks \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format --dry-run --Werror 2>&1; then
  pass "clang-format"
else
  echo ""
  echo "  💡 Fix: find src include tests benchmarks \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0 | xargs -0 clang-format -i"
  fail "clang-format"
fi

# ---- Step 2: clang-tidy ----
step "2/7" "clang-tidy (static analysis)"
if run-clang-tidy -p build/Debug -quiet src/ 2>&1; then
  pass "clang-tidy"
else
  fail "clang-tidy"
fi

# ---- Step 3: cppcheck ----
step "3/7" "cppcheck (additional static analysis)"
if command -v cppcheck &> /dev/null; then
  if cppcheck --enable=all --suppress=missingIncludeSystem --error-exitcode=1 -I include src/ 2>&1; then
    pass "cppcheck"
  else
    fail "cppcheck"
  fi
else
  echo -e "${YELLOW}⚠️  cppcheck not installed, skipping. Install with: apt-get install -y cppcheck${NC}"
fi

# ---- Step 4: Build Debug ----
step "4/7" "CMake Build (Debug)"
if cmake --build build/Debug -j"$(nproc)" 2>&1; then
  pass "Build (Debug)"
else
  fail "Build (Debug)"
fi

# ---- Step 5: Unit Tests ----
step "5/7" "Unit Tests (ctest)"
cd build/Debug
if ctest --output-on-failure 2>&1; then
  pass "Unit Tests"
else
  fail "Unit Tests"
fi
cd /workspace

# ---- Step 6: Code Coverage ----
step "6/7" "Code Coverage (gcovr)"
if command -v gcovr &> /dev/null; then
  cd build/Debug
  find . -name '*.gcda' -delete
  ctest --output-on-failure > /dev/null 2>&1
  gcovr --root /workspace \
        --filter /workspace/src/ \
        --filter /workspace/include/ \
        --print-summary 2>&1
  pass "Code Coverage"
  cd /workspace
else
  echo -e "${YELLOW}⚠️  gcovr not installed, skipping. Install with: pip3 install gcovr${NC}"
fi

# ---- Step 7: Markdown Lint ----
step "7/7" "Markdown Lint (markdownlint)"
if npx -y markdownlint-cli "*.md" "docs/**/*.md" --config .markdownlint.json 2>&1; then
  pass "Markdown Lint"
else
  echo ""
  echo "  💡 Fix: npx -y markdownlint-cli --fix '*.md' 'docs/**/*.md' --config .markdownlint.json"
  fail "Markdown Lint"
fi

# ---- Summary ----
echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  🎉 PIPELINE COMPLETO — TODO VERDE  ${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
