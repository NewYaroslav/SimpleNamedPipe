#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

CXX="${CXX:-g++}"
CXXFLAGS=(-std=c++17 -pthread)
INCLUDE_FLAGS=(-I"${ROOT_DIR}/tests/stubs" -I"${ROOT_DIR}/include" -include cstring)

if ! command -v "${CXX}" >/dev/null 2>&1; then
    echo "error: C++ compiler '${CXX}' not found" >&2
    exit 1
fi

BUILD_DIR="$(mktemp -d "odr-test-XXXXXX")"
cleanup() {
    rm -rf "${BUILD_DIR}"
}
trap cleanup EXIT

compile() {
    local source_file="$1"
    local output_file="$2"
    "${CXX}" "${CXXFLAGS[@]}" "${INCLUDE_FLAGS[@]}" -c "${source_file}" -o "${output_file}"
}

compile "${SCRIPT_DIR}/main1.cpp" "${BUILD_DIR}/main1.o"
compile "${SCRIPT_DIR}/main2.cpp" "${BUILD_DIR}/main2.o"
"${CXX}" "${CXXFLAGS[@]}" "${BUILD_DIR}/main1.o" "${BUILD_DIR}/main2.o" -o "${BUILD_DIR}/odr_test"

echo "ODR test built successfully: ${BUILD_DIR}/odr_test"
