#!/bin/bash

# Usage: mergefile
# No arguments required (expects SPLITTED_TAR.tar)

PROGRAMNAME=$(basename "$0")

if [[ $# != 0 ]]; then
    echo "Usage: $PROGRAMNAME (no arguments)."
    exit 1
fi

if [[ ! -f "SPLITTED_TAR.tar" ]]; then
    echo "$PROGRAMNAME: SPLITTED_TAR.tar not found."
    exit 1
fi

# Extract archive
tar -xf "SPLITTED_TAR.tar"

# Read original filename and hash, better consistency
ORIGINAL_HASH=$(awk '{print $1}' SPLITTED_CHECK_SHA256SUM)
ORIGINAL_FILE=$(awk '{print $2}' SPLITTED_CHECK_SHA256SUM)

if [[ -z "$ORIGINAL_FILE" ]]; then
    echo "$PROGRAMNAME: Could not determine original filename."
    exit 1
fi

# Truncate/create output file safely
> "$ORIGINAL_FILE"

# Extract base and extension (file names)
BASE="${ORIGINAL_FILE%.*}"
EXT="${ORIGINAL_FILE##*.}"

# Collect only valid split files (no more infinite loops)
mapfile -t FILES < <(ls ${BASE}[a-z][a-z].${EXT} 2>/dev/null | sort)

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "$PROGRAMNAME: No split files found."
    exit 1
fi

# Reconstruct manually line by line
for FILE in "${FILES[@]}"; do
    dd if="$FILE" of="$ORIGINAL_FILE" bs=4096 conv=notrunc oflag=append status=none
done

# Verify SHA256, makes sure no corruption happened in the process
NEW_HASH=$(sha256sum "$ORIGINAL_FILE" | awk '{print $1}')

if [[ "$ORIGINAL_HASH" == "$NEW_HASH" ]]; then
    echo "SHASUM Checks!"
else
    echo "File corrupted (SHASUM doesn't check!)"
    exit 1
fi

echo "Done."

