# Fidelis

## Introduction

Fidelis is a programming language designed for reliability in command-line applications while also incorporating reactivity to variable changes similar to frontend frameworks such as Svelte and Vue. Fidelis prevents runtime errors by validating null values similar to TypeScript and Swift. By treating applications as state-driven rather than purely procedural, Fidelis prevents the runtime crashes traditionally caused by invalid user input. It forces developers to validate unpredictable standard input at the language level through safe extractors which effectively eliminates the need for continuous polling loops and manual type checking.

Rather than relying on synchronous execution, the language employs an architecture where localized logic blocks automatically re-evaluate in response to state changes. To support this, the syntax adopts the streamlined data manipulation techniques found in Ruby and Kotlin. This combination combines the flexibility of modern scripting languages while also enforcing the built-in validation that are needed for stable and reliable programs. This separation of state from the procedural flow enables developers to build reactive terminal interfaces that update in real-time without blocking the main execution thread. This shift in paradigm means that as data flows in, be it from standard input, system events, or state changes, the relevant UI and logic blocks recalculates and renders smoothly. In the end, Fidelis bridges the divide between strong type safety and the graceful, reactive developer experience usually exclusive to modern web design.

---

## Syntactic Elements of Language

### Character Set

Let:
$\Sigma = ALPHA \cup DIGIT \cup SYMBOL \cup WHITESPACE \cup UNDERSCORE$

Where:

* $ALPHA = (ALPHA_{lower} \cup ALPHA_{upper})$

* $ALPHA_{lower} = \{a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z\}$

* $ALPHA_{upper} = \{A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z\}$

* $DIGIT = \{0, 1, 2, 3, 4, 5, 6, 7, 8, 9\}$

* $SYMBOL = \{(, ), \{, \}, [, ], \dots, !, =, +, -, *, /, \backslash, \%, :, \dots, \vert{}, \&, ?, \dots, ;, <, >, \wedge\}$

* $WHITESPACE = \{space, tab, newline\}$

* $UNDERSCORE = \{\_\}$


### Identifiers

Identifiers are names used for variables, functions, and tasks. For this proposed language, it is to be defined and parsed using the following regular expression:

`((ALPHA | UNDERSCORE)+(ALPHA | DIGIT | UNDERSCORE)*) 32`

This rule dictates that an identifier must:

1. Begin with a letter (A-Z, a-z) or an underscore (_).


2. The initial character may be followed by any combination of letters, digits (0-9), or underscores.


3. Spaces and other symbols are not allowed.


4. The maximum allowed length is 32 characters.


5. Identifiers are case-sensitive; the compiler treats uppercase and lowercase letters as distinct characters.



**Lexical Evaluation Example:**

| Identifier | Parser Evaluation | Reason |
| --- | --- | --- 
| `studentAge` | Valid | Starts with a letter, containing only letters. |
| `_score_1` | Valid | Starts with an underscore, followed by letters and digits. |
| `MAX_VALUE` | Valid | Starts with a letter, contains only uppercase letters and underscores. |
| `1stPlace` | Invalid | Violates the regex `^[a-zA-Z]` by starting with a digit. |
| `user-name` | Invalid | Contains an illegal operator character (`-`). |
| `total sum` | Invalid | Contains a space, which acts as a token separator. |

### Operation Symbols

This language adopts standard C-like operation symbols for all mathematical, relational, and logical evaluation. Formally, the operation symbols (Op) are defined as a union of six distinct lexical sets: ARITHMETIC, RELATIONAL, LOGICAL, ASSIGNMENT, PIPELINE, and ARROW.

Let:

* $ARITHMETIC = \{+, -, *, /, \%, \backslash, \wedge\}$
* $RELATIONAL = \{==, !=, <=, >=, <, >\}$
* $LOGICAL = \{!, \&\&, \vert{}\vert{}\}$
* $ASSIGNMENT = \{=, +=, -=, *=, /=, \%=\}$
* $PIPELINE = \{\vert{}>\}$
* $ARROW = \{->\}$
* $Op = ARITHMETIC \cup RELATIONAL \cup LOGICAL \cup ASSIGNMENT \cup PIPELINE \cup ARROW$

**ARITHMETIC - Standard mathematical operations**

| Operator | Description |
| --- | --- |
| `+` | add |
| `-` | subtract |
| `*` | multiply |
| `/` | divide |
| `%` | modulo |
| `\` | integer division |
| `^` | exponent |

**BOOLEAN - RELATIONAL**

| Operator | Description |
| --- | --- |
| `==` | Value comparison evaluating to a boolean state (Equal to) |
| `!=` | Value comparison evaluating to a boolean state (Not equal to) |
| `<=` | Value comparison evaluating to a boolean state (Less than or equal to) |
| `>=` | Value comparison evaluating to a boolean state (Greater than or equal to) |
| `<` | Value comparison evaluating to a boolean state (Less than) |
| `>` | Value comparison evaluating to a boolean state (Greater than) |

**BOOLEAN - LOGICAL**

| Operator | Description |
| --- | --- |
| `!` | Negates a boolean condition (NOT) |
| `&&` | Combines boolean conditions (AND) |
| `\|\|` | Evaluates multiple boolean conditions (OR) | 

**ASSIGNMENT**

| Operator | Description |
| --- | --- |
| `=` | Assigns the value of the right-hand expression to the left-hand variable. |
| `+=` | Adds the right-hand value to the variable and reassigns the result. |
| `-=` | Subtracts the right-hand value from the variable and reassigns the result. |
| `*=` | Multiplies the variable by the right-hand value and reassigns the result. |
| `/=` | Divides the variable by the right-hand value and reassigns the result. |
| `%=` | Assigns the variable the remainder of dividing it by the right-hand value. |

**PIPELINE**

| Set Element | Description |
| --- | --- |
| `\|>` | Passes the evaluated result of the left-hand expression directly as the first argument to the function on the right side. |

**ARROW**

| Set Element | Description |
| --- | --- |
| `->` | Acts as a directional mapper that links a function declaration to its explicit return type or routes a matched condition to its corresponding execution path. |

---

## Keywords and Reserved Words

### Keywords

Keywords are action-oriented commands that instruct the compiler to perform a specific operation or dictate the control flow.

| Category | Keyword | Description |
| --- | --- | --- |
| **NULL SAFETY** | `ensure` | Acts as a strict safety gateway to unwrap optional variables, mandating a control flow exit on failure. |
| **CONTROL FLOW** | `transition` | Routes the state machine by transferring execution to a newly specified state block. |
| | `match` | Handles exhaustive pattern evaluation, allowing execution to branch based on matching data values. |
| | `if` | Initiates standard logical branching by executing a block of code only if a provided condition evaluates to true. |
| | `else` | Provides an alternative execution path for logical branching when an if condition evaluates to false. |
| | `for` | Handles standard counting and iterative looping over a sequence or range of values. |
| **INPUT/OUTPUT** | `print` | A built-in action used to push formatted text or variable values directly to the terminal output. |
| | `readLine` | Pauses program execution to capture standard string input from the user via the terminal. |
| | `readInt` | Pauses program execution to safely capture and parse integer input from the user via the terminal. |
| **SYSTEM** | `quit` | An action command that instructs the compiled program to terminate its process entirely and exit. |
| | `sleep` | Pauses the execution thread for a predefined duration, measured in milliseconds. |
| **MODULARITY** | `fn` | Used to declare reusable, pure helper functions that execute independently of the main state machine. |
|  | `return` | Sends a computed value back to the caller's execution flow, terminating the current function block. |

### Reserved Words

Reserved Words hold special architectural or structural meaning within the language. While they define boundaries or represent definitive values, they do not execute actions themselves. These words are protected by the compiler and cannot be used as variable identifiers.

| Category | Reserved Word | Description |
| --- | --- | --- |
| **DATA TYPES** | `int` | Defines the specific primitive type for a variable intended to store whole numbers. Abstract or user-defined data types are not supported. |
|  | `string` | Defines the specific primitive type for a variable intended to store a sequence of text characters. |
|  | `bool` | Defines the specific primitive type for a variable intended to store true or false logical states. |
|  | `float` | Defines the specific primitive type for a variable intended to store numerical values with fractional or decimal components. |
|  | `char` | Defines the specific primitive type for a variable intended to store a single alphanumeric character or symbol. |
| **STRUCTURAL** | `state` | Defines the architectural boundaries of the program by encapsulating a discrete machine mode and its associated logic. |
|  | `react` | Defines a data-driven watcher that triggers automatically when a bound piece of state or data changes. |
|  | `loop` | Defines an infinite, blocking execution context, typically used for main game or event-listening loops. |
| **STATE LITERALS** | `true` | The definitive positive state value for boolean variables and logical evaluations. |
|  | `false` | The definitive negative state value for boolean variables and logical evaluations. |
|  | `null` | The explicit empty representation used for optional primitive types (e.g., Int?), indicating the deliberate absence of a value. |

---

## Noise Words

Noise words are optional terms injected into the syntax solely to enhance human readability. They hold no semantic or structural value. The lexical analyzer systematically discards these tokens during the tokenization phase, meaning the parser never processes them and they have zero impact on the program's execution logic.

Defined Noise Words: `is`, `then`, `do`, `to`

Because noise words are discarded, the parser evaluates statements exactly the same whether the programmer includes them or not. This allows developers to write code that reads closer to natural English without complicating the grammar rules of the compiler.

| Programmer Input (With Noise Words) | Equivalent Code (Parsed AST) |
| --- | --- |
| `int roomTemp is = 22;`<br> | `int roomTemp = 22;`<br> |
| `ensure pin = input then { ... }`<br> | `ensure pin = input { ... }`<br> |
| `state Dashboard do { ... }`<br> | `state Dashboard { ... }`<br> |
| `transition to Dashboard;`<br> | `transition Dashboard;`<br> |

---

## Comments

* **Single-line comments:** Initiated by double forward slashes (`//`). Lexer ignores all characters following the `//` until a hard return is encountered.


* Example:





```c
// This is a single-line comment

```

* **Multi-line block comments:** Enclosed within `/*` and `*/`. The lexer discards all characters between these boundaries, allowing the comment to span multiple lines.


* Example:





```c
/* This is
a multi-line
comment */

```

## Blanks (spaces)

Blanks, spaces, tabs (`\t`), and newlines (`\n`) are treated purely as token separators. The lexical analyzer completely ignores and discards these whitespaces during the tokenization phase. The only exception is when blanks are enclosed within string literals (e.g., `"Hello World"`), where they are preserved as part of the token.

---

## Delimiters and Brackets

**Delimiters**

| Symbol | Name | Function / Usage |
| --- | --- | --- |
| `;` | Semicolon (terminator) | Acts as an optional hard terminator used to separate and conclude individual statements. |
| `,` | Comma (separator) | Acts as a soft separator used to separate parameters and arguments. |
| `:` | Colon (type annotator) | Acts as a type annotator used to separate an identifier from its declared type. |

**Brackets**

| Symbol | Name | Function / Usage |
| --- | --- | --- |
| `()` | Parentheses | Used for mathematical expression groupings and enclosing function arguments. |
| `{}` | Curly Braces | Groups multiple statements together to define scope blocks for functions, loops, and conditional statements. |
| `[]` | Square Brackets | Used for array indexing and list declaration. |

### Pushdown Automaton of the Delimiters

*(Reference from document diagram)*

```text
Z - Represents the bottom of stack
(, Z → (Z       [, Z → [Z
(, ( → ((       [, ( → [(
(, { → ({       [, { → [{
(, [ → ([       [, [ → [[

Start (q0)      ε, Z → Z      (q2)

{, Z → {Z       ), ( → ε
{, ( → {(       }, { → ε
{, { → {{       ], [ → ε
{, [ → {[

```

---

## Free-and-fixed-field formats

The program will use a free-form format. Delimiters will be used to separate the statements. This means the programmer won't have to worry about strict rules for indentation or line breaks breaking your code. Whether you prefer to space things out over multiple lines to make a complex calculation easier to read, or you just want to stack a few quick commands on a single line, the compiler won't mind. As long as you drop that delimiter at the end of your instruction, the parser will know exactly what you mean, giving you the complete freedom to format your work however it makes the most sense to you.

---

## Expression

An expression is any valid combination of variables, literals (constants), operation symbols, and function calls that the compiler evaluates to produce a single value. Since this language is strictly typed, every expression resolves to a specific primitive data type (Int, Float, Boolean, Char, String).

**Types of Expressions:**

1. **Arithmetic Expressions:** Combine numbers and arithmetic operators, evaluating to an int or float. This includes standard division (`/`) which yields a float, and integer division (`\`), which divides two numbers and truncates the decimal remainder to return a strict int.


* Example: `(price * 0.2) + 5`

* Example (Integer Div): `10 \ 3` (Evaluates to 3)




2. **Relational and Logical Expressions:** Combine values with comparison or boolean operators, evaluating strictly to a Boolean(true or false).


* Example: `score >= 50 && !is_absent`



3. **Pipeline Expressions:** Chains function from left to right. The output of the left side automatically becomes the input for the function on the right, avoiding messy, nested code.


* Example: Instead of `print(uppercase(trim("hello")))`, we can write it instead as `"hello" |> trim() |> uppercase() |> print()`



4. **Array Access Expressions:** Retrieves a specific element from an array using its 0-based numeric index enclosed in square brackets `[]`. This expression evaluates to the underlying data type of the array.


* Given: `[int] scores = [90, 85, 88]`

* Example: `scores[0]` (Evaluates to 90)


* Example: `print("First score is: " + scores[0])`




### Evaluation Rules

To ensure that mathematical logic behaves in a standard mathematical order of operations, the compiler will evaluate expressions using PEMDAS or EMDAS (Parenthesis, Exponents, Multiplication, Division, Addition, Subtraction).

**Operator Precedence Table**


Operations are evaluated from highest precedence (Level 1) to lowest (Level 10). Operators on the same level are evaluated based on their associativity. (Note: Exponents are evaluated Right-to-Left to align with standard mathematical rules, e.g., $2^{\wedge}3^{\wedge}2$ evaluates as $2^{\wedge}(3^{\wedge}2)$).

| Precedence Level | Operator Type | Symbols | Associativity |
| --- | --- | --- | --- |
| **1 (Highest)** | Parentheses (Grouping) | `()` | Left-to-Right |
| **2** | Exponent | `^` | Right-to-Left |
| **3** | Logical NOT | `!` | Right-to-Left |
| **4** | Multiplicative | `*`, `/`, `\`, `%` | Left-to-Right |
| **5** | Additive | `+`, `-` | Left-to-Right |
| **6** | Relational | `<`, `>`, `<=`, `>=` | Left-to-Right |
| **7** | Equality | `==`, `!=` | Left-to-Right |
| **8** | Logical AND | `&&` | Left-to-Right |
| **9** | Logical OR | `\|\|` | Left-to-Right |
| **10 (Lowest)** | Pipeline Operator | `\|>` | Left-to-Right |

---

## Data Types

### 1. Core Primitives



| Data Type | Keyword | Description | Declaration Example |
| --- | --- | --- | --- |
| **Integer** | `int` | Stores whole numbers without decimals. Typically used for indexing, counting, and discrete math. | `int roomTemp = 22;`<br> |
| **Floating-Point** | `float` | Stores numbers with fractional (decimal) parts. Replaces "Decimal" to align with standard industry syntax. | `float weight = 150.5`<br> |
| **Character** | `char` | Stores a single alphanumeric character or symbol, enclosed in single quotes. | `char grade = 'A'`<br> |
| **Boolean** | `bool` | Stores a true or false value. Used exclusively for logical operations and conditional control flow. | `bool is_valid = true`<br> |

### 2. Abstract & Composite Data Types



| Data Type | Keyword | Description | Declaration Example |
| --- | --- | --- | --- |
| **String** | `string` | An ADT representing a sequence of text characters. Used heavily for I/O and terminal parsing. | `string? tempInput = readLine()`<br> |
| **Array** | `[]` | An ordered collection of elements sharing the same data type. Enclosed in brackets. | `[int] scores = [90, 85, 88]`<br> |

---

## Statements

### a. Declaration Statements

Declaration statements allocate memory and define the strict type of a variable. The compiler binds variable declarations into strict primitives to make the language type safe.

```c
// Variable Declaration
int roomTemp = 22
int? input = null
[string] accessLogs = ["System Booted", "Thermostat Initialized"]

// Function Declaration
fn formatStatus (bool status) -> string {
    return (status == true) ? "ACTIVE" : "INACTIVE"
}

```


### b. Input Statements

Input statements handle the retrieval of data from the terminal prompt. Because standard input is inherently unsafe, these statements natively return Optional types (e.g., Int?, String?) to prevent crashes when a user enters unexpected characters.

```c
string tempInput = readLine()
int? pin = readInt()

```


### c. Output Statements

Output statements push formatted text directly to the console display. This unified approach handles standard strings alongside primitive concatenation.

```c
print("--- IOT MAIN MENU ---")
print("Current temperature is " + roomTemp + "C.")

```


### d. Assignment Statements

Assignment statements update the value of an already declared mutable variable. Crucially, updating a global variable via an assignment operator (`=`) automatically triggers any react blocks bound to that specific identifier, decoupling business logic from presentation logic.

```c
lightsOn = true
roomTemp += 5
scores[1] = 95 // Updates the second element in the array

```


### e. Condition Statements

Condition statements branch the logic of the program based on real-time evaluations. The language supports standard if/else blocks (which require parentheses around the condition) for straightforward boolean logic, alongside single-line if statements for concise checks.

```c
// If
if (lightsOn == true) {
    print("[SYSTEM EVENT] Living room lights illuminated.")
}

// If/else
if (lightsOn == true) {
    print("[SYSTEM EVENT] Living room lights illuminated.")
} else {
    print("[SYSTEM EVENT] Living room lights extinguished.")
}

```


For handling unsure data, the `ensure` gateway safely unwraps Optional types into strict primitives. It mandates a control flow on failure which guarantees that null or invalid data cannot propagate further into other state block.

```c
// Ensure
ensure validTemp = newTemp else {
    print("[ERROR] Temperature must be a number.")
    transition to TempControl
}

```


**Case Matching**


The `match` expression provides an advanced, scalable alternative to deeply nested if/else statements for exhaustive input routing. It evaluates a single variable against multiple potential values, using an arrow (`->`) to immediately direct the control flow.

For lightweight routing, developers can use single-line arrows. But when routing requires intermediate steps, such as variable mutation or logging before a state transition, `match` supports multi-statement scopes enclosed in curly braces. The underscore (`_`) is used as the required default fallback case to catch unhandled inputs.

```c
// Case Matching
match choice {
    "1" -> transition to LightControl
    "2" -> transition to TempControl
    _   -> print("Unknown command.")
}

// Case Matching with blocks
match choice {
    "1" -> transition to LightControl
    "2" -> {
        print("Initializing climate interface...")
        transition to TempControl
    }
    "3" -> transition to StatusView
    _   -> {
        invalidAttempts += 1
        print("Unknown command. Attempts: (invalidAttempts)")
    }
}

```


### f. Iterative Statements

Iterative statements execute a block of code multiple times. The language utilizes explicit `loop` blocks for infinite, blocking terminal prompts that wait for user input, alongside standard `for` loops for fixed sequential numeric counting.

```c
// Infinite Blocking Loop
loop {
    print("Select an action:")
    string choice = readLine()
    // State transition breaks the loop
    transition to TempControl
}

// Iterating through an array using an index
[int] scores = [90, 85, 88]
for (int i = 0; i < length(scores); i++) {
    print("Score " + (i + 1) + " is: " + scores[i])
}

```


**Nested Loops**


You can place one loop inside another to create a nested loop. The inner loop will execute all of its iterations completely for every single iteration of the outer loop. This is especially useful for processing multidimensional concepts, such as grids or coordinates, or executing a sequential task during an infinite main loop.

```c
// Nested Iteration
for (int x = 1; x <= 3; x++) {
    for (int y = 1; y <= 2; y++) {
        print("Processing coordinate: (" + x + "," + y + ")")
    }
}

```