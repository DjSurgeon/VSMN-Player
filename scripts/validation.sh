#!/bin/bash

################################################################################
# IPTV PLAYER - ENVIRONMENT VALIDATION SCRIPT
# Run INSIDE container to verify everything is ready for Week 1
#
# Usage: bash validation-script.sh
################################################################################

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║         IPTV PLAYER - ENVIRONMENT VALIDATION                       ║${NC}"
echo -e "${BLUE}║         Run this INSIDE container to validate setup                ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to test a command
test_command() {
    local name="$1"
    local command="$2"
    local min_version="$3"
    
    echo -n "Testing $name ... "
    
    if output=$(eval "$command" 2>&1); then
        echo -e "${GREEN}✅${NC}"
        echo "  $output" | sed 's/^/    /'
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}❌${NC}"
        echo "  Error: $output" | sed 's/^/    /'
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Function to test file/directory
test_path() {
    local name="$1"
    local path="$2"
    
    echo -n "Testing $name ... "
    
    if [ -e "$path" ]; then
        echo -e "${GREEN}✅${NC}"
        echo "  Path: $path"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}❌${NC}"
        echo "  Not found: $path"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# ════════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}1. COMPILER TOOLCHAIN${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_command "GCC" "gcc --version | head -n1"
test_command "G++" "g++ --version | head -n1"
test_command "CMake" "cmake --version | head -n1"
test_command "Ninja" "ninja --version"
test_command "Make" "make --version | head -n1"

echo ""

# ════════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}2. PACKAGE MANAGEMENT${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_command "Conan" "conan --version"
test_command "Conan Profile" "conan profile show | head -n5"
test_command "Python 3" "python3 --version"
test_command "Pip 3" "pip3 --version"

echo ""

# ════════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}3. DEVELOPMENT DEPENDENCIES${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo "Testing library availability..."

# Libraries check
libraries=(
    "libcurl:libcurl4-openssl-dev"
    "openssl:libssl-dev"
    "libavcodec:libavcodec-dev"
    "libavformat:libavformat-dev"
    "libavutil:libavutil-dev"
    "libswscale:libswscale-dev"
    "libswresample:libswresample-dev"
    "sdl2:libsdl2-dev"
    "gl:libgl1-mesa-dev"
)

for lib_pair in "${libraries[@]}"; do
    lib_name="${lib_pair%%:*}"
    lib_dev="${lib_pair##*:}"
    
    if pkg-config --exists "$lib_name" 2>/dev/null; then
        echo -e "  ${GREEN}✅${NC} $lib_name ($(pkg-config --modversion $lib_name))"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "  ${RED}❌${NC} $lib_name (missing)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
done

echo ""

# ════════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}4. DEVELOPMENT TOOLS${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_command "GDB (Debugger)" "gdb --version | head -n1"
test_command "Valgrind (Profiler)" "valgrind --version"
test_command "Clang-format (Formatter)" "clang-format --version"
test_command "Clang-tidy (Linter)" "clang-tidy --version | head -n1"

echo ""

# ════════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}5. WORKSPACE STRUCTURE${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_path "Workspace directory" "/workspace"
test_path "Conan cache" "/home/developer/.conan2"
test_path "Conan profile" "/home/developer/.conan2/profiles/default"
test_path "Git repository" "/workspace/.git"
test_path "CMakeLists.txt" "/workspace/CMakeLists.txt"
test_path "conanfile.py" "/workspace/conanfile.py"
test_path "Dockerfile.dev" "/workspace/Dockerfile.dev"
test_path "docker-compose.yml" "/workspace/docker-compose.yml"

echo ""

# ════════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}6. USER & PERMISSIONS${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo -n "Current user: "
whoami
echo -e "${GREEN}✅${NC}"
TESTS_PASSED=$((TESTS_PASSED + 1))

echo -n "User can write to workspace: "
if touch /workspace/.test_write 2>/dev/null; then
    rm -f /workspace/.test_write
    echo -e "${GREEN}✅${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}❌${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

echo -n "User can write to Conan cache: "
if touch /home/developer/.conan2/.test_write 2>/dev/null; then
    rm -f /home/developer/.conan2/.test_write
    echo -e "${GREEN}✅${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}❌${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

echo ""

# ════════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}7. QUICK BUILD TEST${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo "Attempting first Conan install..."

if cd /workspace && conan install . --output-folder=build 2>&1 | tail -10; then
    echo -e "${GREEN}✅ Conan install successful${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}❌ Conan install failed${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

echo ""

# ════════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}8. CMAKE BUILD TEST${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo "Attempting CMake configure..."

if cd /workspace && cmake -B build 2>&1 | tail -5; then
    echo -e "${GREEN}✅ CMake configure successful${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}❌ CMake configure failed${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

echo ""

echo "Attempting CMake build..."

if cd /workspace && cmake --build build 2>&1 | tail -5; then
    echo -e "${GREEN}✅ CMake build successful${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}❌ CMake build failed${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

echo ""

# ════════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}SUMMARY${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

TOTAL=$((TESTS_PASSED + TESTS_FAILED))

echo ""
echo -e "  Tests passed:  ${GREEN}$TESTS_PASSED${NC}/$TOTAL"
echo -e "  Tests failed:  ${RED}$TESTS_FAILED${NC}/$TOTAL"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ ALL TESTS PASSED - ENVIRONMENT IS READY FOR WEEK 1!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Exit container: exit"
    echo "  2. Read: EXECUTION_PLAN.md"
    echo "  3. Start Week 1 checklist"
    exit 0
else
    echo -e "${RED}❌ SOME TESTS FAILED - CHECK ERRORS ABOVE${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Review errors above"
    echo "  2. Check DOCKER_SETUP_FEDORA.md for troubleshooting"
    exit 1
fi