FIDELIS: A Statically Typed and

State-Driven Programming Language

**Members:**

Bulala, Kenneth Andrei G.  
Caadan, Psalm Francis O.  
Cilon, Rachel Anne  
Dukha, James Manuel M.   
Inso, Eliazar N.  
Rodriguez, Jan Earl F.  
Singson, Justin C.

## **Introduction**

Fidelis is a statically-typed, state-driven language that brings the beautiful, reactive developer experience of modern web frameworks to command line apps. Taking cues from the concise syntax of Ruby and Kotlin, and the reactivity model of frontend technologies like Svelte and Vue, Fidelis aims to make terminal interfaces instantly responsive and reliable, without blocking the main execution thread. The language avoids the runtime crashes that so often result from invalid or unpredictable user input by treating applications as state-driven rather than purely procedural.

Fidelis basically works by having localized blocks of logic that recalculate and render as data changes, whether that data is from standard input, system events, or state changes. It forces developers to validate standard input at the language level, via safe extractors and strict null validation, not via synchronous execution or continuous polling loops, like TypeScript and Swift. Fidelis ultimately bridges the gap between powerful, built-in type safety and live reactivity, allowing developers to build highly stable command-line programs with a modern, fluid workflow.

## **Syntactic Elements of Language**

### **Character Set**

Let:

Σ = *ALPHA* ∪ *DIGIT* ∪ *SYMBOL* ∪ *WHITESPACE* ∪ UNDERSCORE

Where:

| *ALPHA* | = {*ALPHAlower* ∪ *ALPHAupper*} |
| :--- | :--- |
| *ALPHAlower* | = *{a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z}* |
| *ALPHAupper* | = *{A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z}* |
| *DIGIT* | = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9} |
| *SYMBOL* | = {(, ), {, }, \[, \], ', ", !, =, +, -, \*, /, \\, %, :, ,, \|, &, ?, ., ;, <, >, ^ } |
| *WHITESPACE* | = {space, tab, newline} |
| *UNDERSCORE* | = { \_ } |

### **Identifiers**

Identifiers are names used for variables, functions, and tasks. For this language, it is to be defined and parsed using the following regular expression:

```
((ALPHA | UNDERSCORE){1}(ALPHA | DIGIT | UNDERSCORE)*){32}
```

This rule dictates that an identifier must:

1. An identifier must begin with a letter (A-Z, a-z) or an underscore (`_`).  
2. The initial character may be followed by any combination of letters, digits (0-9), or underscores.  
3. Spaces and other symbols are not allowed.  
4. The maximum allowed length is 32 characters.  
5. Identifiers are case-sensitive; the compiler treats uppercase and lowercase letters as distinct characters.

Identifier Evaluation Example:

| Identifier | Parser Evaluation | Reason |
| :---: | :---: | ----- |
| studentAge | **Valid** | Starts with a letter, containing only letters. |
| \_score\_1 | **Valid** | Starts with an underscore, followed by letters and digits. |
| MAX\_VALUE | **Valid** | Starts with a letter, contains only uppercase letters and underscores. |
| 1stPlace | **Invalid** | Violates the regular expression by starting with a digit. |
| user-name | **Invalid** | Contains an illegal operator character (-). |
| total sum | **Invalid** | Contains a space, which acts as a token separator. |

### **Operation Symbols**

This language adopts standard C-like operation symbols for all mathematical, relational, and logical evaluation. Formally, the operation symbols (*Op)* are defined as a union of six distinct lexical sets: ARITHMETIC, RELATIONAL, LOGICAL, ASSIGNMENT, PIPELINE, and ARROW.

Let:

| ARITHMETIC | = { +, -, \*, /, %, \\, ^ } |
| :--- | :--- |
| RELATIONAL | = { ==, !=, <=, >=, <, > } |
| LOGICAL | = { !, &&, \|\| } |
| ASSIGNMENT | = { =, +=, -=, \*=, /=, %= } |
| PIPELINE | = { \|\> } |
| TERNARY | = { ? : } |
| ARROW | = { -> } |
| DOT | = { . } |

```
Op = {ARITHMETIC ∪ RELATIONAL ∪ LOGICAL ∪ ASSIGNMENT ∪ PIPELINE
      ∪ ARROW ∪ DOT}
```

#### **Arithmetic**

Performs fundamental mathematical calculations to manipulate numerical data and compute exact quantitative results.

| Operator | Token | Description |
| :---: | :---: | ----- |
| + | ADDITION | Adds two values together to produce their total sum. |
| - | SUBTRACTION | Subtracts the second value from the first to calculate their difference. |
| \* | MULTIPLICATION | Multiplies two values together to calculate their product. |
| / | DIVISION | Divides the first value by the second to return the exact quotient. |
| % | MODULO | Divides two values and returns only the remainder of the operation. |
| \\ | INTEGER\_DIVISION | Divides two values and drops the decimal to return a whole number. |
| ^ | EXPONENT | **Exponent** - Raises a base value to the power of a specified exponent. |

#### **Boolean**

Represents fundamental truth values used to evaluate conditions, drive decision-making logic, and manage application control flow. It is further categorized into Relational and Logical Operators.

##### **Relational**

Compares distinct values or expressions against one another to evaluate their specific relationship as a definitive boolean state.

| Operator | Token | Description |
| :---: | :---: | ----- |
| == | EQUALITY | Evaluates whether two values are strictly equal, returning true if their values match and false otherwise. |
| != | INEQUALITY | Evaluates whether two values are not equal, returning true if their values differ and false if they match. |
| <= | LESS\_EQUAL | Compares two values and returns true if the left operand is less than or equal to the right operand. |
| >= | GREATER\_EQUAL | Compares two values and returns true if the left operand is greater than or equal to the right operand. |
| < | LESS\_THAN | Compares two values and returns true if the left operand is strictly less than the right operand. |
| > | GREATER\_THAN | Compares two values and returns true if the left operand is strictly greater than the right operand. |

##### **Logical**

Combines, evaluates, or inverts boolean conditions to construct complex decision-making logic and conditional pathways.

| Operator | Token | Description |
| :---: | :---: | ----- |
| ! | LOGICAL\_NOT | Negates a boolean condition (NOT) |
| && | LOGICAL\_AND | Combines boolean conditions (AND) |
| \|\| | LOGICAL\_OR | Evaluates multiple boolean conditions (OR) |
| ? : | TERNARY | Ternary Operator. If the condition before ? is true, it returns the value before :. Otherwise, it returns the value after :. |

#### **Assignment**

Binds evaluated expressions to variables and provides efficient shorthand for mutating existing data states in place.

| Operator | Token | Description |
| :---: | :---: | ----- |
| = | ASSIGNMENT | Assigns the value of the right-hand expression to the left-hand variable. |
| += | ADD\_ASSIGN | Adds the right-hand value to the variable and reassigns the result. |
| -= | SUBTRACT\_ASSIGN | Subtracts the right-hand value from the variable and reassigns the result. |
| \*= | MULTIPLY\_ASSIGN | Multiplies the variable by the right-hand value and reassigns the result. |
| /= | DIVIDE\_ASSIGN | Divides the variable by the right-hand value and reassigns the result. |
| %= | MODULO\_ASSIGN | Assigns the variable the remainder of dividing it by the right-hand value. |

#### **Pipeline**

Streamlines functional data flow by passing evaluated results forward to flatten nested function calls into readable sequences.

| Set Element | Token | Description |
| :---: | :---: | ----- |
| \|\> | PIPELINE | Passes the evaluated result of the left-hand expression directly as the first argument to the function on the right side. |

#### **Arrow**

Evaluates variables against multiple potential values to dynamically route control flow and manage complex state transitions.

| Set Element | Token | Description |
| :---: | :---: | ----- |
| -> | ARROW | Acts as a direct case mapper within case matching expressions to route the process to a different state based on the evaluated input. |

#### **Dot**

Accesses intrinsic properties or member attributes associated with composite data types, such as retrieving array length.

| Set Element | Token | Description |
| :---: | :---: | ----- |
| . | DOT | Accesses intrinsic member properties (e.g., array length via `.length`) |

### **Keywords and Reserved Words**

#### **Keywords**

Keywords are action-oriented commands that instruct the compiler to perform a specific operation or dictate the control flow.

| Category | Keyword | Description |
| :---: | :---: | ----- |
| NULL SAFETY | ensure | Acts as a strict safety gateway to unwrap optional variables, mandating a control flow exit on failure. |
| CONTROL FLOW | transition | Routes the state machine by transferring execution to a newly specified state block. |
| | match | Handles exhaustive pattern evaluation, allowing execution to branch based on matching data values. |
| | if | Initiates standard logical branching by executing a block of code only if a provided condition evaluates to true. |
| | else | Provides an alternative execution path for logical branching when an if condition evaluates to false. |
| | for | Handles standard counting and iterative looping over a sequence or range of values. |
| INPUT/OUTPUT | print | A built-in action used to push formatted text or variable values directly to the terminal output. Print accepts two arguments: `print(string_to_print, endline_string)`. `print("Hello");` // prints "Hello\n" (default) `print("Enter PIN: ", "");` // prints "Enter PIN: " (cursor stays on same line) `print("Loading", "... ");` // prints "Loading... " |
| | readLine | Pauses program execution to capture standard string input from the user via the terminal. |
| | readInt | Pauses program execution to safely capture and parse integer input from the user via the terminal. |
| SYSTEM | quit | An action command that instructs the compiled program to terminate its process entirely and exit. |
| | sleep | Pauses the execution thread for a predefined duration, measured in milliseconds. |
| MODULARITY | fn | Used to declare reusable, pure helper functions that execute independently of the main state machine. |
| | return | Sends a computed value back to the caller's execution flow, terminating the current function block. |

*Figure 1: Finite Automaton for the Keywords*

#### **Reserved Words**

Reserved Words hold special architectural or structural meaning within the language. While they define boundaries or represent definitive values, they do not execute actions themselves. These words are protected by the compiler and cannot be used as variable identifiers.

| Category | Reserved Word | Description |
| :---: | :---: | ----- |
| **DATA TYPES** | int | Defines the specific primitive type for a variable intended to store whole numbers. Abstract or user-defined data types are not supported. |
| | string | Defines the specific primitive type for a variable intended to store a sequence of text characters. |
| | bool | Defines the specific primitive type for a variable intended to store *true* or *false* logical states. |
| | float | Defines the specific primitive type for a variable intended to store numerical values with fractional or decimal components. |
| | char | Defines the specific primitive type for a variable intended to store a single alphanumeric character or symbol. |
| **STRUCTURAL** | state | Defines the architectural boundaries of the program by encapsulating a discrete machine mode and its associated logic. |
| | react | Defines a data-driven watcher that triggers automatically when a bound piece of state or data changes. |
| | loop | Defines an infinite, blocking execution context, typically used for main game or event-listening loops. |
| **STATE LITERALS** | true | The definitive positive state value for boolean variables and logical evaluations. |
| | false | The definitive negative state value for boolean variables and logical evaluations. |
| | null | The explicit empty representation used for optional primitive types (e.g., Int?), indicating the deliberate absence of a value. |

*Figure 2: Finite automaton for recognizing reserved words*

### **Noise Words**

Noise words are optional terms injected into the syntax solely to enhance human readability. They hold no semantic or structural value. The lexical analyzer systematically discards these tokens during the tokenization phase, meaning the parser never processes them and they have zero impact on the program's execution logic. This allows developers to write code that reads closer to natural English without complicating the grammar rules of the compiler.

Defined Noise Words:

* **is**  
* **do**  
* **to**

| Programmer Input (With Noise Words) | Equivalent Code |
| :---: | :---: |
| `int roomTemp **is** = 22;` | `int roomTemp = 22;` |
| `state Dashboard **do** { ... }` | `state Dashboard { ... }` |
| `transition **to** Dashboard;` | `transition Dashboard;` |

*Figure 3: Finite automaton for recognizing noise words*

### **Comments**

* **Single-line comments** - Initiated by double forward slashes (//). Lexer ignores all characters following the // until a hard return is encountered.

```
// This is a single-line comment
```

* **Multi-line block comments** - Enclosed within /* and */. The lexer discards all characters between these boundaries, allowing the comment to span multiple lines.

```
/* This is
   a multi-line
   comment */
```

*Figure 4: Finite automaton for recognizing comments*

### **Blanks (spaces)**

Blanks, spaces, tabs (\t), and newlines (\n) are treated purely as token separators. The lexical analyzer completely ignores and discards these whitespaces during the tokenization phase. The only exception is when blanks are enclosed within string literals (e.g., "Hello World"), where they are preserved as part of the token.

### **Delimiters and Brackets**

**Delimiters**

| Symbol | Name | Function / Usage |
| :---: | :---: | ----- |
| ; | **Semicolon (terminator)** | Acts as an optional hard terminator used to separate and conclude individual statements. |
| , | **Comma (separator)** | Acts as a soft separator used to separate parameters and arguments. |
| : | **Colon (type annotator)** | Acts as a type annotator used to separate an identifier from its declared type. |

*Figure 5: Finite automaton for recognizing terminators (semicolon)*

*Figure 6: Finite Automaton for recognizing separators (comma)*

*Figure 7: Finite Automaton for recognizing type annotator (Colon)*

**Brackets**

| Symbol | Name | Function / Usage |
| :---: | :---: | ----- |
| () | **Parentheses** | Used for mathematical expression groupings and enclosing function arguments. |
| {} | **Curly Braces** | Groups multiple statements together to define scope blocks for functions, loops, and conditional statements. |
| \[\] | **Square Brackets** | Used for array indexing and list declaration. |

*Figure 8: Pushdown Automaton of the Brackets*

Transition Notation: $a,X\rightarrow \gamma$  
Where:  
$a$ = input symbol  
$X$ = symbol popped from the top of the stack  
$\gamma$ = string of symbols pushed onto the stack  
$\epsilon$ = empty string (no input consumed)  
${Z}_{0}$ = bottom-of the stack

### **Free-and-fixed-field formats**

The program will use a free-form format. Delimiters will be used to separate the statements. This means the programmer won't have to worry about strict rules for indentation or line breaks breaking your code. Whether you prefer to space things out over multiple lines to make a complex calculation easier to read, or you just want to stack a few quick commands on a single line, the compiler won't mind. As long as you drop that delimiter at the end of your instruction, the parser will know exactly what you mean, giving you the complete freedom to format your work however it makes the most sense to you.

### **Expression**

An expression is any valid combination of variables, literals (constants), operation symbols, and function calls that the compiler evaluates to produce a single value. Since this language is strictly typed, every expression resolves to a specific primitive data type (Int, Float, Boolean, Char, String).

#### **Types of Expressions:**

1. #### **Arithmetic Expressions**

Combine numbers and arithmetic operators, evaluating to an int or float. This includes standard division (/) which yields a float, and integer division (\\), which divides two numbers and truncates the decimal remainder to return a strict int.

* **Example:** `(price * 0.2) + 5`  
* **Example:** `10 \ 3` (Evaluates to 3)

2. #### **Relational and Logical Expressions**

Combine values with comparison or boolean operators, evaluating strictly to a Boolean (*true* or *false*).

* **Example:** `score >= 50 && !is_absent`

3. #### **Pipeline Expressions**

Chains function from left to right. The output of the left side automatically becomes the input for the function on the right, avoiding messy, nested code.

* **Example:** Instead of `print(uppercase(trim("hello ")))`, we can write it instead as `"hello " |> trim() |> uppercase() |> print()`

4. #### **Array Access Expressions**

Retrieves a specific element from an array using its 0-based numeric index enclosed in square brackets `[]`. This expression evaluates to the underlying data type of the array.  
Given: `[int] scores = [90, 85, 88]`

* **Example:** `scores[0]` *(Evaluates to 90)*

#### **Evaluation Rules**

To ensure that mathematical logic behaves in a standard mathematical order of operations, the compiler will evaluate expressions using PEMDAS or EMDAS (Parenthesis, Exponents, Multiplication, Division, Addition, Subtraction).

#### **Operator Precedence Table**

Operations are evaluated from highest precedence (Level 1) to lowest (Level 8). Operators on the same level are evaluated based on their associativity. (Note: Exponents are evaluated Right-to-Left to align with standard mathematical rules, e.g., 2^3^2 evaluates as 2^(3^2)).

| Precedence Level | Operator Type | Symbols | Associativity |
| :---: | :---: | :---: | :---: |
| 1 (Highest) | Primary (Parentheses and Dot) | ( ), . | Left-to-Right |
| 2 | Exponent | ^ | Right-to-Left |
| 3 | Logical NOT | ! | Right-to-Left |
| 4 | Multiplicative | \*, /, \\, % | Left-to-Right |
| 5 | Additive | +, - | Left-to-Right |
| 6 | Relational | <, >, <=, >= | Left-to-Right |
| 7 | Equality | ==, != | Left-to-Right |
| 8 | Logical AND | && | Left-to-Right |
| 9 | Logical OR | \|\| | Left-to-Right |
| 10 | Ternary Conditional | ? : | Right-to-Left |
| 11 (Lowest) | Pipeline Operator | \|\> | Left-to-Right |

### **Data Types**

1. #### **Primitive Data Types**

Defines the fundamental, strictly typed building blocks used to store basic scalar values and enforce memory safety throughout the program.

| Data Type | Keyword | Description |
| :---: | :---: | ----- |
| Integer | int | Stores whole numbers without decimals. Typically used for counting, indexing, and discrete math. Example: `int roomTemp = 22;` |
| Floating-Point | float | Stores numbers with fractional (decimal) parts. Replaces "Decimal" to align with standard industry syntax. Example: `float weight = 150.5;` |
| Character | char | Stores a single alphanumeric character or symbol, enclosed in single quotes. Example: `char grade = 'A';` |
| Boolean | bool | Stores a true or false value. Used exclusively for logical operations and conditional control flow. Example: `bool is_valid = true;` |

2. #### **Abstract & Composite Data Types**

Provides complex data structures designed to manage ordered sequences of text and group multiple elements of the same type into accessible collections.

| Data Type | Keyword | Description |
| :---: | :---: | ----- |
| String | string | An ADT representing a sequence of text characters. Used heavily for I/O and terminal parsing. Example: `string? tempInput = readLine();` |
| Array | \[\] | An ordered collection of elements sharing the same data type. Fidelis supports three distinct declarative syntaxes to handle dynamic inference and fixed memory allocation safely: **Inferred Size** - The size is automatically inferred from the provided array literal. `[int] scores = [90, 85, 88];` **Fixed Size** - Allocates a fixed block of memory with uninitialized value having 0. `[int, 10] scores;` // Empty `[int, 10] scores = [90, 85, 88];` // Initialized |

3. #### **Optional Data Types**

Handles unsure data and prevents runtime crashes by explicitly allowing any data type to represent the deliberate absence of a value (null). Optional data types cannot be used directly in standard operations. To use them, they must first be explicitly unwrapped into strict primitives using the ensure, which mandates a control flow exit on failure to guarantee that null or invalid data cannot propagate further into other state blocks.

| Data Type | Keyword | Description |
| :---: | :---: | ----- |
| Optional Integer | int? | Stores a whole number or a null value. Natively returned by safe extractors like readInt() to prevent crashes from invalid terminal input. Example: `int? pin = readInt();` |
| Optional Floating-Point | float? | Stores a decimal number or a null value, ensuring safe extraction and handling of fractional data. Example: `float? weight = null;` |
| Optional Character | char? | Stores a single alphanumeric character or a null value. Example: `char? initial = null;` |
| Optional Boolean | bool? | Stores a true or false logical state, or a null value for uninitialized conditions. Example: `bool? is_valid = null;` |
| Optional String | string? | Stores a sequence of text characters or a null value. Natively returned by functions like readLine() to handle inherently unsafe standard input safely. Example: `string? tempInput = readLine();` |
| Optional Array | \[type\]? | Stores an ordered collection of elements or a null value. Example: `[int]? scores = null;` |

### **Statements**

1. #### **Declaration Statements**

Declaration statements allocate memory and define the strict type of a variable. The compiler binds variable declarations into strict primitives to make the language type safe.

```fidelis
// Variable Declaration
int roomTemp = 22;
int? input = null;
[string] accessLogs = ["System Booted", "Thermostat Initialized"];

// Function Declaration
fn formatStatus(bool status) : string {
   return (status == true) ? "ACTIVE" : "INACTIVE";
}
```

2. #### **Input Statements**

Input statements handle the retrieval of data from the terminal prompt. Because standard input is inherently unsafe, these statements natively return Optional types (e.g., `Int?`, `String?`) to prevent crashes when a user enters unexpected characters.

```fidelis
string tempInput = readLine();
int? pin = readInt();
```

3. #### **Output Statements**

Output statements push formatted text directly to the console display. This unified approach handles standard strings alongside primitive concatenation.

```fidelis
print("--- IOT MAIN MENU ---");
print("Current temperature is " + roomTemp + "C.");
```

4. #### **Assignment Statements**

Assignment statements update the value of an already declared mutable variable. Crucially, updating a global variable via an assignment operator (`=`) automatically triggers any `react` blocks bound to that specific identifier, decoupling business logic from presentation logic.

```fidelis
lightsOn = true;
roomTemp = 5;
scores[1] = 95; // Updates the second element in the array
```

5. #### **Condition Statements**

Condition statements branch the logic of the program based on real-time evaluations. The language supports standard `if/else` blocks (which require parentheses around the condition) for straightforward boolean logic, alongside single-line `if` statements for concise checks.

##### **If Statement**

```fidelis
// If
if (lightsOn == true) {
   print("[SYSTEM EVENT] Living room lights illuminated.");
}
```

##### **If-Else Statement**

```fidelis
// If/else
if (lightsOn == true) {
   print("[SYSTEM EVENT] Living room lights illuminated.");
} else {
   print("[SYSTEM EVENT] Living room lights extinguished.");
}
```

**Ensure Typing**

Handling optional data traditionally requires standard `if` statements to evaluate null conditions. However, relying on generic conditional branches for data validation introduces structural inefficiencies. First, the primary execution flow becomes wrapped within nested scopes which increases the cyclomatic complexity and diminishes the readability. Second, standard conditional blocks lack enforced termination where they do not mandate a state exit upon validation failure. This deficiency permits invalid or null data to propagate silently, potentially compromising the integrity of the state machine.

```fidelis
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

To enforce strict null safety and avoid deeply nested control structures, Fidelis introduces the `ensure` gateway. This keyword safely unwraps Optional types into strict primitives while maintaining a flattened execution scope. Crucially, it imposes a strict architectural contract: the `else` block must culminate in a definitive control flow exit (such as a `transition` or `return`). By mandating this termination at compile-time, the language guarantees that unhandled null states cannot propagate into subsequent computational blocks.

```fidelis
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

##### **Case Matching**

The `match` expression provides an advanced, scalable alternative to deeply nested `if/else` statements for exhaustive input routing. It evaluates a single variable against multiple potential values, using an arrow (`->`) to immediately direct the control flow.

For lightweight routing, developers can use single-line arrows. But when routing requires intermediate steps, such as variable mutation or logging before a state transition, `match` supports multi-statement scopes enclosed in curly braces. The underscore (`_`) is used as the required default fallback case to catch unhandled inputs.

##### **Case Matching for single-line statements**

```fidelis
// Case Matching
match choice {
   "1" -> transition to LightControl
   "2" -> transition to TempControl
   ""  -> print("")
   _   -> print("Unknown command.")
}
```

##### **Case Matching with blocks**

```fidelis
// Case Matching with blocks
match choice {
   "1" -> transition to LightControl
   "2" -> {
       print("Initializing climate interface...");
       transition to TempControl;
   }
   "3" -> transition to StatusView
   _   -> {
       invalidAttempts += 1;
       print("Unknown command. Attempts: {invalidAttempts}");
   }
}
```

6. #### **Iterative Statements**

Iterative statements execute a block of code multiple times. The language utilizes explicit loop blocks for infinite, blocking terminal prompts that wait for user input, alongside standard for loops for fixed sequential numeric counting.

##### **Infinite Loop**

```fidelis
// Infinite Blocking Loop
loop {
  print("Select an action:");
  string choice = readLine();
  // State transition breaks the loop
  transition to TempControl;
}
```

##### **For Loop**

```fidelis
// Iterating through an array using an index
[int] scores = [90, 85, 88];

for (int i = 0; i < scores.length; i += 1) {
 print("Score " + (i + 1) + " is: " + scores[i]);
}
```

##### **Nested Loops**

You can place one loop inside another to create a nested loop. The inner loop will execute all of its iterations completely for every single iteration of the outer loop. This is especially useful for processing multidimensional concepts, such as grids or coordinates, or executing a sequential task during an infinite main loop.

```fidelis
// Nested Iteration
for (int x = 1; x <= 3; x += 1) {
   for (int y = 1; y <= 2; y += 1) {
       print("Processing coordinate: (" + x + ", " + y + ")");
   }
}
```

### **Built-in Intrinsics**

Fidelis provides built-in functions for common operations on strings and arrays. These functions use the same call syntax as user-defined functions; their names are not keywords or reserved words.

#### **replace**

Returns a new string made by replacing every non-overlapping occurrence of search in source with replacement. Matching is case-sensitive, and the source string is not modified. If the search is empty, the result is source unchanged. All three arguments must be non-null strings.

**Function Signature:**  
`replace(string source, string search, string replacement) : string`

**Example:**  
`string normalized = replace("Ada Lovelace", " ", ""); // "AdaLovelace"`

**Contribution Matrix**

| | Bulala, Kenneth | Caadan, Psalm | Cilon, Rachel | Dukha, James | Inso, Eliazar | Rodriguez, Jan | Singson, Justin |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| | 8 | 9 | 10 | 6 | 10 | 9 | 9 |
| | 10 | 9 | 10 | 10 | 10 | 10 | 10 |
| | 5 | 10 | 8 | 5 | 10 | 8 | 8 |
| | 10 | 10 | 10 | 6 | 10 | 10 | 10 |
| | 8 | 10 | 10 | 10 | 10 | 10 | 10 |
| | 10 | 9 | 10 | 4 | 10 | 10 | 10 |
| **Total** | **51** | **57** | **58** | **41** | **60** | **57** | **57** |
| **Ave.** | **8.5** | **9.5** | **9.67** | **6.83** | **10** | **9.5** | **9.5** |

### **EBNF Grammar Rules**

```
Program = { Statement } ;
Statement = BlockStatement | ( SimpleStatement, ";" ) ;
BlockStatement = ReactDecl | FnDecl | StateDecl | IfStmt | LoopStmt | ForStmt | MatchStmt | EnsureStmt ;
Block = "{", { Statement }, "}" ;

ReactDecl = "react", "(", Identifier, ")", Block ;
FnDecl = "fn", Identifier, "(", [ParamList], ")", ":", Type, Block ;
StateDecl = "state", Identifier, Block ;

IfStmt = "if", "(", Expression, ")", ( Statement | Block ),
         [ "else", ( Statement | Block ) ] ;

LoopStmt = "loop", Block ;
ForStmt = "for", "(", Type, Identifier, "=", Expression, ";", Expression, ";", Assignment, ")", Block ;

MatchStmt = "match", ( "(", Expression, ")" | Identifier ), "{", { MatchCase }, "}" ;
MatchCase = MatchPattern, "->", ( Block | SimpleStatement, ";" ) ;
MatchPattern = [ "+" | "-" ], ( IntLiteral | FloatLiteral ) | Literal | "_" ;

EnsureStmt = [ Type ], Identifier, "=", "ensure", Expression, "else", Block ;

SystemStmt = "quit" | "sleep", "(", Expression, ")";

(* Simple Statements *)

SimpleStatement = VarDecl | Assignment | PrintStmt | TransitionStmt | ReturnStmt | SystemStmt | ExprStmt ;
VarDecl = Type, Identifier, [ "=", Expression ] ;
Assignment = PostfixExpr, AssignOp, Expression ;
PrintStmt = "print", "(", Expression, [ ",", Expression ], ")" ;
TransitionStmt = "transition", Identifier ;
ReturnStmt = "return", Expression ;
ExprStmt = Expression ;

(* Types and Identifiers *)

Type = "int" | "float" | "string" | "bool" | "char"
     | "int?" | "float?" | "string?" | "bool?" | "char?"
     | "[", Type, [ ",", IntLiteral ], "]", [ "?" ] ;
ParamList = Type, Identifier, { ",", Type, Identifier } ;
Identifier = ( Letter | "_" ), { Letter | Digit | "_" } ;
(* Constraint: Identifier length must be <= 32 characters. *)

(* Expressions & Operator Precedence *)

(* Level 11: Pipeline (Left-to-Right) *)
Expression = PipelineExpr;
PipelineTarget = FunctionCall | ( "print", "(", [ ArgList ], ")" ) ;
PipelineExpr = TernaryExpr, { "|>", PipelineTarget } ;

(* Level 10: Ternary Conditional (Right-to-Left) *)
TernaryExpr = LogicalOrExpr, [ "?", Expression, ":", TernaryExpr ] ;

(* Level 9: Logical OR (Left-to-Right) *)
LogicalOrExpr = LogicalAndExpr, { "||", LogicalAndExpr };

(* Level 8: Logical AND (Left-to-Right) *)
LogicalAndExpr = EqualityExpr, { "&&", EqualityExpr };

(* Level 7: Equality (Left-to-Right) *)
EqualityExpr = RelationalExpr, { ( "==" | "!=" ), RelationalExpr } ;

(* Level 6: Relational (Left-to-Right) *)
RelationalExpr = AdditiveExpr, { ( "<" | ">" | "<=" | ">=" ), AdditiveExpr } ;

(* Level 5: Additive (Left-to-Right) *)
AdditiveExpr = MultiplicativeExpr, { ( "+" | "-" ), MultiplicativeExpr } ;

(* Level 4: Multiplicative (Left-to-Right) *)
MultiplicativeExpr = UnaryExpr, { ( "*" | "/" | "\" | "%" ), UnaryExpr } ;

(* Level 3: Unary (Right-to-Left) *)
UnaryExpr = { "!" | "-" }, ExponentExpr ;

(* Level 2: Exponent (Right-to-Left Associativity) *)
ExponentExpr = PostfixExpr, [ "^", ExponentExpr ] ;

(* Level 1.5: Postfix Access (Left-to-Right Associativity) *)
PostfixExpr = PrimaryExpr, { "[", Expression, "]" | ".", "length" | "(", [ ArgList ], ")" } ;

(* Level 1: Parentheses, Grouping, & Access *)
InputExpr = ("readLine" | "readInt"), "(", ")";
PrimaryExpr = Identifier | Literal | InputExpr | "(", Expression, ")" ;

FunctionCall = Identifier, "(", [ ArgList ], ")" ;
ArgList = Expression, { ",", Expression } ;

(* Terminals & Lexical Primitives *)

AssignOp = "=" | "+=" | "-=" | "*=" | "/=" | "%=" ;
Literal = IntLiteral | FloatLiteral | CharLiteral | StringLiteral | BoolLiteral | NullLiteral | ArrayLiteral ;
BoolLiteral = "true" | "false" ;
NullLiteral = "null" ;
IntLiteral = Digit, { Digit } ;
FloatLiteral = Digit, { Digit }, ".", Digit, { Digit } ;
StringLiteral = '"', { ? any character except '"' ? }, '"' ;
CharLiteral = "'", ? any single character "'" ?, "'" ;
ArrayLiteral = "[", [ ArgList ], "]" ;
Letter = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L" | "M" |
         "N" | "O" | "P" | "Q" | "R" | "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z" |
         "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j" | "k" | "l" | "m" |
         "n" | "o" | "p" | "q" | "r" | "s" | "t" | "u" | "v" | "w" | "x" | "y" | "z" ;
Digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;