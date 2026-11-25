# Fidelis Samples

These examples use the `.fdls` extension accepted by the Fidelis. The
commands below are intended to be run from the repository root in PowerShell.

## Feature demonstrations

| File | Demonstrates | Run |
| --- | --- | --- |
| [`basic/hello-world.fdls`](basic/hello-world.fdls) | The smallest valid program: a string literal and `print` | `.\fidelis.exe samples\basic\hello-world.fdls` |
| [`basic/arithmetic.fdls`](basic/arithmetic.fdls) | Operator precedence, unary minus, exponentiation, division, integer division, modulo, and compound assignment | `.\fidelis.exe samples\basic\arithmetic.fdls` |
| [`basic/bubble-sort.fdls`](basic/bubble-sort.fdls) | Nested `for` loops, arrays, indexing, comparisons, temporary values, and in-place swapping | `.\fidelis.exe samples\basic\bubble-sort.fdls` |
| [`basic/find-min-max.fdls`](basic/find-min-max.fdls) | Array `.length`, indexed traversal, `for`, and conditional minimum/maximum scans | `.\fidelis.exe samples\basic\find-min-max.fdls` |
| [`basic/search.fdls`](basic/search.fdls) | Functions, parameters, `return`, linear search, binary search, arrays, and loops | `.\fidelis.exe samples\basic\search.fdls` |
| [`test_features.fdls`](error/part-of-lexical-checking/test_features.fdls) | Comments, primitive types, literals, logical and relational operators, `if`/`else`, functions, states, transitions, loops, `match`, `ensure`, and noise words | `.\fidelis.exe samples\error\part-of-lexical-checking\test_features.fdls` |
| [`escape_test.fdls`](basic/escape_test.fdls) | Escaped string and character literals, including newline and tab sequences | `.\fidelis.exe samples\basic\escape_test.fdls` |
| [`all_features.fdls`](all_features.fdls) | Comprehensive interactive showcase: nullable values, arrays and mutations, reactive watchers, functions and recursion, states, transitions, loops, `match`, `ensure`, pipelines, I/O, escape sequences, all arithmetic/logical operators, compound assignments, and `sleep`/`quit` | `.\fidelis.exe samples\all_features.fdls` |
| [`inventory_order_workflow.fdls`](real-world/inventory_order_workflow.fdls) | A complete inventory workflow with sign-in, reactive stock, validation, clamping, checkout totals, discounts, tax, state transitions, and order updates | `.\fidelis.exe samples\real-world\inventory_order_workflow.fdls` |
| [`reactive_cli_dashboard.fdls`](real-world/reactive_cli_dashboard.fdls) | An interactive server monitor with reactive scalar state, nullable input, fixed-size arrays, pipelines, validation, scaling, deployment, shutdown, and command processing | `.\fidelis.exe samples\real-world\reactive_cli_dashboard.fdls` |

## Lexer and error fixtures

These files intentionally exercise tokenization boundaries or invalid input.
They are useful when checking `TOKEN_ERROR` reporting rather than program
execution.

| File | Purpose | Run |
| --- | --- | --- |
| [`all_token_list.fdls`](error/part-of-lexical-checking/all_token_list.fdls) | Token inventory covering comments, primitive types, literals, keywords, noise words, every operator, pipelines, arrows, optional markers, dots, and delimiters | `.\fidelis.exe samples\error\part-of-lexical-checking\all_token_list.fdls` |
| [`invalid-lexical-rules.fdls`](error/invalid-lexical-rules.fdls) | Invalid characters, malformed numbers and identifiers, overlong identifiers, unterminated strings/chars/comments, empty or oversized chars, and incomplete operators | `.\fidelis.exe samples\error\invalid-lexical-rules.fdls` |
| [`edge-cases/invalid-characters.fdls`](edge-cases/invalid-characters.fdls) | Invalid identifier symbols and an unsupported backtick-quoted string | `.\fidelis.exe samples\edge-cases\invalid-characters.fdls` |
| [`edge-cases/deep-nesting.fdls`](edge-cases/deep-nesting.fdls) | Nested states, conditionals, loops, grouped expressions, arrays, and stacked delimiters | `.\fidelis.exe samples\edge-cases\deep-nesting.fdls` |
| [`syntax-error.fdls`](error/syntax-error.fdls) | Missing syntax, such as an incomplete declaration and unclosed control-flow statement | `.\fidelis.exe samples\error\syntax-error.fdls` |

## Output

The current Fidelis executable runs lexical analysis, parsing, semantic
analysis, and interpretation. Normal runs write:

- `outputs\symbol_table.txt` - token table
- `outputs\parse_tree.txt` - parse tree
- `outputs\semantic_report.txt` - semantic diagnostics and symbol-table report

Use `.\fidelis.exe --check samples\basic\hello-world.fdls` to validate a sample
without executing it. Interactive samples may prompt for input during normal
execution.
