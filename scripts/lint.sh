#!/bin/bash
################################################################################
# IPTV PLAYER - LINT SCRIPT
# Static analysis using clang-tidy
################################################################################

set -e

# Change to project root directory
cd "$(dirname "$0")/.."

# Ensure compile_commands.json exists for clang-tidy
if [ ! -f "build/Release/compile_commands.json" ]; then
    echo -e "\033[1;33m⚠️  Warning: compile_commands.json not found.\033[0m"
    echo -e "Generating it via Conan and CMake..."
    
    # Run conan (without --output-folder to respect cmake_layout from conanfile)
    conan install . --build=missing > /dev/null
    
    # Run cmake explicitly without presets (Ubuntu 22.04 uses CMake 3.22, presets need >=3.23)
    cmake -B build/Release -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > /dev/null
fi

# Default targets if none provided
TARGETS="${@:-src tests}"

echo -e "\033[0;34m╔════════════════════════════════════════════════════════════════════╗\033[0m"
echo -e "\033[0;34m║                 IPTV PLAYER - STATIC ANALYSIS                      ║\033[0m"
echo -e "\033[0;34m╚════════════════════════════════════════════════════════════════════╝\033[0m"
echo ""

echo "🔍 Scanning targets: $TARGETS"
echo "Running clang-tidy... (this may take a moment)"

# Build file list
FILES=""
for TARGET in $TARGETS; do
    if [ -d "$TARGET" ]; then
        FILES="$FILES $(find $TARGET -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \))"
    elif [ -f "$TARGET" ]; then
        FILES="$FILES $TARGET"
    fi
done

if [ -z "$FILES" ]; then
    echo -e "\033[1;33m⚠️  No files found to analyze.\033[0m"
    exit 0
fi

# Run clang-tidy
clang-tidy $FILES -p build/Release

echo -e "\033[0;32m✅ Static analysis complete!\033[0m"
