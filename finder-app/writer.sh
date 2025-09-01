#!/bin/bash
set -euo pipefail

# Check for 2 arguments
if [ $# -ne 2 ]; then
    echo "Error: Two arguments required: writefile writestr" >&2
    exit 1
fi

writefile="$1"
writestr="$2"

# Create directory path if needed
dirpath=$(dirname "$writefile")
mkdir -p "$dirpath"

# Write string to file (overwrite if exists)
if ! echo "$writestr" > "$writefile"; then
    echo "Error: Could not create file $writefile" >&2
    exit 1
fi

