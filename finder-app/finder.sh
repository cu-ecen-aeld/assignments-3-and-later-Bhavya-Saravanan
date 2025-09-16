#!/bin/sh
set -euo pipefail

# Check arguments
if [ $# -ne 2 ]; then
    echo "Error: Two arguments required: filesdir searchstr" >&2
    exit 1
fi

filesdir="$1"
searchstr="$2"

# Check if directory exists
if [ ! -d "$filesdir" ]; then
    echo "Error: $filesdir is not a valid directory" >&2
    exit 1
fi

# Count files
file_count=$(find "$filesdir" -type f | wc -l)

# Count matching lines
match_count=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $file_count and the number of matching lines are $match_count"

