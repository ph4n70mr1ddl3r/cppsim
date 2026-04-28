#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"
rm -rf build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
echo ""
echo "Running tests..."
cd build
ctest --output-on-failure
