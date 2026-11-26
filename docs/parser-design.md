# Fidelis Parser Design

## Purpose and scope

The parser converts the lexer-generated symbol table into a syntax tree for a
Fidelis source program. It is a recursive-descent parser: one function checks
one grammar production, consumes matching tokens, and delegates to other
production functions.

The implementation is split across:

- `src/parser_core.c`: parser state, symbol-table input, token matching,
  diagnostics, panic recovery, and the public entry point.
- `src/syntax.c`: the Fidelis grammar productions and parse-tree construction.
- `include/syntax.h`: parser types, public parser functions, and helper
  declarations.
- `docs/grammar-rules.txt`: the EBNF reference for Fidelis syntax.

The lexer does not validate grammar structure. The parser consumes
`outputs/symbol_table.txt`, which is generated from the original source file,
and writes diagnostics and the parse tree to `outputs/parse_tree.txt`.

## Pipeline

The normal command-line flow is:

1. Read a `.fdls` source file.
2. Run the lexer and write tokens to `outputs/symbol_table.txt`.
3. Initialize `Parser` from that symbol table.
4. Parse `Program` and build a `ParseNode` tree.
5. Write syntax diagnostics and the parse tree to
   `outputs/parse_tree.txt`.
6. Close parser files and return the parse tree to the caller, which owns and
   frees it.

Lexical and parser output are not source re-parsing. The parser receives the
fixed-width symbol-table representation, including source line and column
fields.

### Overall flow

The parser consumes one token at a time from the symbol table. Grammar
functions inspect the current token, consume expected tokens, and call nested
grammar functions. A mismatch reports an error; selected productions invoke
recovery before parsing continues.

```text
source.fdls
    |
    v
lexer ----------------------> outputs/symbol_table.txt
                                      |
                                      v
parseSourceFile()
    |
    +--> initParser() ------> read first non-comment, non-noise token
    |
    +--> parseProgram()
    |       |
    |       +--> parseStatement()
    |       |       |
    |       |       +--> grammar productions consume tokens
    |       |       +--> accepted tokens build ParseNode children
    |       |       `--> errors may invoke panicRecovery()
    |       |
    |       `--> repeat until EOF
    |
    +--> printParseTree() --> outputs/parse_tree.txt
    `--> closeParser() -----> close files; caller owns the returned tree
```

### Worked parse: declaration with an additive expression

Consider this source:

```fidelis
int total = 1 + 2;
```

The lexer supplies tokens in order. The parser sees the token type and lexeme;
it does not re-read or re-tokenize the source:

```text
INT("int")
IDENTIFIER("total")
ASSIGNMENT("=")
INT_LITERAL("1")
ADDITION("+")
INT_LITERAL("2")
SEMICOLON(";")
EOF
```

The main production calls for this statement are:

```text
parseProgram
└── parseStatement
    ├── isTypeStart            sees INT
    └── parseDeclOrEnsureStmt
        ├── parseType          consumes INT
        ├── expect IDENTIFIER
        ├── expect ASSIGNMENT
        ├── parseExpression
        │   └── parseTernaryExpr
        │       └── parsePipelineExpr
        │           └── parseLogicalOrExpr
        │               └── ...
        │                   └── parseAdditiveExpr
        │                       ├── parseMultiplicativeExpr -> literal 1
        │                       ├── consume ADDITION
        │                       └── parseMultiplicativeExpr -> literal 2
        └── expect SEMICOLON
```

The omitted expression levels are still called; with no matching operator they
pass control down to the next tighter level. `parseAdditiveExpr()` consumes
`+` and wraps the expression's nodes in `AdditiveExpr`. The semicolon is
consumed after the expression as the statement terminator.

The resulting tree is represented approximately as:

```text
Program
└── Statement
    ├── SimpleStatement
    │   └── VarDecl
    │       ├── Type
    │       │   └── INT
    │       ├── IDENTIFIER
    │       │   └── total
    │       ├── ASSIGNMENT
    │       └── AdditiveExpr
    │           ├── INT_LITERAL
    │           │   └── 1
    │           ├── ADDITION
    │           └── INT_LITERAL
    │               └── 2
    └── SEMICOLON
```

This tree is illustrative of the current implementation: accepted tokens are
added as nodes by `advanceToken()`, and expression operators create grouping
nodes where the parser wraps operands. Non-operator expression levels do not
necessarily create a node of their own.

## Public API

`parseSourceFile(sourceFilePath, symbolTablePath, outputPath, outRoot,
nonInteractive)` is the high-level entry point. It initializes the parser,
calls `parseProgram()`, prints the parse tree when an output file is configured,
closes the parser, and returns whether parsing completed without syntax errors.
When `outRoot` is non-NULL, ownership of the tree is transferred to the caller;
the caller must eventually call `freeParseTree()`. If the tree is not
transferred, `closeParser()` frees it.

The lower-level API is:

- `initParser()` opens the symbol table and output files and loads the first
  token.
- `advanceToken()` consumes the current token and loads the next token.
- `checkToken()` and `checkTokenAny()` inspect the current token without
  consuming it.
- `matchToken()` consumes a token only when it matches.
- `parseProgram()` parses the complete token stream.
- `closeParser()` closes files and frees the tree only if the parser still owns
  it.
- `freeParseTree()` releases a tree returned through `outRoot`.

## Parser state

`Parser` contains:

- the symbol-table input stream and parse-output stream;
- the original source path used for diagnostics;
- the current `SymbolEntry`;
- whether another token is available;
- parser error count and panic-recovery state;
- the optional default recovery choice;
- the parse-tree root and current node;
- a tree-suppression flag used while recovering.

The parser is initialized with the root node labeled `Program`. Parsing moves
through child nodes under this root.

## Symbol-table input

`readNextEntry()` reads rows produced by `main.c` from the symbol-table file.
Each row contains:

```text
LINE | COLUMN | TOKEN TYPE | LEXEME | STATE TRANSITIONS
```

The reader:

- skips headers, divider lines, blank lines, and wrapped continuation rows;
- parses line, column, token type, lexeme, and transition text;
- ignores `LINE_COMMENT`, `BLOCK_COMMENT`, and `NOISE_WORD` entries;
- comments remain in lexer output only, and noise words are discarded before
  grammar productions receive tokens;
- reports EOF when no more usable entries remain.

Lexemes that contain `|` are handled by locating the transition separator
associated with `STATE_` text rather than splitting on every pipe.

## Token consumption and parse-tree nodes

`checkToken()` peeks at the current token. `matchToken()` consumes it and
creates a corresponding parse-tree child only after the token is accepted.

Consumed valued tokens add a token node containing the token type and, for
identifiers and literal types, the source lexeme:

```text
IDENTIFIER
└── example
```

`EOF` and `NOISE_WORD` do not appear in the parse tree. In the current input
path, `readNextEntry()` filters noise words before `syntax.c` receives them.
Consequently, although `syntax.c` has optional noise-word matching helpers,
they do not see `is`, `do`, or `to` when parsing the normal lexer symbol table;
those words are effectively ignored before parsing.

Grammar functions create named nodes, such as `Statement`, `VarDecl`,
`IfStmt`, `FunctionCall`, or `TernaryExpr`. `wrapChildrenFrom()` moves a range
of sibling nodes under a newly added expression wrapper.

## Grammar structure

The top-level shape is:

```ebnf
Program = { Statement } ;
Statement = ";" | BlockStatement | ( SimpleStatement, ";" ) ;
```

This is a simplified summary of dispatch, not the complete grammar: typed
declarations have their own semicolon handling, and `ensure` declarations are
self-delimited by their `else` block.

Block statements are self-delimited and include:

- `state`, `react`, and `fn` declarations;
- `if`, `loop`, `for`, and `match` statements;
- ensure declarations, which are handled as a block statement.

Simple statements include variable declarations, assignments, print calls,
transitions, return statements, sleep calls, function calls, expressions, and
the `quit` keyword.

Type parsing supports scalar, nullable, nested-array, and optional fixed-size
array types:

```ebnf
Type = ("int" | "float" | "string" | "bool" | "char"),
       ["?"]
     | "[", Type, [",", INT_LITERAL], "]", ["?"] ;
```

The parser accepts initialized declarations and declarations without an
initializer. The semantic analyzer subsequently requires an initializer unless
the declaration is a fixed-size array. The source-level forms include:

```ebnf
VarDecl = Type, Identifier, ";"
        | Type, Identifier, ["is"], "=", Expression, ";";
EnsureStmt = Type, Identifier, ["is"], "=", "ensure",
             Expression, "else", Block
           | Identifier, "=", "ensure", Expression, "else", Block;
```

The parser input reader currently removes noise-word tokens, so the optional
`is` spelling is not actually consumed by a grammar production.

Fixed-size arrays use a comma in the type brackets:

```ebnf
Type = "[", Type, [",", INT_LITERAL], "]", ["?"] ;
```

For example, `[int, 10] values;` declares an array with a fixed capacity of
10. The declaration form without `=` is accepted by the parser; the semantic
analyzer requires an initializer for every other declaration.

## Statement dispatch

At the start of a statement, the parser:

1. Consumes an empty semicolon as an empty statement.
2. Dispatches a type start to declarations or ensure statements.
3. Dispatches block keywords with `IF`, `LOOP`, `FOR`, `MATCH`, `STATE`,
   `REACT`, or `FN`.
4. Otherwise parses a simple statement.

Simple statements dispatch by keyword or token shape:

- `print` calls;
- `transition` statements;
- `return` statements;
- `sleep` calls;
- identifiers, for assignments and function calls;
- literal/parenthesized expressions;
- `quit`;

Declarations, assignments, and expression parsing are in `syntax.c`.

## Expression precedence

Expression parsing delegates through this strongest-to-weakest hierarchy:

1. Primary expressions
2. Exponentiation
3. Unary `!` and `-`
4. Multiplication, division, integer division, and modulo
5. Addition and subtraction
6. Relational operators
7. Equality
8. Logical AND
9. Logical OR
10. Pipeline
11. Ternary

Left-associative levels repeat their operand and use the `LEFT_ASSOC` helper.
Unary and exponentiation are right-associative. Ternary expressions are parsed
with a right-associated second branch.

Primary expressions support:

- literals;
- parenthesized expressions;
- array literals;
- `readLine()` and `readInt()` built-ins;
- identifiers;
- identifier function calls;
- identifier array access;
- property access with `.`.

Pipeline expressions accept an identifier or the `print` keyword after `|>`,
require a parenthesized argument list, and add `FunctionCall` nodes.
Both the semantic analyzer and interpreter resolve `print` as a pipeline stage;
the piped value is inserted as the first argument. `print` accepts one value
followed by newline, or a value plus an explicit end/separator string.

## Control-flow productions

`if` supports a single statement or block as its body and an optional `else`,
including `else if` recursion.

`loop` accepts optional `do` and requires a block.

`for` requires:

```ebnf
"for", "(", Type, Identifier, "=", Expression, ";",
Expression, ";", Assignment, ")", ["do"], Block ;
```

`match` supports either an identifier or parenthesized expression. It accepts
optional `do`, requires a block, and requires at least one case. Case patterns
may be identifiers or literals; numeric patterns may have a leading `+` or `-`.
Actions may be blocks or simple statements followed by semicolons.

`ensure` parses as a declaration with `ensure`, an expression, `else`, and a
block. Unlike an ordinary variable declaration, it is not terminated by a
separate semicolon in the block-statement production.

## Error reporting

`reportError()`:

- sets `encounteredError`;
- increments `errorCount`;
- prints an annotated error box to stderr;
- writes the same diagnostic to the parse-output file;
- shows source file, line, and column;
- shows symbol-table line number;
- prints the current token type and lexeme;
- prints a source snippet when available.

No source locations are synthesized by the parser. It relies on line and column
fields parsed from the symbol table.

## Panic-mode recovery

When a required token or production fails, the parser may call
`panicRecovery()`. It suppresses parse-tree additions during recovery and
offers:

- **Continue**: skip tokens until a caller-provided synchronization token;
- **Skip statement**: skip to a semicolon or block delimiter;
- **Abort**: stop parsing.

Uppercase choices become the default for the remainder of the current parse;
lowercase choices apply once.

The top-level recovery synchronization set is `SEMICOLON`, `RBRACE`, and
`EOF`. For declarations and statement termination, the parser uses production-
specific synchronization sets. The top-level loop advances past a stray
semicolon or right brace if recovery stops on one, preventing a recovery loop.

Recovery prompts interactively unless `nonInteractive` is enabled (as in
`--check` mode), where it synchronizes automatically using the continue
behavior. When a parse-output stream is open, recovery progress is also written
there. Recovery is intentionally limited: it cannot prove that skipped source
text would have formed valid Fidelis syntax.

## Parse-tree output

After `parseProgram()` completes, `parseSourceFile()` prints the tree recursively
to the output file. The tree contains grammar labels and selected token nodes,
not symbol-table transition text.

During normal parsing, `advanceToken()` attaches valued token nodes to the
current grammar node. Parsing failures may suppress further node additions
until recovery finishes.

`closeParser()` closes both files and frees the complete parse tree only if it
was not transferred to the caller through `outRoot`.

## Current implementation notes

- The parser requires semicolons for ordinary simple statements.
- The parser accepts comments in the symbol table but skips them.
- The symbol-table reader skips `NOISE_WORD` along with comments. The
  noise-word helpers in `syntax.c` therefore do not receive those tokens in
  normal parsing.
- Match case patterns include identifiers and literals; array and signed pattern
  handling follows `parseMatchStmt()` in `syntax.c`.
- `match` accepts an identifier or a parenthesized expression as its subject.
  A case may contain literals, identifiers (including `_`), or signed numeric
  patterns.
- Pipeline calls accept identifier names or `print`, followed by argument
  parentheses. The piped value is available as the pipeline stage's first
  argument.
- The parser does not perform semantic validation such as type compatibility,
  undeclared variable checks, or duplicate declaration checks.
- Diagnostics depend on the lexer symbol-table format remaining stable.
- `docs/grammar-rules.txt` is the EBNF grammar reference; `src/syntax.c` and
  `include/syntax.h` are the source of truth for implementation details.
