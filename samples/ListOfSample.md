# Fidelis Samples

These examples use the `.fdls` extension accepted by the Fidelis lexer.

## About the Files

| File | Demonstrates | Run from repository root |
| --- | --- | --- |
| [`basic/hello-world.fdls`](basic/hello-world.fdls) | A minimal `print` statement | `.\fidelis.exe samples\basic\hello-world.fdls` |
| [`basic/bubble-sort.fdls`](basic/bubble-sort.fdls) | Nested `for` loops, array indexing, and swapping values | `.\fidelis.exe samples\basic\bubble-sort.fdls` |
| [`basic/search.fdls`](basic/search.fdls) | Linear search and binary search implemented as functions | `.\fidelis.exe samples\basic\search.fdls` |
| [`inventory_order_workflow.fdls`](inventory_order_workflow.fdls) | Inventory, stock adjustment, checkout flow, and lexer edge-case probes | `.\fidelis.exe samples\inventory_order_workflow.fdls` |

The current Fidelis executable performs lexical analysis and writes the token table to `outputs\symbol_table.txt`; these files are source examples for the language syntax.
