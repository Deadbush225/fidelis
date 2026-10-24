#!/usr/bin/env bash
set -eu

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
OUT="$ROOT_DIR/fidelis"

printf 'Compiling Fidelis...\n'

gcc \
  -std=c11 \
  -Wall \
  -Wextra \
  -pedantic \
  -O2 \
  -I"$ROOT_DIR/include" \
  "$ROOT_DIR/src/main.c" \
  "$ROOT_DIR/src/lexer.c" \
  "$ROOT_DIR/src/helper.c" \
  -o "$OUT"

printf 'Build succeeded: %s\n' "$OUT"
printf 'Run with: ./fidelis\n'
