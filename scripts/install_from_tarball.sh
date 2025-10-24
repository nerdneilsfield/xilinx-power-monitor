#!/bin/bash
# Installation script for xlnpwmon from tarball

set -e

# Default installation prefix
PREFIX="${PREFIX:-/usr}"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Error: This script must be run as root${NC}"
    echo "Please run with: sudo $0"
    exit 1
fi

# Get the script directory (should be in extracted tarball root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR"

echo -e "${GREEN}Installing xlnpwmon from tarball...${NC}"
echo "Installation prefix: $PREFIX"
echo "Source directory: $ROOT_DIR"
echo ""

# Check if required directories exist
if [ ! -d "$ROOT_DIR/lib" ] || [ ! -d "$ROOT_DIR/include" ]; then
    echo -e "${RED}Error: This doesn't appear to be a valid xlnpwmon tarball directory${NC}"
    echo "Required directories: lib/, include/"
    exit 1
fi

# Install libraries
echo -e "${YELLOW}Installing libraries...${NC}"
install -d "$PREFIX/lib"
if [ -d "$ROOT_DIR/lib" ]; then
    for lib in "$ROOT_DIR/lib"/*; do
        if [ -f "$lib" ]; then
            echo "  Installing $(basename "$lib")"
            install -m 644 "$lib" "$PREFIX/lib/"
        fi
    done
fi

# Install headers
echo -e "${YELLOW}Installing headers...${NC}"
install -d "$PREFIX/include"
if [ -d "$ROOT_DIR/include" ]; then
    cp -r "$ROOT_DIR/include"/* "$PREFIX/include/"
    echo "  Installed header files to $PREFIX/include/"
fi

# Install binaries
if [ -d "$ROOT_DIR/bin" ] && [ "$(ls -A "$ROOT_DIR/bin" 2>/dev/null)" ]; then
    echo -e "${YELLOW}Installing binaries...${NC}"
    install -d "$PREFIX/bin"
    for bin in "$ROOT_DIR/bin"/*; do
        if [ -f "$bin" ]; then
            echo "  Installing $(basename "$bin")"
            install -m 755 "$bin" "$PREFIX/bin/"
        fi
    done
fi

# Install pkg-config files
if [ -d "$ROOT_DIR/lib/pkgconfig" ] && [ "$(ls -A "$ROOT_DIR/lib/pkgconfig" 2>/dev/null)" ]; then
    echo -e "${YELLOW}Installing pkg-config files...${NC}"
    install -d "$PREFIX/lib/pkgconfig"
    for pc in "$ROOT_DIR/lib/pkgconfig"/*.pc; do
        if [ -f "$pc" ]; then
            echo "  Installing $(basename "$pc")"
            install -m 644 "$pc" "$PREFIX/lib/pkgconfig/"
        fi
    done
fi

# Install CMake configuration files
if [ -d "$ROOT_DIR/lib/cmake/xlnpwmon" ]; then
    echo -e "${YELLOW}Installing CMake configuration files...${NC}"
    install -d "$PREFIX/lib/cmake/xlnpwmon"
    cp -r "$ROOT_DIR/lib/cmake/xlnpwmon"/* "$PREFIX/lib/cmake/xlnpwmon/"
    echo "  Installed CMake config files to $PREFIX/lib/cmake/xlnpwmon/"
fi

# Create ncurses compatibility symlinks if needed
echo -e "${YELLOW}Checking ncurses compatibility...${NC}"
# Check if libncursesw.so.6 exists, if not, try to find and link it
if [ ! -e "$PREFIX/lib/libncursesw.so.6" ]; then
    # Look for libncursesw5 or other versions
    NCURSES_LIB=$(find "$PREFIX/lib" -name "libncursesw*.so.*" 2>/dev/null | head -1)
    if [ -n "$NCURSES_LIB" ]; then
        echo "  Creating symlink: libncursesw.so.6 -> $(basename $NCURSES_LIB)"
        ln -sf "$(basename $NCURSES_LIB)" "$PREFIX/lib/libncursesw.so.6"
        # Also create libncursesw.so if it doesn't exist
        if [ ! -e "$PREFIX/lib/libncursesw.so" ]; then
            ln -sf "$(basename $NCURSES_LIB)" "$PREFIX/lib/libncursesw.so"
        fi
    else
        echo -e "  ${YELLOW}Warning: ncursesw library not found. CLI tool may not work.${NC}"
        echo "  Please install ncurses-dev package or create symlinks manually."
    fi
else
    echo "  ncursesw.so.6 already exists"
fi

# Update library cache
echo -e "${YELLOW}Updating library cache...${NC}"
ldconfig

echo ""
echo -e "${GREEN}Installation completed successfully!${NC}"
echo ""
echo "Installed components:"
echo "  - Libraries: $PREFIX/lib/libxlnpwmon*"
echo "  - Headers: $PREFIX/include/"
if [ -d "$ROOT_DIR/bin" ] && [ "$(ls -A "$ROOT_DIR/bin" 2>/dev/null)" ]; then
    echo "  - Binaries: $PREFIX/bin/xlnpwmon_cli"
fi
echo ""
echo "To customize installation prefix, use: PREFIX=/custom/path sudo -E $0"
