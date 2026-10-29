# Fidelis Lexer

A complete C lexer implementing the full Fidelis programming language specification. The lexer uses a pure DFA-based design to scan source text and emit tokens for keywords, literals, operators, and comments.

## Architecture

- `src/` — lexer implementation
  - `main.c` — entry point that reads source files and prints tokens
  - `lexer.c` — DFA tokenization engine
- `include/` — public interfaces
  - `lexer.h` — token types, token structure, and declarations
- `samples/` — example inputs
  - `test_features.swift` — default demo source
  - `all_features.swift` — comprehensive feature showcase
  - `escape_test.swift` — escape-sequence examples
- `docs/lexer-design.md` — lexer state and design notes

## Quick Start

### Option 1: Use `make`

```bash
make
./fidelis
```

### Option 2: Unix shell script

```bash
chmod +x compile.sh
./compile.sh
```

### Option 3: Windows batch script

```cmd
compile.cmd
```

### Option 4: Compile directly with GCC

#### Windows (PowerShell)
```powershell
gcc -Iinclude src\main.c src\lexer.c src\helper.c -o fidelis.exe
.\fidelis.exe
```

#### macOS / Linux
```bash
gcc -Iinclude src/main.c src/lexer.c src/helper.c -o fidelis
./fidelis
```

## Platform-specific instructions

### Windows

1. Open PowerShell in the project folder.
2. Compile:

```powershell
gcc -Iinclude src\main.c src\lexer.c src\helper.c -o fidelis.exe
```

3. Run the default sample:

```powershell
.\fidelis.exe
```

4. Run a specific sample:

```powershell
.\fidelis.exe samples\test_features.swift
```

5. Or use the ready-made Windows helper script:

```cmd
compile.cmd
```

### macOS

1. Open Terminal in the project folder.
2. Compile:

```bash
gcc -Iinclude src/main.c src/lexer.c src/helper.c -o fidelis
```

3. Run the default sample:

```bash
./fidelis
```

4. Run a specific sample:

```bash
./fidelis samples/test_features.swift
```

5. Or run the Unix helper script:

```bash
chmod +x compile.sh
./compile.sh
```

### Linux

1. Open a shell in the project folder.
2. Compile:

```bash
gcc -Iinclude src/main.c src/lexer.c src/helper.c -o fidelis
```

3. Run the default sample:

```bash
./fidelis
```

4. Run a specific sample:

```bash
./fidelis samples/test_features.swift
```

5. Or run the Unix helper script:

```bash
chmod +x compile.sh
./compile.sh
```

## Requirements

- GCC or another C compiler supporting C11
- `make` if you want to use the `make` workflow
- A Unix shell for `compile.sh` or Windows Command Prompt for `compile.cmd`

## Build

```bash
make
```

This produces a `fidelis` binary in the repository root.

## Run

### Default input file

```bash
./fidelis
```

This reads `samples/test_features.swift`.

### Custom input file

```bash
./fidelis samples/test_features.swift
```

### Build and run in one command

```bash
make run
```

## Clean

```bash
make clean
```

## Complete Token Support

### Literals
- Integer: `42`, `1234`
- Float: `98.6`, `3.14`, `100.0`
- String: `"hello world"` (whitespace is preserved)
- Character: `'A'`, `'x'`, `'!'`, `'\n'`
- Boolean: `true`, `false`

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

### Noise Words (discarded automatically)
- `is`, `do`, `to`

These are recognized and filtered by the lexer; they do not appear in the token stream.

### Operators

#### Arithmetic
- `+`, `-`, `*`, `/`, `%`, `\`, `^`

#### Relational
- `==`, `!=`, `<`, `>`, `<=`, `>=`

#### Logical
- `!`, `&&`, `||`

#### Assignment
- `=`, `+=`, `-=`, `*=`, `/=`, `%=`

#### Special
- `|>` (pipeline)
- `->` (arrow)
- `?` (optional-type suffix)
- `.` (intrinsic property operator)

### Delimiters and brackets
- `;`, `,`, `:`
- `(`, `)`, `{`, `}`, `[`, `]`

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

Comments are returned as `LINE_COMMENT` and `BLOCK_COMMENT` tokens; the caller decides whether to display or ignore them.

## Output Format

The lexer prints each token in a fixed-width format:

```text
Token Type: FLOAT_LITERAL       | Lexeme: 98.6
Token Type: IDENTIFIER          | Lexeme: temperature
Token Type: ASSIGN              | Lexeme: =
Token Type: INT_LITERAL         | Lexeme: 42
```

## Samples

See `Fidelis.md` for the full language specification and examples.

To test the lexer on the bundled sample source:

```bash
./fidelis samples/test_features.swift
```

## Notes

- The lexer stores lexemes up to 255 characters before truncating
- Unrecognized characters produce `TOKEN_ERROR`
- Unterminated strings or block comments produce descriptive error tokens
- Whitespace is ignored as a separator
- The lexer follows a pure DFA architecture for efficient scanning

## Script files

The project includes helper scripts for compiling on each platform:

- `compile.sh` — Unix/macOS build helper
- `compile.cmd` — Windows build helper

The scripts compile the same set of source files as `make` and produce the `fidelis` binary or `fidelis.exe` output.
