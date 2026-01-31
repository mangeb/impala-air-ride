#!/bin/bash
# Converts the built React app into a C header for ESP32 PROGMEM
# Usage: ./build_header.sh
# Output: ../include/html_content.h

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DIST_DIR="$SCRIPT_DIR/dist"
OUTPUT="$SCRIPT_DIR/../include/html_content.h"

# Build the React app
echo "Building React app..."
cd "$SCRIPT_DIR"
npm run build

# Gzip it
echo "Compressing..."
gzip -c "$DIST_DIR/index.html" > "$DIST_DIR/index.html.gz"

RAW_SIZE=$(wc -c < "$DIST_DIR/index.html" | tr -d ' ')
GZ_SIZE=$(wc -c < "$DIST_DIR/index.html.gz" | tr -d ' ')

echo "Raw: ${RAW_SIZE} bytes, Gzipped: ${GZ_SIZE} bytes"

# Convert to C byte array
echo "Generating C header..."
cat > "$OUTPUT" << HEADER
#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

#include <Arduino.h>

// Auto-generated from React build - do not edit manually
// Raw size: ${RAW_SIZE} bytes
// Gzipped size: ${GZ_SIZE} bytes
// Build date: $(date -u +"%Y-%m-%d %H:%M:%S UTC")

const uint32_t HTML_CONTENT_SIZE = ${GZ_SIZE};

const uint8_t HTML_CONTENT[] PROGMEM = {
HEADER

# Convert binary to hex bytes
xxd -i < "$DIST_DIR/index.html.gz" >> "$OUTPUT"

cat >> "$OUTPUT" << FOOTER
};

#endif // HTML_CONTENT_H
FOOTER

echo "Generated: $OUTPUT (${GZ_SIZE} bytes)"
