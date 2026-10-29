# Fidelis Samples

These examples use the `.fdls` extension accepted by the Fidelis lexer.

## About the Files

| File | Demonstrates | Run from repository root |
| --- | --- | --- |
| [`basic/hello-world.fdls`](basic/hello-world.fdls) | A minimal `print` statement | `.\fidelis.exe samples\basic\hello-world.fdls` |
| [`basic/bubble-sort.fdls`](basic/bubble-sort.fdls) | Nested `for` loops, array indexing, and swapping values | `.\fidelis.exe samples\basic\bubble-sort.fdls` |
| [`basic/search.fdls`](basic/search.fdls) | Linear search and binary search implemented as functions | `.\fidelis.exe samples\basic\search.fdls` |
| [`inventory_order_workflow.fdls`](inventory_order_workflow.fdls) | Sign-in, scalar inventory adjustment, checkout totals, reactive stock, and state transitions | `.\fidelis.exe samples\inventory_order_workflow.fdls` |
| [`reactive_cli_dashboard.fdls`](reactive_cli_dashboard.fdls) | Reactive server monitor CLI, fixed-size arrays, pipeline operator, and full token coverage | `.\fidelis.exe samples\reactive_cli_dashboard.fdls` |

The current Fidelis executable performs lexical analysis and writes the token table to `outputs\symbol_table.txt`; these files are source examples for the language syntax.
