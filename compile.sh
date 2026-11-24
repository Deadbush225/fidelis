#!/usr/bin/env bash
set -eu

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
OUT="$ROOT_DIR/fidelis"

printf 'Compiling Fidelis...\n'

gcc \
  -I"$ROOT_DIR/include" \
  "$ROOT_DIR/src/main.c" \
  "$ROOT_DIR/src/lexer.c" \
  "$ROOT_DIR/src/helper.c" \
  "$ROOT_DIR/src/syntax.c" \
  "$ROOT_DIR/src/parser_core.c" \
  "$ROOT_DIR/src/semantic.c" \
  "$ROOT_DIR/src/interpreter.c" \
  -o "$OUT"

printf 'Build succeeded: %s\n' "$OUT"
printf 'Run with: ./fidelis <input.fdls>\n'
