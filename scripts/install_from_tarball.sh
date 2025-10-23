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
