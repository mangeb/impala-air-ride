#!/bin/bash
# Full build: React UI → gzip → C header → PlatformIO compile
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HTML_DIR="$SCRIPT_DIR/html"

# Load nvm for node/npm
export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && . "$NVM_DIR/nvm.sh"

echo "==============================="
echo "  1/3  Building React UI"
echo "==============================="
cd "$HTML_DIR"
npm run build

echo ""
echo "==============================="
echo "  2/3  Generating C header"
echo "==============================="
bash "$HTML_DIR/build_header.sh"

echo ""
echo "==============================="
echo "  3/3  Compiling firmware"
echo "==============================="
cd "$SCRIPT_DIR"
python3 -m platformio run

echo ""
echo "==============================="
echo "  BUILD COMPLETE"
echo "==============================="
