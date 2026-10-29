# Fidelis Lexer Design

## Purpose and scope

The lexer converts a Fidelis source stream into a sequence of tokens. It reads
one character at a time and classifies lexemes; it does not check whether the
resulting token sequence forms a valid program. Grammar checks, such as whether
an identifier is valid in a particular declaration, belong to a later parser
stage.

The implementation is in [`src/lexer.c`](../src/lexer.c). Public token types
and function declarations are in [`include/lexer.h`](../include/lexer.h). The
demo program in `src/main.c` repeatedly requests tokens and prints them.

## Token interface

`getNextToken(FILE *source)` returns one `Token` at a time. A token contains:

- `type`: a `TokenType` enum value.
- `lexeme`: a null-terminated character array with capacity for 255
  characters plus the terminator.

The token types are grouped in `lexer.h` as literals, keywords, operators,
delimiters, brackets, comments, and the special `TOKEN_EOF` and `TOKEN_ERROR`
types. `getTokenTypeName()` provides printable names, and `printToken()` writes
the type and lexeme to standard output.

The caller owns the input stream and is responsible for closing it. The caller
can continue calling `getNextToken()` until it receives `TOKEN_EOF`.

## Scanning model

Each call to `getNextToken()` starts in `STATE_START`, consumes characters
with `fgetc()`, and advances through `LexerState` values until it reaches
`STATE_DONE`. State data is local to that call; the `FILE *` position preserves
progress between calls.

At the start state, the lexer:

1. Skips whitespace.
2. Selects a scanning path for comments, numbers, strings, characters,
   operators, delimiters, brackets, or words.
3. Returns a token when the current lexeme is complete.

For tokens that need to inspect one character beyond their end, the lexer uses
`ungetc()` to return that character to the stream. The next call can then
process it as the start of the next token.

### Identifiers and keywords

Words beginning with a letter or underscore are scanned as identifiers or as
keywords. The keyword implementation is a trie-like DFA: the first character
selects a branch, and subsequent states match the rest of a keyword. If the
characters stop matching a keyword but remain part of a word, scanning falls
back to `STATE_IN_IDENTIFIER`.

Keyword states only emit a keyword when the next character is not a letter,
digit, or underscore. Therefore a longer word beginning with a keyword spelling
is treated as an identifier (for example, `integer` is not the `int` keyword
followed by `eger`).

The ordinary identifier continuation rule is letters, digits, and underscores.
The lexer recognizes tokens; it does not reject a whole string such as
`user-name` as an invalid identifier. It instead emits an identifier, a minus
operator, and another identifier, and `total sum` becomes two identifier
tokens. A word that starts with a digit, such as `1stPlace`, is consumed as a
whole and reported as `TOKEN_ERROR` instead of being split into a number and an
identifier. Identifiers longer than the 32-character limit are also reported as
`TOKEN_ERROR`.

The keyword DFA uses helper macros in `lexer.c`:

- `CASE_X_BRANCH_TO` follows a matching character to the next keyword state.
- `ELSE_CHECK_IF_POSSIBLE_IDENTIFIER` falls back to identifier handling when
  the current characters do not match a keyword branch.
- `CAN_WE_BRANCH_OR_IDENTIFIER` advances a keyword match or continues scanning
  the input as an identifier.
- `CONFIRM_KEYWORD_OR_IDENTIFIER` emits a completed keyword at a word boundary,
  or continues as an identifier if more identifier characters follow.
- `CONSUME_CHAR` appends one character while keeping the lexeme inside its
  buffer.

The noise words `is`, `do`, and `to` are returned as
`TOKEN_NOISE_WORD`. The lexer does not discard them; a caller that wants to
ignore noise words must filter that token type.

The keyword paths are case-sensitive. Most are lowercase; the input spellings
for the I/O keywords are `readLine` and `readInt`. `string` shares its `s`
entry with `state` and branches on the second character, so the two keywords do
not shadow one another.

### Numbers

An initial digit enters the integer state. Further digits extend the integer;
a period switches scanning to the float state. Digits after the period extend
the float. If a letter or underscore immediately follows the digits, the lexer
consumes the rest of that identifier-like sequence and emits one
`TOKEN_ERROR` (for example, `42items` is not split into an integer and an
identifier). Otherwise, the first character that does not belong to the number
is put back for the next token.

The current implementation does not require a digit after the decimal point
and does not validate numeric range. It also does not recognize a leading
decimal point as a float.

### Strings and characters

Double-quoted input is scanned as a string until the next unescaped double
quote. A backslash keeps the following byte inside the string, so `"a\"b"` is a
single `TOKEN_STRING_LITERAL`. Escapes are preserved verbatim in the lexeme;
the lexer does not decode them. An unescaped newline, carriage return, or
end-of-file before the closing quote produces `TOKEN_ERROR`. For an unterminated
string, the lexeme contains the source text scanned so far; it is not replaced
with a fixed "Unterminated string" diagnostic.

Single-quoted input is scanned as a character literal. A valid literal holds
exactly one character or one backslash escape, such as `'A'`, `'\n'`, or
`'\''`. An empty literal produces `TOKEN_ERROR` with the lexeme
"Empty char literal", more than one character produces `TOKEN_ERROR` with
"Char literal too long", and a missing closing quote produces `TOKEN_ERROR`
with "Unterminated char".

### Operators and punctuation

The start state directly emits single-character punctuation and simple
operators. For operator prefixes that may form multi-character operators, the
lexer reads one character of lookahead and selects the corresponding token.
Examples include:

| Input | Token |
| --- | --- |
| `+`, `+=` | `TOKEN_PLUS`, `TOKEN_PLUS_ASSIGN` |
| `-`, `-=`, `->` | `TOKEN_MINUS`, `TOKEN_MINUS_ASSIGN`, `TOKEN_ARROW` |
| `*`, `*=` | `TOKEN_STAR`, `TOKEN_STAR_ASSIGN` |
| `%`, `%=` | `TOKEN_PERCENT`, `TOKEN_PERCENT_ASSIGN` |
| `=`, `==` | `TOKEN_ASSIGN`, `TOKEN_EQ` |
| `!`, `!=` | `TOKEN_NOT`, `TOKEN_NE` |
| `<`, `<=`; `>`, `>=` | Relational token pairs |
| `&&`; `||`; `|>` | `TOKEN_AND`; `TOKEN_OR`; `TOKEN_PIPELINE` |
| `^` | `TOKEN_CARET` |
| `\` | `TOKEN_BACKSLASH` |
| `?` | `TOKEN_QUESTION` (optional-type suffix) |
| `.` | `TOKEN_DOT` (intrinsic property operator) |
The lexer also recognizes `/=` as `TOKEN_SLASH_ASSIGN` and accepts `\` and `^`
as standalone operators in addition to the punctuation and bracket tokens above.
A single `&` or a `|` not followed by `|` or `>` is an error token. Characters
that are not part of a recognized token, such as `@`, also produce
`TOKEN_ERROR`.

### Comments

`//` starts a line comment and `/*` starts a block comment. Block comments end
at the first `*/`; reaching end-of-file before that returns `TOKEN_ERROR`.
Line comments stop at the newline or end-of-file.

Comments are currently returned as `TOKEN_LINE_COMMENT` and
`TOKEN_BLOCK_COMMENT`. They are not discarded by `getNextToken()`; the caller
decides whether to print, preserve, or ignore them.

## Current limitations and maintenance notes

- `Token.lexeme` has a fixed capacity of 256 bytes. The scanner's
  `CONSUME_CHAR()` helper stops at the capacity limit, so over-long input is
  truncated instead of writing past the buffer. Truncation is silent for
  literals and comments; identifiers longer than 32 characters instead
  produce `TOKEN_ERROR` (their lexeme is still limited to 255 bytes).
- Tokens do not carry source line or column locations.
- `TOKEN_ERROR` lexemes are not a uniform diagnostic format: some error paths
  retain the scanned source text, while others use a fixed message. Errors do
  not include structured diagnostics or source locations, and recovery from a
  malformed construct is limited.
- Numeric literals are not validated: there is no exponent syntax, no range
  check, and a float may omit digits after the decimal point.
- The code is the source of truth for actual behavior. When language rules
  change, update the DFA, token definitions, examples, and tests together.

## Trying the lexer

Build with `make`, then pass a sample source file to the executable:

```sh
make
./fidelis samples/test_features.swift
```

The sample files demonstrate valid tokenization and selected lexical errors.
For example, unsupported characters produce `TOKEN_ERROR`; malformed
identifier-like text demonstrates token boundaries, not parser validation.
