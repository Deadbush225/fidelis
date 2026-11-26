# Fidelis Interpreter Design

## Purpose and scope

The interpreter executes a Fidelis parse tree after the command-line driver
has completed lexical, syntax, and semantic analysis successfully. It walks
the tree directly; it does not generate bytecode or machine code.

The implementation is in [`src/interpreter.c`](../src/interpreter.c), and its
runtime types and public entry point are in
[`include/interpreter.h`](../include/interpreter.h). The public API is
`executeProgram(sourceFilePath, root)`.

## Runtime model

### Values

`Value` is a tagged union representing integers, floats, strings, booleans,
characters, arrays, and null. Strings are heap allocated and copied when
cloned. Arrays are heap-allocated `ArrayObject`s with a count, capacity, and
reference count; cloning an array value increments that count. Releasing the
last reference frees its elements and storage.

The runtime's default-value helper maps nullable types to null, primitive types
to their zero/empty values, and arrays to element defaults. Semantic analysis
normally allows a source declaration without an initializer only for a
fixed-size array; those arrays are filled to their declared size.

### Environments

An `Env` stores variable bindings and has an optional parent. Lookup walks
from the current environment toward the global environment. Blocks and
branches may create child environments, and the interpreter releases their
values when those scopes exit.

User functions get a new environment whose parent is the global environment,
not the caller's current local environment. This is lexical lookup against
globals, not a closure over the caller's local variables.

### Registries and control flow

Before execution, the interpreter stores top-level function parse nodes, state
blocks, and reactive watcher blocks in registries. `FlowSignal` carries
non-local control:

- `FLOW_RETURN` passes a result back from a function.
- `FLOW_TRANSITION` requests a state change.
- `FLOW_QUIT` stops execution.
- `FLOW_NONE` means execution is continuing normally.

## Execution pipeline

`executeProgram()` initializes an `Interpreter` and performs these steps:

1. Create the global runtime environment.
2. Register top-level functions, states, and reactive watchers.
3. Execute top-level statements in source order. Function/state/react
   declarations themselves are skipped here because they were registered.
4. If execution did not fail or quit, enter the state selected by a top-level
   `transition`; otherwise enter `Boot` if declared, or the first declared
   state.
5. Run state blocks and follow `transition` signals until there is no next
   state, the program quits, or a runtime error occurs.
6. Release the return value, environments, and registries.

```text
validated ParseNode tree
          |
          v
executeProgram(source, root)
          |
          +--> register fn / state / react declarations
          +--> execute top-level statements
          +--> choose initial state (transition, Boot, or first state)
          +--> run state blocks and process transitions
          `--> cleanup; return false only on runtime error
```

If there are no states, the interpreter stops after top-level statements.
An explicit `quit` prevents automatic state entry.

## Expression evaluation

`evalExpression()` maps parse-tree nodes to runtime `Value`s. It handles
literals, identifiers, unary and binary expressions, ternary expressions,
function calls, pipelines, array literals and access, and `.length` property
access. It also handles `readLine()` and `readInt()` input expressions.

Binary operator chains are evaluated from left to right. Logical `&&` and `||`
short-circuit. Integer arithmetic remains integer arithmetic where
implemented; mixed or floating arithmetic produces a float. Division by zero,
invalid indexing, undefined names, and other runtime failures report a
location-aware runtime error and stop further execution.

String and character literal escape sequences are decoded by the interpreter,
not the lexer. Arrays are formatted recursively when converted to strings.

## Functions and built-ins

Function calls evaluate arguments left to right. For a user function, the
interpreter:

1. Looks up the registered function node.
2. Creates a function environment parented by the global environment.
3. Binds arguments to parameter names.
4. Executes the function block.
5. Captures `returnValue` when `FLOW_RETURN` is set.
6. Restores the caller environment and frees the function scope.

Built-in runtime functions include:

- `readLine()` for a line of input;
- `readInt()` for an integer input, returning null when input is invalid;
- `length()` for string or array length;
- `replace()` for string replacement.
- `print()` for output: one argument prints with a trailing newline; a second
  argument is printed as the suffix with no trailing newline.

Pipelines evaluate their left side once, then pass the current value as the
first argument to each function-call stage.

The public header sets limits of 256 nested function calls, a reactive watcher
nesting depth of 32, and 32 arguments.

## Assignments and reactive watchers

Variable declarations evaluate initializers and define bindings in the current
environment. Fixed-size array initializers may be padded with element defaults;
an initializer larger than the fixed size causes a runtime error.

Assignments resolve the owning environment, update a variable or array
element, and apply compound operators when requested. Assignments and updates
through `ensure` to a binding owned by the global environment trigger
registered `react(variable) { ... }` blocks for that name. Watchers execute
with the global environment as their current environment. The interpreter
limits nested watcher triggering to avoid unbounded recursion.

Additional statement behavior:

- `sleep(milliseconds)` pauses for the integer expression's duration.
- `ensure` unwraps a nullable value or executes its `else` block on null.
- `match` evaluates the subject once and executes the first matching case;
  `_` acts as the wildcard case.
- `quit` sets `FLOW_QUIT` and stops execution.

## Runtime diagnostics and cleanup

The first runtime error is printed to `stderr` with the source location and a
source snippet when available. It sets `hadRuntimeError`, which causes statement
execution and state processing to stop. `executeProgram()` returns false for a
runtime error and true otherwise.

At shutdown, the interpreter releases owned strings and arrays, the global
environment, function/state/react registries, and any unconsumed return value.
The parse tree is owned by the caller and is not freed by `executeProgram()`.

## Worked example: function call and output

Consider:

```fidelis
fn add(int left, int right): int {
    return left + right;
}

int result = add(3, 4);
print(result);
```

The interpreter processes it as follows:

1. **Register declarations:** `add` is saved in the function registry. Its
   declaration is not executed as a statement.
2. **Initialize the global:** the `result` initializer evaluates
   `add(3, 4)`.
3. **Enter the function:** a new function environment binds `left` to `3` and
   `right` to `4`. Its parent is the global environment.
4. **Evaluate and return:** `left + right` evaluates to integer `7`.
   `return` sets `FLOW_RETURN`; the call captures `7`, restores the caller's
   environment, and resumes top-level execution.
5. **Print:** `result` resolves to `7`, and the interpreter writes:

```text
7
```

No state declarations exist in this example, so the state-machine phase has
nothing to run.

## Current implementation notes

- Execution assumes the caller has already rejected lexer, parser, and
  semantic errors; `executeProgram()` itself requires a non-NULL tree.
- Only top-level function, state, and reactive declarations are registered by
  the initial pass.
- Function environments use the global scope as their parent rather than the
  caller's local environment.
- State entry defaults to `Boot`, then the first registered state, unless a
  top-level transition selected a state or execution quit.
- Runtime representation, tree-label handling, and behavior are defined by
  `src/interpreter.c` and `include/interpreter.h`.
