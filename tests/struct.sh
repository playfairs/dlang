#!/bin/sh
set -eu

compiler=$1
source=$2
output=$(mktemp "${TMPDIR:-/tmp}/dlang-struct.XXXXXX")
trap 'rm -f "$output"' EXIT

"$compiler" "$source" -o "$output"
set +e
"$output"
result=$?
set -e
[ "$result" -eq 12 ]
