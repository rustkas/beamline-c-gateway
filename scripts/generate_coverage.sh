#!/bin/bash
# Generate code coverage report for Gateway observability tests
# Requires: gcov, lcov (optional for HTML reports)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GATEWAY_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${GATEWAY_DIR}/build"
COVERAGE_DIR="${BUILD_DIR}/coverage_html"

cd "${GATEWAY_DIR}"

echo "=== Gateway Observability Code Coverage ==="
echo ""

# Check if coverage is enabled
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ] || ! grep -q "ENABLE_COVERAGE:BOOL=ON" "${BUILD_DIR}/CMakeCache.txt"; then
    echo "Building tests with coverage enabled..."
    make test-coverage
else
    echo "Running tests with coverage..."
    cd "${BUILD_DIR}"
    ./c-gateway-observability-test || true
    ./c-gateway-health-test || true
fi

echo ""
echo "Generating coverage data..."

# Generate gcov reports
cd "${BUILD_DIR}"
gcov -b -c *.gcda >/dev/null 2>&1 || echo "Warning: gcov reports may be incomplete"

# Check if lcov is available
if command -v lcov >/dev/null 2>&1; then
    echo "Generating HTML coverage report with lcov..."
    lcov --capture --directory . --output-file coverage.info --quiet --no-external
    
    # Generate HTML report
    if command -v genhtml >/dev/null 2>&1; then
        genhtml coverage.info --output-directory "${COVERAGE_DIR}" --quiet
        echo ""
        echo "✅ Coverage report generated: ${COVERAGE_DIR}/index.html"
        echo ""
        echo "Coverage summary:"
        lcov --summary coverage.info 2>/dev/null | grep -E "lines|functions|branches" || true
    else
        echo "Warning: genhtml not found. Install with: sudo apt-get install lcov"
        echo "Coverage data available in: ${BUILD_DIR}/coverage.info"
    fi
else
    echo "Warning: lcov not found. Install with: sudo apt-get install lcov"
    echo "Coverage data available in: ${BUILD_DIR}/*.gcov files"
fi

echo ""
echo "=== Coverage Report Complete ==="

