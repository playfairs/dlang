#!/bin/sh
set -eu

compiler=$1
source=$2

if "$compiler" "$source" --check 2> /tmp/dlang-diagnostics.err; then
  exit 1
fi
grep -q "expected '}'" /tmp/dlang-diagnostics.err
rm -f /tmp/dlang-diagnostics.err
