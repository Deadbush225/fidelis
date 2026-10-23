# Fidelis Lexer

A complete C lexer implementing the full Fidelis programming language specification. The lexer uses a pure Deterministic Finite Automaton (DFA) architecture with character-by-character evaluation to recognize and tokenize the entire Fidelis syntax.

## Architecture

The lexer is organized into three primary concerns:

- **`src/`** — Lexer implementation
  - `main.c` — Entry point that reads source files and prints tokens
  - `lexer.c` — Complete DFA-based tokenization engine
- **`include/`** — Public interfaces
  - `lexer.h` — Token types, Token struct, and function declarations
- **`samples/`** — Test input files
  - `test_features.swift` — Default input for the lexer demo
  - `all_features.swift` — Comprehensive feature showcase
  - `escape_test.swift` — Escape-sequence focused sample
- **`docs/lexer-design.md`** — DFA structure, tokenization behavior, and
  implementation limitations

## Requirements

- GCC or another C compiler supporting C11 standard or later
- `make`

## Build

```bash
make
```

This produces a `fidelis` executable in the repository root.

## Run

### Default input file
```bash
./fidelis
```
Reads and tokenizes `samples/test_features.swift`.

### Custom input file
```bash
./fidelis samples/test_features.swift
```
Reads and tokenizes any specified file.

### Build and run
```bash
make run
```

## Clean

```bash
make clean
```

## Complete Token Support

### Literals
- **Integer**: `42`, `1234`
- **Float**: `98.6`, `3.14`, `100.0`
- **String**: `"hello world"` (preserves whitespace; `\"` stays inside the literal)
- **Character**: `'A'`, `'x'`, `'!'`, `'\n'` (exactly one character or escape)
- **Boolean**: `true`, `false`

### Keywords

#### Data Types
- `int`, `float`, `string`, `bool`, `char`

#### Structural
- `state`, `react`, `loop`

#### Control Flow
- `if`, `else`, `for`, `match`, `transition`, `ensure`

#### I/O
- `print`, `readLine`, `readInt`

#### Modularity
- `fn`, `return`

#### System
- `quit`, `sleep`

#### Special Values
- `true`, `false`, `null`

### Noise Words (Automatically Discarded)
- `is`, `then`, `do`, `to`

These are recognized and filtered by the lexer; they do not appear in the token stream.

### Operators

#### Arithmetic
- `+` `PLUS`, `-` `MINUS`, `*` `STAR`, `/` `SLASH`, `%` `PERCENT`, `\` `BACKSLASH`, `^` `CARET`

#### Relational
- `==` `EQ`, `!=` `NE`, `<` `LT`, `>` `GT`, `<=` `LE`, `>=` `GE`

#### Logical
- `!` `NOT`, `&&` `AND`, `||` `OR`

#### Assignment
- `=` `ASSIGN`, `+=` `PLUS_ASSIGN`, `-=` `MINUS_ASSIGN`, `*=` `STAR_ASSIGN`, `/=` `SLASH_ASSIGN`, `%=` `PERCENT_ASSIGN`

#### Special
- `|>` `PIPELINE` (data flow operator)
- `->` `ARROW` (function return type / match routing)
- `?` `QUESTION` (optional-type suffix, as in `int?` and `string?`)

### Delimiters & Brackets
- Delimiters: `;` `SEMICOLON`, `,` `COMMA`, `:` `COLON`
- Brackets: `(` `LPAREN`, `)` `RPAREN`, `{` `LBRACE`, `}` `RBRACE`, `[` `LBRACKET`, `]` `RBRACKET`

### Comments

#### Single-line
```c
// This comment extends to end of line
```

#### Multi-line block
```c
/* This comment can span
   multiple lines and is
   terminated only by the closing */
```

Comments are returned as `LINE_COMMENT` and `BLOCK_COMMENT` tokens; the caller
decides whether to display or ignore them.

## Output Format

The lexer prints each token in a fixed-width format:

```
Token Type: FLOAT_LITERAL       | Lexeme: 98.6
Token Type: IDENTIFIER          | Lexeme: temperature
Token Type: ASSIGN              | Lexeme: =
Token Type: INT_LITERAL         | Lexeme: 42
```

The token type is padded to 20 characters for visual alignment.

## Implementation Details

### Pure DFA Architecture

The lexer uses a Deterministic Finite Automaton that processes the input character-by-character without using string comparison (`strcmp`). Each state represents a unique lexical position:

- **State branching** for multi-character sequences (e.g., `==`, `->`, `|>`, `&&`, `||`, keyword prefixes)
- **Keyword trie-like paths** where each branch represents a character in a keyword
- **Operator lookahead** to distinguish single-character operators from their multi-character variants
- **Iterative noise-word discarding** that restarts the scan loop, so long noise-word runs use constant stack

### Macro System

Three core macros keep the DFA implementation modular and maintainable:

- `BRANCH_OR_IDENTIFIER()` — Continue down a keyword path or fall back to identifier
- `YIELD_KEYWORD_OR_IDENTIFIER()` — Emit a keyword token or fall back to identifier if more letters follow
- `DISCARD_NOISE_OR_IDENTIFIER()` — Discard noise words by restarting the scan loop, or treat as identifier

For the state flow, token interface, and current implementation limitations,
see [Lexer Design](docs/lexer-design.md).

## Samples

### Full Language Example

See `Fidelis.md` for the complete Fidelis language specification with syntax examples, precedence rules, and detailed feature descriptions.

### Test File

Run the lexer on the comprehensive test file:

```bash
./fidelis samples/test_features.swift
```

This exercises comments, floats, strings, characters, all keywords, operators, and noise-word handling.

## Notes

- The lexer stores lexemes up to 255 characters; longer input is truncated rather than overflowing
- Unrecognized characters produce `TOKEN_ERROR` tokens
- Unterminated strings or block comments produce error tokens with a descriptive message
- All whitespace (spaces, tabs, newlines) is treated as token separators and ignored
- The lexer maintains the pure DFA design for efficiency and clarity
