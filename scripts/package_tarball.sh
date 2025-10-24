#!/bin/bash
# Package xlnpwmon binary distribution tarball
# This script packages the compiled binaries, libraries, headers, and source code

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Build directory (default to build, can be overridden)
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found: $BUILD_DIR${NC}"
    echo "Please build the project first or set BUILD_DIR environment variable"
    echo "Example: BUILD_DIR=./build $0"
    exit 1
fi

echo -e "${GREEN}Creating binary distribution tarball...${NC}"
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo ""

# Create distribution directory
DIST_DIR="$PROJECT_ROOT/xlnpwmon-zynqmp-binary"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

echo -e "${YELLOW}Copying source files...${NC}"
# Copy source files
cp -r "$PROJECT_ROOT/include" "$DIST_DIR/"
cp -r "$PROJECT_ROOT/src" "$DIST_DIR/"
cp -r "$PROJECT_ROOT/example" "$DIST_DIR/" 2>/dev/null || true
cp "$PROJECT_ROOT/README.md" "$DIST_DIR/"
cp "$PROJECT_ROOT/README_CN.md" "$DIST_DIR/" 2>/dev/null || true
cp "$PROJECT_ROOT/LICENSE" "$DIST_DIR/" 2>/dev/null || true
cp "$PROJECT_ROOT/INSTALL.txt" "$DIST_DIR/"
cp "$PROJECT_ROOT/scripts/install_from_tarball.sh" "$DIST_DIR/" 2>/dev/null || true

# Copy compiled binaries and libraries
mkdir -p "$DIST_DIR/bin"
mkdir -p "$DIST_DIR/lib"

echo -e "${YELLOW}Copying compiled binaries...${NC}"
# Copy CLI binary
if [ -f "$BUILD_DIR/xlnpwmon_cli" ]; then
    cp "$BUILD_DIR/xlnpwmon_cli" "$DIST_DIR/bin/"
    echo "  + xlnpwmon_cli"
else
    echo "  ! xlnpwmon_cli not found (skipping)"
fi

echo -e "${YELLOW}Copying libraries...${NC}"
# Copy libraries (both shared and static)
# C libraries
if [ -f "$BUILD_DIR/libxlnpwmon.so" ]; then
    cp "$BUILD_DIR/libxlnpwmon.so" "$DIST_DIR/lib/"
    echo "  + libxlnpwmon.so (C shared)"
fi

if [ -f "$BUILD_DIR/libxlnpwmon_static.a" ]; then
    cp "$BUILD_DIR/libxlnpwmon_static.a" "$DIST_DIR/lib/"
    echo "  + libxlnpwmon_static.a (C static)"
fi

# C++ libraries
if [ -f "$BUILD_DIR/libxlnpwmon_cpp.so" ]; then
    cp "$BUILD_DIR/libxlnpwmon_cpp.so" "$DIST_DIR/lib/"
    echo "  + libxlnpwmon_cpp.so (C++ shared)"
fi

if [ -f "$BUILD_DIR/libxlnpwmon_static_cpp.a" ]; then
    cp "$BUILD_DIR/libxlnpwmon_static_cpp.a" "$DIST_DIR/lib/"
    echo "  + libxlnpwmon_static_cpp.a (C++ static)"
fi

echo -e "${YELLOW}Copying pkg-config files...${NC}"
# Copy pkg-config files
mkdir -p "$DIST_DIR/lib/pkgconfig"
if [ -f "$BUILD_DIR/xlnpwmon.pc" ]; then
    cp "$BUILD_DIR/xlnpwmon.pc" "$DIST_DIR/lib/pkgconfig/"
    echo "  + xlnpwmon.pc"
fi

echo -e "${YELLOW}Copying CMake configuration files...${NC}"
# Copy CMake target files
mkdir -p "$DIST_DIR/lib/cmake/xlnpwmon"
if [ -d "$BUILD_DIR/cmake" ]; then
    cp -r "$BUILD_DIR/cmake/"* "$DIST_DIR/lib/cmake/xlnpwmon/" 2>/dev/null || true
    echo "  + CMake config files from build/cmake/"
fi

# Also check for CMake files in common locations
for cmake_file in xlnpwmonConfig.cmake xlnpwmonConfigVersion.cmake xlnpwmonTargets.cmake xlnpwmonTargets-release.cmake; do
    if [ -f "$BUILD_DIR/$cmake_file" ]; then
        cp "$BUILD_DIR/$cmake_file" "$DIST_DIR/lib/cmake/xlnpwmon/"
        echo "  + $cmake_file"
    fi
done

# Get version info
cd "$PROJECT_ROOT"
VERSION=$(git describe --tags --always 2>/dev/null || echo "dev")
TARBALL_NAME="xlnpwmon-${VERSION}-zynqmp-aarch64.tar.gz"

echo ""
echo -e "${YELLOW}Creating tarball: $TARBALL_NAME${NC}"
tar czf "$TARBALL_NAME" -C "$PROJECT_ROOT" "$(basename "$DIST_DIR")"

echo ""
echo -e "${GREEN}Tarball created successfully!${NC}"
ls -lh "$PROJECT_ROOT/$TARBALL_NAME"

echo ""
echo "Contents:"
tar tzf "$PROJECT_ROOT/$TARBALL_NAME" | head -20
echo "..."

echo ""
echo "To extract: tar xzf $TARBALL_NAME"
echo "To install: cd xlnpwmon-zynqmp-binary && sudo ./install_from_tarball.sh"
