# Fidelis Semantic Analysis Design

## Purpose and scope

Semantic analysis checks whether a syntactically valid Fidelis parse tree is
meaningful. It resolves names, checks types and declarations, verifies
context-sensitive rules such as returns and state transitions, and writes a
symbol-table report when an output path is supplied. It does not execute the
program or produce machine code.

The implementation is in [`src/semantic.c`](../src/semantic.c), and its public
types and entry points are in [`include/semantic.h`](../include/semantic.h).
It consumes the `ParseNode` tree built by the parser; it does not read the
source file or lexer symbol table directly.

## Pipeline

The command-line driver runs semantic analysis only after parsing succeeds:

1. The parser returns a `ParseNode` tree.
2. `analyzeSemantics()` creates the analyzer state and global scope.
3. Built-in function symbols are registered.
4. Top-level function signatures and state names are hoisted.
5. The analyzer walks top-level statements and checks declarations,
   expressions, scopes, and control flow.
6. If configured, it writes the scoped symbol table and summary, then frees
   analyzer-owned scopes and closes the report.

```text
ParseNode tree
      |
      v
analyzeSemantics(source, root, reportPath)
      |
      +--> create global scope
      +--> register built-in functions
      +--> hoist top-level fn signatures and state names
      +--> analyze statements in source order
      |       +--> enter/leave lexical scopes
      |       +--> infer expression types
      |       `--> report errors and warnings
      +--> write scoped symbol table (if reportPath is non-NULL)
      `--> cleanup scopes and return (errorCount == 0)
```

The function and state declaration pass lets function calls and transition
targets resolve before their textual declarations. Variable declarations are
introduced during the later statement walk, not hoisted.

## Public API

```c
bool analyzeSemantics(const char *sourceFilePath,
                      const ParseNode *root,
                      const char *outputPath);
```

- `sourceFilePath` is used to show source snippets in diagnostics.
- `root` must be a non-NULL parse tree.
- `outputPath` may be NULL. Diagnostics still go to `stderr`; the detailed
  report and symbol table are written only when this path is non-NULL.
- The return value is true when no semantic errors were counted. Warnings do
  not make the result fail. A NULL root or a report-file open failure also
  returns false.

`formatSemType()` converts the internal `SemType` representation into a
human-readable Fidelis type string.

## Types

`SemType` represents a base type plus array and nullability information:

- `base`: primitive types (`int`, `float`, `string`, `bool`, `char`) and
  internal/special types (`void`, `null`, empty array, wildcard, error).
- `arrayDepth`: zero for scalars, increasing for nested arrays.
- `fixedSize`: `-1` for dynamic sizing or a positive fixed-array length.
- `isNullable`: whether the outer value can be null.
- `elemNullable`: whether array elements can be null.

For example, `int?` has nullable scalar value semantics, `[int]?` is a nullable
array, and `[int?]` is an array whose elements may be null. Fixed-size array
lengths must be positive. Empty array literals have a special type that can be
assigned to an array type.

The assignment rules include:

- A non-nullable value can be assigned to the corresponding nullable type; the
  reverse requires an `ensure`.
- `null` is accepted for nullable targets.
- `int` widens to `float`.
- Array depths must match, and nullable array elements cannot be assigned to a
  non-nullable element type.
- The internal `TYPE_ERROR` suppresses cascading diagnostics after an earlier
  failure; `TYPE_ANY` is used for generic built-in signatures.

## Scopes and symbols

`Scope` objects form a parent-linked lexical chain. Name lookup checks the
current scope and then each parent. A symbol records its name, kind, type,
source location, and function parameter types. Redeclaring a name in the same
scope is an error; shadowing is possible in nested scopes.

The analyzer retains all scopes until report generation, even after leaving
them, so the report can show symbols from function, state, branch, loop, and
react scopes.

Top-level function and state declarations are hoisted into the global scope.
Function parameters are defined in the function scope. Blocks and control-flow
constructs create scopes according to their analysis routines; a `for` loop
has a scope containing its loop variable.

## Built-in functions

The analyzer registers these built-ins in the global scope. `print` is a
special built-in handled by call validation rather than a normal function
symbol:

| Name | Signature | Special checks |
| --- | --- | --- |
| `readLine` | `() -> string` | No arguments |
| `readInt` | `() -> int?` | No arguments |
| `length` | `(any) -> int` | Requires a non-nullable string or array |
| `replace` | `(string, string, string) -> string` | Arguments must be strings |
| `print` | `(value[, end]) -> void` | One or two arguments; the value must be printable |

Built-in symbols are available for lookup but omitted from the printed
user-symbol table.

`print` is handled specially rather than registered as a normal function symbol,
so it can accept an optional second argument that acts as the output suffix.
It is also accepted as a pipeline stage: the piped value becomes the first
argument.

## Analysis behavior

During the statement walk, each declaration or statement has a dedicated
analysis routine. Expression analysis returns a `SemType` and checks:

- identifier existence and use as a value;
- operator operand compatibility and result type;
- function existence, arity, argument types, and pipeline input;
- array indexing, array literals, and property access;
- assignment and initializer compatibility;
- boolean conditions for `if` and `for`;
- valid `return` types and whether functions return on all recognized paths;
- existence of transition target states and context restrictions;
- variable-only `react` watchers.

The analyzer also checks `ensure` expressions and their `else` blocks. An
`else` block must exit the current path using `transition`, `return`, or
`quit`. Warnings are emitted for redundant or nullable `ensure` usage and
match cases after a wildcard.

Diagnostics include severity, source path, line and column, and a source
snippet when the location is available. Errors and warnings are sent to
`stderr`; when a report file is open, they are copied there as well. The report
ends with the scoped symbol table and error/warning totals.

## Worked example: checking a function call

Consider:

```fidelis
fn add(int left, int right): int {
    return left + right;
}

int result = add(3, 4);
print(result);
```

### Pass 1: hoist declarations

Before checking statements, the analyzer records the `add` signature in the
global scope:

```text
global
└── add : (int, int) -> int
```

This signature is available when the initializer is checked.

### Pass 2: analyze the function and statements

When it reaches `add`, the analyzer opens a function scope and defines
`left: int` and `right: int`. The `+` expression has type `int`; that type is
assignable to the declared return type. The function returns on all recognized
paths, so no return diagnostic is emitted.

For `int result = add(3, 4);`, it analyzes each integer argument, looks up the
hoisted function signature, checks the argument count and types, and gets
`int` as the call's result type. That result is assignable to `result`.
Finally, `print(result)` resolves `result` and accepts its non-void value.

With an output path, the relevant report entries are approximately:

```text
SCOPE                  | NAME                   | KIND       | TYPE                 | LINE
global                 | add                    | FUNCTION   | (2 args) -> int      | 1
fn:add                 | left                   | PARAMETER  | int                  | 1
fn:add                 | right                  | PARAMETER  | int                  | 1
global                 | result                 | VARIABLE   | int                  | 5
```

The analysis returns true if the complete walk found no semantic errors.
Execution is a later phase and is not performed by this example's analyzer
pass.

## Current implementation notes

- The first pass hoists top-level functions and states, not variables or
  reactive declarations.
- The second pass analyzes statements in source order and retains scopes for
  report output.
- `analyzeSemantics()` accepts an optional report path; without one, errors and
  warnings are still displayed on `stderr`.
- The semantic analyzer depends on the parse-tree labels and child layout
  produced by `syntax.c`.
- The implementation in `src/semantic.c` and public declarations in
  `include/semantic.h` are the source of truth for current behavior.
