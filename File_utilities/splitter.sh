#!/bin/bash

# Usage: splitter [FILENAME] [NUMBER_OF_PARTS]

PROGRAMNAME=$(basename "$0")

# Checks that is recieves both inputs
if [[ $# != 2 ]]; then
    echo "Usage: $PROGRAMNAME [FILENAME] [NUMBER_OF_PARTS]"
    exit 1
fi

FILENAME="$1"
PARTS="$2"

# Validates that the file exists
if [[ ! -f "$FILENAME" ]]; then
    echo "$PROGRAMNAME: Invalid filename"
    exit 1
fi

# Validate number of parts
if [[ ! "$PARTS" =~ ^[0-9]+$ || "$PARTS" -le 0 ]]; then
    echo "$PROGRAMNAME: NUMBER_OF_PARTS must be a positive integer"
    exit 1
fi

# Get total file size in bytes
FILESIZE=$(stat -c%s "$FILENAME")

if [[ "$FILESIZE" -eq 0 ]]; then
    echo "$PROGRAMNAME: File is empty"
    exit 1
fi

# Calculate bytes per part
BYTES_PER_PART=$(( (FILESIZE + PARTS - 1) / PARTS ))

# Extract filename without extension
BASE="${FILENAME%.*}"
EXT="${FILENAME##*.}"

# Generate checksum
sha256sum "$FILENAME" > SPLITTED_CHECK_SHA256SUM

# Split file by size in bytes
split -b "$BYTES_PER_PART" -a 2 "$FILENAME" temp_split_

# Rename files to baseaa.txt, baseab.txt, etc, AND also generates their HASH
COUNTER=0
for FILE in temp_split_*; do
    LETTER1=$(( COUNTER / 26 ))
    LETTER2=$(( COUNTER % 26 ))

    CHAR1=$(printf "\\$(printf '%03o' $((97 + LETTER1)))")
    CHAR2=$(printf "\\$(printf '%03o' $((97 + LETTER2)))")

    NEWNAME="${BASE}${CHAR1}${CHAR2}.${EXT}"
    mv "$FILE" "$NEWNAME"

    sha256sum "$NEWNAME" >> SPLITTED_CHECK_SHA256SUM

    ((COUNTER++))
done

# Create tar archive
tar -cf SPLITTED_TAR.tar ${BASE}??.${EXT} SPLITTED_CHECK_SHA256SUM

echo "File successfully split into $PARTS parts (by size)."
echo "Archive created: SPLITTED_TAR.tar"
