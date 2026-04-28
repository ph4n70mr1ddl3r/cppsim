#!/bin/bash
# validate_clang_format.sh
# Validates that all project source files are properly formatted.
# Run from the project root or via: bash scripts/validate_clang_format.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLANG_FORMAT_CONFIG="${PROJECT_ROOT}/.clang-format"

echo "🔍 Validating clang-format for project source files..."

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo "❌ clang-format not found. Please install clang-format (version 10+)."
    exit 1
fi

# Verify configuration file exists
if [[ ! -f "${CLANG_FORMAT_CONFIG}" ]]; then
    echo "❌ .clang-format configuration file not found at ${CLANG_FORMAT_CONFIG}"
    exit 1
fi

CLANG_FORMAT_VERSION="$(clang-format --version)"
echo "   Using: ${CLANG_FORMAT_VERSION}"

# Collect all C++ source files (excluding build directories and vendored code)
MAPFILE -t SOURCE_FILES < <( \
  find "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/tests" \
    \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) \
    -not -path '*/build/*' \
    -not -path '*/_deps/*' \
    | sort
)

if [[ ${#SOURCE_FILES[@]} -eq 0 ]]; then
    echo "❌ No source files found in src/ or tests/"
    exit 1
fi

echo "   Checking ${#SOURCE_FILES[@]} source file(s)..."
echo ""

# Run clang-format in dry-run mode to detect formatting violations
# --dry-run / --Werror exits non-zero if any file needs changes
HAS_VIOLATIONS=false
NEEDS_FORMAT=()

for file in "${SOURCE_FILES[@]}"; do
  RELATIVE="${file#${PROJECT_ROOT}/}"
  if ! clang-format --dry-run --Werror --style=file "${file}" 2>/dev/null; then
    HAS_VIOLATIONS=true
    NEEDS_FORMAT+=("${RELATIVE}")
  fi
done

if [[ "${HAS_VIOLATIONS}" == "true" ]]; then
  echo "❌ The following files have formatting violations:"
  for f in "${NEEDS_FORMAT[@]}"; do
    echo "   - ${f}"
  done
  echo ""
  echo "   Run the following to fix:"
  echo "   find src tests \\( -name '*.cpp' -o -name '*.hpp' \\) -exec clang-format -i {} +"
  echo ""
  echo "⚠️  Note: clang-format cannot enforce snake_case naming conventions."
  echo "   Developers must manually follow architecture naming rules."
  exit 1
fi

echo "✅ All ${#SOURCE_FILES[@]} source files are properly formatted."
echo ""
echo "⚠️  Note: clang-format cannot enforce snake_case naming conventions."
echo "   Developers must manually follow architecture naming rules:"
echo "   - snake_case for all types, functions, variables"
echo "   - Trailing underscore for member variables (e.g., my_variable_)"
echo "   - Use code reviews or clang-tidy for naming enforcement."
