#!/bin/bash
# IPTV PLAYER - CODE COVERAGE SCRIPT
# Generates a local HTML code coverage report using gcovr
################################################################################

set -e

# Change to project root directory
cd "$(dirname "$0")/.."

echo -e "\033[0;34m╔════════════════════════════════════════════════════════════════════╗\033[0m"
echo -e "\033[0;34m║                 IPTV PLAYER - CODE COVERAGE                        ║\033[0m"
echo -e "\033[0;34m╚════════════════════════════════════════════════════════════════════╝\033[0m"
echo ""

BUILD_DIR="build/coverage"

echo "🧹 Cleaning previous coverage data..."
rm -rf $BUILD_DIR

echo "📦 Resolving dependencies..."
conan install . --build=missing -s build_type=Debug > /dev/null

echo "🛠️ Configuring CMake with coverage flags..."
# CMake has built-in support for coverage flags with -fprofile-arcs -ftest-coverage
# But injecting them directly via CXX_FLAGS is simple and reliable
cmake -B $BUILD_DIR -DCMAKE_TOOLCHAIN_FILE=build/Debug/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage -g -O0" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
    -DCMAKE_SHARED_LINKER_FLAGS="--coverage" > /dev/null

echo "🔨 Building project..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)" > /dev/null

echo "🧪 Running unit tests..."
cd $BUILD_DIR
ctest --output-on-failure > /dev/null
cd ../..

echo "📊 Generating coverage report..."
mkdir -p $BUILD_DIR/reports

# Run gcovr
# -r . : Root directory
# -e build : Exclude build directory (except coverage data)
# -e tests : Exclude test files (we only want coverage of src)
# --html-details : Generate detailed HTML report
gcovr -r . \
      -e "build/.*" \
      -e "tests/.*" \
      -e "test/.*" \
      --html-details -o $BUILD_DIR/reports/coverage.html \
      --print-summary

echo -e "\n\033[0;32m✅ Coverage report generated successfully!\033[0m"
echo -e "Open \033[1;36m$BUILD_DIR/reports/coverage.html\033[0m in your browser to view the details."
