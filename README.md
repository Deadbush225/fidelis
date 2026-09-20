# Fidelis Lexer Demo

This project is a small C lexer prototype for Fidelis syntax. It reads tokens from `samples/source_file.swift` and prints each token that the lexer recognizes.

## Requirements

- GCC or another C compiler that supports the C11 standard
- `make`

## Build

From the project root, run:

```bash
make
```

This produces a `fidelis` executable in the repository root.

## Run

The program always opens `samples/source_file.swift`, so keep that file in place before running:

```bash
./fidelis
```

You can also build and run in one step:

```bash
make run
```

## Clean

Remove the compiled binary with:

```bash
make clean
```

## What To Expect

When you run the program, it scans the input file and prints each recognized token in this format:

```text
Token Type: 00 | Lexeme: example
```

The current lexer recognizes:

- identifiers
- integer literals
- keywords such as `int`, `state`, `ensure`, and `transition`
- punctuation and operators such as `;`, `|>`, `->`, and `-=`

It also skips whitespace and the noise words `is`, `then`, `do`, and `to`.

## Notes

- The lexer currently ignores many characters that are not part of its token set, so the output is intentionally limited.
- `src/main.c` is hardcoded to open `samples/source_file.swift`, so changing the input file name requires updating the source.
