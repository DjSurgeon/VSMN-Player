#!/bin/bash
################################################################################
# IPTV PLAYER - FORMAT SCRIPT
# Formats C++ code using clang-format
################################################################################

set -e

# Change to project root directory
cd "$(dirname "$0")/.."

# Default targets if none provided
TARGETS="${*:-src tests}"

echo -e "\033[0;34m╔════════════════════════════════════════════════════════════════════╗\033[0m"
echo -e "\033[0;34m║                 IPTV PLAYER - CODE FORMATTER                       ║\033[0m"
echo -e "\033[0;34m╚════════════════════════════════════════════════════════════════════╝\033[0m"
echo ""

echo "🔍 Scanning targets: $TARGETS"
echo "Formatting files..."

# Find all C++ files and run clang-format in-place
# shellcheck disable=SC2086
find $TARGETS -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print0 | xargs -0 clang-format -i

echo -e "\033[0;32m✅ Formatting complete!\033[0m"
