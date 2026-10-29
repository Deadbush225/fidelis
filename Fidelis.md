FIDELIS: A Statically Typed and State-Driven Programming Language

**Members:**
Bulala, Kenneth Andrei G.
Caadan, Psalm Francis O.
Cilon, Rachel Anne
Dukha, James Manuel M.
Inso, Eliazar N.
Rodriguez, Jan Earl F.
Singson, Justin C.


## Introduction

Fidelis is a statically-typed, state-driven language that brings the
beautiful, reactive developer experience of modern web frameworks to
command line apps. Taking cues from the concise syntax of Ruby and
Kotlin, and the reactivity model of frontend technologies like Svelte
and Vue, Fidelis aims to make terminal interfaces instantly responsive
and reliable, without blocking the main execution thread. The language
avoids the runtime crashes that so often result from invalid or
unpredictable user input by treating applications as state-driven rather
than purely procedural.

Fidelis basically works by having localized blocks of logic that
recalculate and render as data changes, whether that data is from
standard input, system events, or state changes. It forces developers to
validate standard input at the language level, via safe extractors and
strict null validation, not via synchronous execution or continuous
polling loops, like TypeScript and Swift. Fidelis ultimately bridges the
gap between powerful, built-in type safety and live reactivity, allowing
developers to build highly stable command-line programs with a modern,
fluid workflow.

## Syntactic Elements of Language

### Character Set

Let:

> Σ = *ALPHA* ∪ *DIGIT* ∪ *SYMBOL* ∪ *WHITESPACE* ∪ UNDERSCORE

Where:

  ---------------- ---------------------------------------------------------
           *ALPHA* = {*ALPHA~lower~* ∪ *ALPHA~upper~}*

    *ALPHA~lower~* = *{a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r,
                   s, t, u, v, w, x, y, z}*

    *ALPHA~upper~* = *{A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R,
                   S, T, U, V, W, X, Y, Z}*

           *DIGIT* = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}

          *SYMBOL* = {(, ), {, }, \[, \], \', \", !, =, +, -, \*, /, \\, %,
                   :, ,, \|, &, ?, ., ;, \<, \>, \^ }

      *WHITESPACE* = {space, tab, newline}

      *UNDERSCORE* = { \_ }
  ---------------- ---------------------------------------------------------


### Identifiers

Identifiers are names used for variables, functions, and tasks. For this
language, it is to be defined and parsed using the following regular
expression:

$($$(ALPHA\ |\ UNDERSCORE)^{+}(ALPHA\ |\ DIGIT\ |\ UNDERSCORE)$\*$)^{32}$

This rule dictates that an identifier must:

1.  An identifier must begin with a letter (A-Z, a-z) or an underscore
    (\_).

2.  The initial character may be followed by any combination of letters,
    digits (0-9), or underscores.

3.  Spaces and other symbols are not allowed.

4.  The maximum allowed length is 32 characters.

5.  Identifiers are case-sensitive; the compiler treats uppercase and
    lowercase letters as distinct characters.

Identifier Evaluation Example:

  ---------------- ----------------- ---------------------------------------
   **Identifier**      **Parser                    **Reason**
                     Evaluation**    

     studentAge        **Valid**      Starts with a letter, containing only
                                                    letters.

     \_score_1         **Valid**     Starts with an underscore, followed by
                                               letters and digits.

     MAX_VALUE         **Valid**       Starts with a letter, contains only
                                       uppercase letters and underscores.

      1stPlace        **Invalid**      Violates the regular expression by
                                             starting with a digit.

     user-name        **Invalid**    Contains an illegal operator character
                                                      (-).

     total sum        **Invalid**    Contains a space, which acts as a token
                                                   separator.
  ---------------- ----------------- ---------------------------------------

### Operation Symbols

This language adopts standard C-like operation symbols for all
mathematical, relational, and logical evaluation. Formally, the
operation symbols (*Op)* are defined as a union of six distinct lexical
sets: ARITHMETIC, RELATIONAL, LOGICAL, ASSIGNMENT, PIPELINE, and ARROW.

Let:

+---------------:+:----------------------------------------------------+
| ARITHMETIC     | = { +, -, \*, /, %, \\, \^ }                        |
+----------------+-----------------------------------------------------+
| > RELATIONAL   | = { ==, !=, \<=, \>=, \<, \> }                      |
+----------------+-----------------------------------------------------+
| > LOGICAL      | = { !, &&, \|\| }                                   |
+----------------+-----------------------------------------------------+
| > ASSIGNMENT   | = { =, +=, -=, \*=, /=, %= }                        |
+----------------+-----------------------------------------------------+
| > PIPELINE     | = { \|\> }                                          |
+----------------+-----------------------------------------------------+
| > ARROW        | = { -\> }                                           |
+----------------+-----------------------------------------------------+

> Op = {ARITHMETIC ∪ RELATIONAL ∪ LOGICAL ∪ ASSIGNMENT ∪ PIPELINE
>
> ∪ ARROW}

#### Arithmetic

Performs fundamental mathematical calculations to manipulate numerical
data and compute exact quantitative results.

  --------------- ------------------------------------------------------
   **Operator**                      **Description**

        \+          **Addition** - Adds two values together to produce
                                     their total sum.

        \-        **Subtraction** - Subtracts the second value from the
                           first to calculate their difference.

        \*        **Multiplication** - Multiplies two values together to
                                 calculate their product.

         /         **Division** - Divides the first value by the second
                              to return the exact quotient.

        \%         **Modulo** - Divides two values and returns only the
                               remainder of the operation.

        \\         **Integer Division** - Divides two values and drops
                          the decimal to return a whole number.

        \^         **Exponent** - Raises a base value to the power of a
                                   specified exponent.
  --------------- ------------------------------------------------------

#### 

#### Boolean

Represents fundamental truth values used to evaluate conditions, drive
decision-making logic, and manage application control flow. It is
further categorized into Relational and Logical Operators.

##### Relational

Compares distinct values or expressions against one another to evaluate
their specific relationship as a definitive boolean state.

  --------------- ------------------------------------------------------
   **Operator**                      **Description**

        ==        Value comparison evaluating to a boolean state (Equal
                                           to)

        !=         Value comparison evaluating to a boolean state (Not
                                        equal to)

        \<=        Value comparison evaluating to a boolean state (Less
                                    than or equal to)

        \>=           Value comparison evaluating to a boolean state
                                (Greater than or equal to)

        \<         Value comparison evaluating to a boolean state (Less
                                          than)

        \>            Value comparison evaluating to a boolean state
                                      (Greater than)
  --------------- ------------------------------------------------------


##### Logical

Combines, evaluates, or inverts boolean conditions to construct complex
decision-making logic and conditional pathways.

  ---------------- ------------------------------------------------------
    **Operator**                      **Description**

         !                   Negates a boolean condition (NOT)

         &&                  Combines boolean conditions (AND)

        \|\|             Evaluates multiple boolean conditions (OR)
  ---------------- ------------------------------------------------------


#### Assignment

Binds evaluated expressions to variables and provides efficient
shorthand for mutating existing data states in place.

  ---------------- ------------------------------------------------------
    **Operator**                      **Description**

         =         Assigns the value of the right-hand expression to the
                                    left-hand variable.

         +=            Adds the right-hand value to the variable and
                                   reassigns the result.

         -=         Subtracts the right-hand value from the variable and
                                   reassigns the result.

        \*=         Multiplies the variable by the right-hand value and
                                   reassigns the result.

         /=           Divides the variable by the right-hand value and
                                   reassigns the result.

         %=         Assigns the variable the remainder of dividing it by
                                   the right-hand value.
  ---------------- ------------------------------------------------------


#### Pipeline

Streamlines functional data flow by passing evaluated results forward to
flatten nested function calls into readable sequences.

  ---------------- ------------------------------------------------------
  **Set Element**                     **Description**

        \|\>            Passes the evaluated result of the left-hand
                      expression directly as the first argument to the
                                function on the right side.
  ---------------- ------------------------------------------------------

#### Arrow

Evaluates variables against multiple potential values to dynamically
route control flow and manage complex state transitions.

  ---------------- ------------------------------------------------------
  **Set Element**                     **Description**

        -\>          Acts as a direct case mapper within case matching
                   expressions to route the process to a different state
                               based on the evaluated input.
  ---------------- ------------------------------------------------------



### Keywords and Reserved Words

#### Keywords

Keywords are action-oriented commands that instruct the compiler to
perform a specific operation or dictate the control flow.

+:--------------:+:-----------:+:--------------------------------------:+
| **Category**   | **Keyword** | **Description**                        |
+----------------+-------------+----------------------------------------+
| **NULL         | ensure      | Acts as a strict safety gateway to     |
| SAFETY**       |             | unwrap optional variables, mandating a |
|                |             | control flow exit on failure.          |
+----------------+-------------+----------------------------------------+
| **CONTROL      | transition  | Routes the state machine by            |
| FLOW**         |             | transferring execution to a newly      |
|                |             | specified state block.                 |
|                +-------------+----------------------------------------+
|                | match       | Handles exhaustive pattern evaluation, |
|                |             | allowing execution to branch based on  |
|                |             | matching data values.                  |
|                +-------------+----------------------------------------+
|                | if          | Initiates standard logical branching   |
|                |             | by executing a block of code only if a |
|                |             | provided condition evaluates to true.  |
|                +-------------+----------------------------------------+
|                | else        | Provides an alternative execution path |
|                |             | for logical branching when an if       |
|                |             | condition evaluates to false.          |
|                +-------------+----------------------------------------+
|                | for         | Handles standard counting and          |
|                |             | iterative looping over a sequence or   |
|                |             | range of values.                       |
+----------------+-------------+----------------------------------------+
| **INPUT/       | print       | A built-in action used to push         |
| OUTPUT**       |             | formatted text or variable values      |
|                |             | directly to the terminal output.       |
|                +-------------+----------------------------------------+
|                | readLine    | Pauses program execution to capture    |
|                |             | standard string input from the user    |
|                |             | via the terminal.                      |
|                +-------------+----------------------------------------+
|                | readInt     | Pauses program execution to safely     |
|                |             | capture and parse integer input from   |
|                |             | the user via the terminal.             |
+----------------+-------------+----------------------------------------+
| **SYSTEM**     | quit        | An action command that instructs the   |
|                |             | compiled program to terminate its      |
|                |             | process entirely and exit.             |
|                +-------------+----------------------------------------+
|                | sleep       | Pauses the execution thread for a      |
|                |             | predefined duration, measured in       |
|                |             | milliseconds.                          |
+----------------+-------------+----------------------------------------+
| **MODULARITY** | fn          | Used to declare reusable, pure helper  |
|                |             | functions that execute independently   |
|                |             | of the main state machine.             |
|                +-------------+----------------------------------------+
|                | return      | Sends a computed value back to the     |
|                |             | caller\'s execution flow, terminating  |
|                |             | the current function block.            |
+----------------+-------------+----------------------------------------+





### Keywords - Finite Automaton


#### Reserved Words

Reserved Words hold special architectural or structural meaning within
the language. While they define boundaries or represent definitive
values, they do not execute actions themselves. These words are
protected by the compiler and cannot be used as variable identifiers.

+:--------------:+:------------:+:------------------------------------:+
| **Category**   | **Reserved   | **Description**                      |
|                | Word**       |                                      |
+----------------+--------------+--------------------------------------+
| **DATA TYPES** | int          | Defines the specific primitive type  |
|                |              | for a variable intended to store     |
|                |              | whole numbers. Abstract or           |
|                |              | user-defined data types are not      |
|                |              | supported.                           |
|                +--------------+--------------------------------------+
|                | string       | Defines the specific primitive type  |
|                |              | for a variable intended to store a   |
|                |              | sequence of text characters.         |
|                +--------------+--------------------------------------+
|                | bool         | Defines the specific primitive type  |
|                |              | for a variable intended to store     |
|                |              | *true* or *false* logical states.    |
|                +--------------+--------------------------------------+
|                | float        | Defines the specific primitive type  |
|                |              | for a variable intended to store     |
|                |              | numerical values with fractional or  |
|                |              | decimal components.                  |
|                +--------------+--------------------------------------+
|                | char         | Defines the specific primitive type  |
|                |              | for a variable intended to store a   |
|                |              | single alphanumeric character or     |
|                |              | symbol.                              |
+----------------+--------------+--------------------------------------+
| **STRUCTURAL** | state        | Defines the architectural boundaries |
|                |              | of the program by encapsulating a    |
|                |              | discrete machine mode and its        |
|                |              | associated logic.                    |
|                +--------------+--------------------------------------+
|                | react        | Defines a data-driven watcher that   |
|                |              | triggers automatically when a bound  |
|                |              | piece of state or data changes.      |
|                +--------------+--------------------------------------+
|                | loop         | Defines an infinite, blocking        |
|                |              | execution context, typically used    |
|                |              | for main game or event-listening     |
|                |              | loops.                               |
+----------------+--------------+--------------------------------------+
| **STATE        | true         | The definitive positive state value  |
| LITERALS**     |              | for boolean variables and logical    |
|                |              | evaluations.                         |
|                +--------------+--------------------------------------+
|                | false        | The definitive negative state value  |
|                |              | for boolean variables and logical    |
|                |              | evaluations.                         |
|                +--------------+--------------------------------------+
|                | null         | The explicit empty representation    |
|                |              | used for optional primitive types    |
|                |              | (e.g., Int?), indicating the         |
|                |              | deliberate absence of a value.       |
+----------------+--------------+--------------------------------------+

**Reserved Words - Finite Automaton**



### Noise Words

Noise words are optional terms injected into the syntax solely to
enhance human readability. They hold no semantic or structural value.
The lexical analyzer systematically discards these tokens during the
tokenization phase, meaning the parser never processes them and they
have zero impact on the program's execution logic.

Defined Noise Words:

- **is**

- **do**

- **to**

Because noise words are discarded, the parser evaluates statements
exactly the same whether the programmer includes them or not. This
allows developers to write code that reads closer to natural English
without complicating the grammar rules of the compiler.

  ------------------------------------ ---------------------------------
     **Programmer Input (With Noise           **Equivalent Code**
                Words)**               

       int roomTemp **is** = 22;              int roomTemp = 22;

    state Dashboard **do** { \... }        state Dashboard { \... }

      transition **to** Dashboard;           transition Dashboard;
  ------------------------------------ ---------------------------------


### Comments

- **Single-line comments** - Initiated by double forward slashes (//).
  Lexer ignores all characters following the // until a hard return is
  encountered.

```
// This is a single-line comment
```

- **Multi-line block comments** - Enclosed within /\* and \*/ . The
  lexer discards all characters between these boundaries, allowing the
  comment to span multiple lines.

```
/* This is
a multi-line
comment */
```

**Comments - Finite Automaton**


### Blanks (spaces)

Blanks, spaces, tabs (\\t), and newlines (\\n) are treated purely as
token separators. The lexical analyzer completely ignores and discards
these whitespaces during the tokenization phase. The only exception is
when blanks are enclosed within string literals (e.g., "Hello World"),
where they are preserved as part of the token.

### Delimiters and Brackets

**Delimiters**

+:----------:+:--------------:+:-------------------------------------------:+
| **Symbol** | **Name**       | **Function / Usage**                        |
+------------+----------------+---------------------------------------------+
| ;          | **Semicolon    | Acts as an optional hard terminator used to |
|            | (terminator)** | separate and conclude individual            |
|            |                | statements.                                 |
+------------+----------------+---------------------------------------------+
| ,          | **Comma        | Acts as a soft separator used to separate   |
|            | (separator)**  | parameters and arguments.                   |
+------------+----------------+---------------------------------------------+
| :          | **Colon**      | Acts as a type annotator used to separate   |
|            |                | an identifier from its declared type.       |
|            | **(type        |                                             |
|            | annotator)**   |                                             |
+------------+----------------+---------------------------------------------+

**Delimiters - Automaton**

; **Semicolon (Terminator)**


**, Comma (separator)**


**: Colon (type annotator)**


**Brackets**

  ------------ ----------------- --------------------------------------------
   **Symbol**      **Name**                  **Function / Usage**

       ()       **Parentheses**   Used for mathematical expression groupings
                                      and enclosing function arguments.

       {}      **Curly Braces**     Groups multiple statements together to
                                  define scope blocks for functions, loops,
                                         and conditional statements.

      \[\]         **Square            Used for array indexing and list
                  Brackets**                     declaration.
  ------------ ----------------- --------------------------------------------

***Pushdown Automaton of the Brackets***



### Free-and-fixed-field formats

The program will use a free-form format. Delimiters will be used to
separate the statements. This means the programmer won\'t have to worry
about strict rules for indentation or line breaks breaking your code.
Whether you prefer to space things out over multiple lines to make a
complex calculation easier to read, or you just want to stack a few
quick commands on a single line, the compiler won\'t mind. As long as
you drop that delimiter at the end of your instruction, the parser will
know exactly what you mean, giving you the complete freedom to format
your work however it makes the most sense to you.

### Expression

An expression is any valid combination of variables, literals
(constants), operation symbols, and function calls that the compiler
evaluates to produce a single value. Since this language is strictly
type d, every expression resolves to a specific primitive data type
(Int, Float, Boolean, Char, String).

#### Types of Expressions: 

#### Arithmetic Expressions

Combine numbers and arithmetic operators, evaluating to an int or float.
This includes standard division (/) which yields a float, and integer
division (\\), which divides two numbers and truncates the decimal
remainder to return a strict int.

- **Example:** (price \* 0.2) + 5

- **Example:** 10 \\ 3 (Evaluates to 3)

#### Relational and Logical Expressions

Combine values with comparison or boolean operators, evaluating strictly
to a Boolean (*true* or *false*).

- **Example:** score \>= 50 && !is_absent

#### Pipeline Expressions

Chains function from left to right. The output of the left side
automatically becomes the input for the function on the right, avoiding
messy, nested code.

- **Example:** Instead of print(uppercase(trim("hello "))), we can write
  it instead as "hello " \|\> trim() \|\> uppercase() \|\> print() )

#### Array Access Expressions 

Retrieves a specific element from an array using its 0-based numeric
index enclosed in square brackets \[\]. This expression evaluates to the
underlying data type of the array.

> Given: \[int\] scores = \[90, 85, 88\]

- **Example:** scores\[0\] *(Evaluates to 90)*

- **Example:** print(\"First score is: \" + scores\[0\])

#### Evaluation Rules

To ensure that mathematical logic behaves in a standard mathematical
order of operations, the compiler will evaluate expressions using PEMDAS
or EMDAS (Parenthesis, Exponents, Multiplication, Division, Addition,
Subtraction).

#### 

#### Operator Precedence Table

Operations are evaluated from highest precedence (Level 1) to lowest
(Level 8). Operators on the same level are evaluated based on their
associativity. (Note: Exponents are evaluated Right-to-Left to align
with standard mathematical rules, e.g., 2\^3\^2 evaluates as 2\^(3\^2)).

  ----------------- ---------------------- ---------------- -------------------
    **Precedence      **Operator Type**      **Symbols**     **Associativity**
       Level**                                              

   **1 (Highest)**  Parentheses (Grouping)       ( )           Left-to-Right

        **2**              Exponent               \^           Right-to-Left

        **3**            Logical NOT              !            Right-to-Left

        **4**           Multiplicative       \*, /, \\, %      Left-to-Right

        **5**              Additive              +, -          Left-to-Right

        **6**             Relational       \<, \>, \<=, \>=    Left-to-Right

        **7**              Equality             ==, !=         Left-to-Right

        **8**            Logical AND              &&           Left-to-Right

        **9**             Logical OR             \|\|          Left-to-Right

   **10 (Lowest)**    Pipeline Operator          \|\>          Left-to-Right
  ----------------- ---------------------- ---------------- -------------------


### Data Types

#### Core Primitives

Defines the fundamental, strictly typed building blocks used to store
basic scalar values and enforce memory safety throughout the program.

+:------------------:+:-----------:+:------------------------------------------:+
| **Data Type**      | **Keyword** | **Description**                            |
+--------------------+-------------+--------------------------------------------+
| **Integer**        | int         | Stores whole numbers without decimals.     |
|                    |             | Typically used for counting, indexing, and |
|                    |             | discrete math.                             |
|                    |             |                                            |
|                    |             | Example:                                   |
|                    |             |                                            |
|                    |             | int roomTemp = 22;                         |
+--------------------+-------------+--------------------------------------------+
| **Floating-Point** | float       | Stores numbers with fractional (decimal)   |
|                    |             | parts. Replaces \"Decimal\" to align with  |
|                    |             | standard industry syntax.                  |
|                    |             |                                            |
|                    |             | Example:                                   |
|                    |             |                                            |
|                    |             | float weight = 150.5;                      |
+--------------------+-------------+--------------------------------------------+
| **Character**      | char        | Stores a single alphanumeric character or  |
|                    |             | symbol, enclosed in single quotes.         |
|                    |             |                                            |
|                    |             | Example:                                   |
|                    |             |                                            |
|                    |             | char grade = \'A\';                        |
+--------------------+-------------+--------------------------------------------+
| **Boolean**        | bool        | Stores a true or false value. Used         |
|                    |             | exclusively for logical operations and     |
|                    |             | conditional control flow.                  |
|                    |             |                                            |
|                    |             | Example:                                   |
|                    |             |                                            |
|                    |             | bool is_valid = true;                      |
+--------------------+-------------+--------------------------------------------+

#### Optional Data Types

Handles unsure data and prevents runtime crashes by explicitly allowing
any data type to represent the deliberate absence of a value (null).

+:-----------:+:-----------:+:------------------------------------------:+
| **Data      | **Keyword** | **Description**                            |
| Type**      |             |                                            |
+-------------+-------------+--------------------------------------------+
| **Optional  | int?        | Stores a whole number or a null value.     |
| Integer**   |             | Natively returned by safe extractors like  |
|             |             | readInt() to prevent crashes from invalid  |
|             |             | terminal input.                            |
|             |             |                                            |
|             |             | Example:                                   |
|             |             |                                            |
|             |             | int? pin = readInt();                      |
+-------------+-------------+--------------------------------------------+
| **Optional  | float?      | Stores a decimal number or a null value,   |
| Floating-** |             | ensuring safe extraction and handling of   |
|             |             | fractional data.                           |
| **Point**   |             |                                            |
|             |             | Example:                                   |
|             |             |                                            |
|             |             | float? weight = null;                      |
+-------------+-------------+--------------------------------------------+
| **Optional  | char?       | Stores a single alphanumeric character or  |
| Character** |             | a null value.                              |
|             |             |                                            |
|             |             | Example:                                   |
|             |             |                                            |
|             |             | char? initial = null;                      |
+-------------+-------------+--------------------------------------------+
| **Optional  | bool?       | Stores a true or false logical state, or a |
| Boolean**   |             | null value for uninitialized conditions.   |
|             |             |                                            |
|             |             | Example:                                   |
|             |             |                                            |
|             |             | bool? is_valid = null;                     |
+-------------+-------------+--------------------------------------------+
| **Optional  | string?     | Stores a sequence of text characters or a  |
| String**    |             | null value. Natively returned by functions |
|             |             | like readLine() to handle inherently       |
|             |             | unsafe standard input safely.              |
|             |             |                                            |
|             |             | Example:                                   |
|             |             |                                            |
|             |             | string? tempInput = readLine();            |
+-------------+-------------+--------------------------------------------+
| **Optional  | \[type\]?   | Stores an ordered collection of elements   |
| Array**     |             | or a null value.                           |
|             |             |                                            |
|             |             | Example:                                   |
|             |             |                                            |
|             |             | \[int\]? scores = null;                    |
+-------------+-------------+--------------------------------------------+

Optional data types cannot be used directly in standard operations. To
use them, they must first be explicitly unwrapped into strict primitives
using the ensure, which mandates a control flow exit on failure to
guarantee that null or invalid data cannot propagate further into other
state blocks.

#### Abstract & Composite Data Types

Provides complex data structures designed to manage ordered sequences of
text and group multiple elements of the same type into accessible
collections.

+:-----------:+:-----------:+:------------------------------------------:+
| **Data      | **Keyword** | **Description**                            |
| Type**      |             |                                            |
+-------------+-------------+--------------------------------------------+
| **String**  | string      | An ADT representing a sequence of text     |
|             |             | characters. Used heavily for I/O and       |
|             |             | terminal parsing.                          |
|             |             |                                            |
|             |             | Example:                                   |
|             |             |                                            |
|             |             | string? tempInput = readLine();            |
+-------------+-------------+--------------------------------------------+
| **Array**   | \[\]        | An ordered collection of elements sharing  |
|             |             | the same data type. Enclosed in brackets.  |
|             |             |                                            |
|             |             | Example:                                   |
|             |             |                                            |
|             |             | \[int\] scores = \[90, 85, 88\];           |
+-------------+-------------+--------------------------------------------+

### Standard Library

Fidelis provides built-in functions for common operations on strings and
arrays. These functions use the same call syntax as user-defined functions;
their names are not keywords or reserved words.

#### `length`

```text
length(string value) -> int
length([T] value) -> int
```

Returns the number of elements in an array or the number of characters in a
string. `T` represents any supported element type. The argument must be a
non-null array or string. Array indices are zero-based, so the result is the
first index past the end of the array.

```fidelis
[int] scores = [90, 85, 88];
int scoreCount = length(scores); // 3
int labelLength = length("ACTIVE"); // 6
```

#### `replace`

```text
replace(string source, string search, string replacement) -> string
```

Returns a new string made by replacing every non-overlapping occurrence of
`search` in `source` with `replacement`. Matching is case-sensitive, and the
source string is not modified. If `search` is empty, the result is `source`
unchanged. All three arguments must be non-null strings.

```fidelis
string normalized = replace("Ada Lovelace", " ", ""); // "AdaLovelace"
```

### Statements

#### Declaration Statements

Declaration statements allocate memory and define the strict type of a
variable. The compiler binds variable declarations into strict
primitives to make the language type safe.

```
// Variable Declaration
int roomTemp = 22;
int? input = null;
[string] accessLogs = ["System Booted", "Thermostat Initialized"];

// Function Declaration
fn formatStatus(bool status) -> string {
    return (status == true) ? "ACTIVE" : "INACTIVE";
}
```

#### Input Statements

Input statements handle the retrieval of data from the terminal prompt.
Because standard input is inherently unsafe, these statements natively
return Optional types (e.g., Int?, String?) to prevent crashes when a
user enters unexpected characters.

```
string tempInput = readLine();
int? pin = readInt();
```

#### Output Statements

Output statements push formatted text directly to the console display.
This unified approach handles standard strings alongside primitive
concatenation.

```
print("--- IOT MAIN MENU ---");
print("Current temperature is " + roomTemp + "C.");
```

#### Assignment Statements

Assignment statements update the value of an already declared mutable
variable. Crucially, updating a global variable via an assignment
operator (=) automatically triggers any react blocks bound to that
specific identifier, decoupling business logic from presentation logic.

```
lightsOn = true;
roomTemp = 5;
scores[1] = 95; // Updates the second element in the array
```

#### Condition Statements

Condition statements branch the logic of the program based on real-time
evaluations. The language supports standard if/else blocks (which
require parentheses around the condition) for straightforward boolean
logic, alongside single-line if statements for concise checks.

##### If Statement

```
// If
if (lightsOn == true) {
    print("[SYSTEM EVENT] Living room lights illuminated.");
}
```

##### If-Else Statement

```
// If/else
if (lightsOn == true) {
    print("[SYSTEM EVENT] Living room lights illuminated.");
} else {
    print("[SYSTEM EVENT] Living room lights extinguished.");
}
```

**Ensure Typing**

Handling optional data traditionally requires standard if statements to
evaluate null conditions. However, relying on generic conditional
branches for data validation introduces structural inefficiencies.
First, the primary execution flow becomes wrapped within nested scopes
which increases the cyclomatic complexity and diminishes the
readability. Second, standard conditional blocks lack enforced
termination where they do not mandate a state exit upon validation
failure. This deficiency permits invalid or null data to propagate
silently, potentially compromising the integrity of the state machine.

```
// The Standard Conditional Approach (Nested and Unsafe)
int? newTemp = readInt();
if (newTemp != null) {
    int validTemp = newTemp; // The primary logic is restricted to this nested scope
    if (validTemp < 16) {
        print("Error: Minimum temperature is 16C.");
    }
} else {
    print("[ERROR] Must be a number.");
    transition to TempControl; // Exit is not strictly enforced by the compiler
}
```

To enforce strict null safety and avoid deeply nested control
structures, Fidelis introduces the ensure gateway. This keyword safely
unwraps Optional types into strict primitives while maintaining a
flattened execution scope. Crucially, it imposes a strict architectural
contract: the else block must culminate in a definitive control flow
exit (such as a transition or return). By mandating this termination at
compile-time, the language guarantees that unhandled null states cannot
propagate into subsequent computational blocks.

```
// The Ensure Gateway (Linear and Architecturally Safe)
int? newTemp = readInt();
int validTemp = ensure newTemp else {
    print("[ERROR] Temperature must be a number.");
    transition to TempControl; // The compiler mathematically mandates this exit
}

// The primary execution flow remains linear and unnested
if (validTemp < 16) {
    print("Error: Minimum temperature is 16C.");
}
```

##### Case Matching

The match expression provides an advanced, scalable alternative to
deeply nested if/else statements for exhaustive input routing. It
evaluates a single variable against multiple potential values, using an
arrow (-\>) to immediately direct the control flow.

For lightweight routing, developers can use single-line arrows. But when
routing requires intermediate steps, such as variable mutation or
logging before a state transition, match supports multi-statement scopes
enclosed in curly braces. The underscore (\_) is used as the required
default fallback case to catch unhandled inputs.

##### Case Matching for single-line statements

```
// Case Matching
match choice {
    "1" -> transition to LightControl;
    "2" -> transition to TempControl;
    "" -> print("");
    _ -> print("Unknown command.");
}
```

##### Case Matching with blocks

```
// Case Matching with blocks
match choice {
    "1" -> transition to LightControl;
    "2" -> {
        print("Initializing climate interface...");
        transition to TempControl;
    }
    "3" -> transition to StatusView;
    _ -> {
        invalidAttempts += 1;
        print("Unknown command. Attempts: {invalidAttempts}");
    }
}
```

#### Iterative Statements

Iterative statements execute a block of code multiple times. The
language utilizes explicit loop blocks for infinite, blocking terminal
prompts that wait for user input, alongside standard for loops for fixed
sequential numeric counting.

##### Infinite Loop

```
// Infinite Blocking Loop
loop {
    print("Select an action:");
    string choice = readLine();
    // State transition breaks the loop
    transition to TempControl;
}
```

##### For Loop

```
// Iterating through an array using an index
[int] scores = [90, 85, 88];
for (int i = 0; i < length(scores); i += 1) {
    print("Score " + (i + 1) + " is: " + scores[i]);
}
```

##### Nested Loops

You can place one loop inside another to create a nested loop. The inner
loop will execute all of its iterations completely for every single
iteration of the outer loop. This is especially useful for processing
multidimensional concepts, such as grids or coordinates, or executing a
sequential task during an infinite main loop.

```
// Nested Iteration
for (int x = 1; x <= 3; x += 1) {
  for (int y = 1; y <= 2; y += 1) {
        print("Processing coordinate: (" + x + ", " + y + ")");
    }
}
```
