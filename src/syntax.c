#include "syntax.h"
#include "helper.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// ============================================================================
//                                EBNF MACROS
// ============================================================================
// Guard at the start of every production rule
#define GUARD(p) \
    do { if ((p) == NULL || !(p)->hasMoreTokens) return false; } while (0)

// Require a terminal token, or report error and return false
#define EXPECT(tok, msg) \
    do { if (!expectToken(parser, (tok), (msg))) return false; } while (0)

#define EXPECT_LOG(tok, label, msg) \
    do { if (!expectTokenLogged(parser, (tok), (label), (msg))) return false; } while (0)

// Expect a terminal or immediately run panic recovery
#define EXPECT_SYNC(tok, msg, ctx, ...) \
    do { \
        if (!matchToken(parser, (tok))) { \
            reportError(parser, (msg)); \
            panicRecovery(parser, TOKEN_LIST(__VA_ARGS__), TOKEN_COUNT(__VA_ARGS__), (ctx)); \
            return false; \
        } \
    } while (0)

// Require a non-terminal production, or propagate failure
#define REQUIRE(call) \
    do { if (!(call)) return false; } while (0)

// Helper: ( Block | Statement )
static bool parseBlockOrStmt(Parser *parser) {
    return checkToken(parser, "LBRACE") ? parseBlock(parser) : parseStatement(parser);
}

// Variadic helpers - no more manual arrays or magic element counts
#define TOKEN_LIST(...)  ((const char *[]){ __VA_ARGS__ })
#define TOKEN_COUNT(...) ((int)(sizeof(TOKEN_LIST(__VA_ARGS__)) / sizeof(const char *)))

// EBNF primitives
#define CHECK_ANY(...)   checkTokenAny(parser, TOKEN_LIST(__VA_ARGS__), TOKEN_COUNT(__VA_ARGS__))
#define OPT(tok)         matchToken(parser, (tok))       // EBNF: [ "tok" ]
#define OPT_NOISE(word)  matchNoise(parser, (word))      // EBNF: [ "do" | "is" | "to" ]

// ============================================================================
//                             EBNF HELPER FUNCTIONS
// ============================================================================
typedef bool (*ParseFn)(Parser *);

typedef struct {
    const char *token;
    ParseFn fn;
} TokenRule;

static ParseFn lookupRule(Parser *parser, const TokenRule *rules, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (checkToken(parser, rules[i].token)) return rules[i].fn;
    }
    return NULL;
}

// Matches an optional noise word like "do", "is", or "to"
static bool matchNoise(Parser *parser, const char *word) {
    if (checkToken(parser, "NOISE_WORD") &&
        strcasecmp(parser->currentToken.lexeme, word) == 0) {
        advanceToken(parser);
        return true;
    }
    return false;
}

// Checks if the current token can start a Type (int, float, string, bool, char, '[')
static bool isTypeStart(Parser *parser) {
    static const char *types[] = {"INT", "FLOAT", "STRING", "BOOL", "CHAR", "LBRACKET"};
    return checkTokenAny(parser, types, 6);
}

// Single-token expect helper with error reporting
static bool expectToken(Parser *parser, const char *expected, const char *errorMsg) {
    if (matchToken(parser, expected)) {
        return true;
    }
    reportError(parser, errorMsg);
    return false;
}

static bool parseLeftAssoc(
    Parser *parser,
    bool (*parseOperand)(Parser *),
    const char *ops[],
    int opCount,
    const char *label
) {
    GUARD(parser);
    REQUIRE(parseOperand(parser));
    while (checkTokenAny(parser, ops, opCount)) {
        fprintf(parser->output, "        %s: %s\n", label, parser->currentToken.lexeme);
        advanceToken(parser);
        REQUIRE(parseOperand(parser));
    }
    return true;
}

// Expect a terminal, log its lexeme BEFORE advancing (fixes the double-advance bug)
static bool expectTokenLogged(Parser *parser, const char *expected, const char *label, const char *errMsg) {
    if (!checkToken(parser, expected)) {
        reportError(parser, errMsg);
        return false;
    }
    fprintf(parser->output, "    %s: %s\n", label, parser->currentToken.lexeme);
    advanceToken(parser);
    return true;
}

// Left-associative binary precedence level: EBNF  Next, { (op1 | op2), Next }
#define LEFT_ASSOC(operandFn, label, ...) \
    parseLeftAssoc(parser, (operandFn), TOKEN_LIST(__VA_ARGS__), TOKEN_COUNT(__VA_ARGS__), (label))

// ============================================================================
//                     RECURSIVE DESCENT PARSING FUNCTIONS
// ============================================================================

// =============================================================================
// TOP-LEVEL & DISPATCH (Program, Statement, BlockStatement, SimpleStatement)
// =============================================================================
// ----------------------------------------------------------------------------
// PROGRAM
// 
// Program = { Statement } ;
//
// The top-level production. Parses statements until EOF is reached.
// Returns true if the entire program was parsed without errors.
// ----------------------------------------------------------------------------
bool parseProgram(Parser *parser) {
    if (parser == NULL) return false;

    fprintf(parser->output, "─── PROGRAM ─────────────────────────────────────────────────────\n");

    // Parse statements until EOF
    while (parser->hasMoreTokens) {
        if (checkToken(parser, "EOF")) {
            fprintf(parser->output, "─── END OF PROGRAM ───────────────────────────────────────────────\n");
            return !parser->encounteredError;
        }

        if (!parseStatement(parser)) {
            const char *sync[] = {"SEMICOLON", "RBRACE", "EOF"};
            panicRecovery(parser, sync, 3, "top-level statement");

            // Guarantee forward progress if recovery stopped on a top-level ';' or stray '}'
            if (CHECK_ANY("SEMICOLON", "RBRACE")) {
                advanceToken(parser);
            }
        }
    }

    fprintf(parser->output, "─── END OF PROGRAM ───────────────────────────────────────────────\n");
    return !parser->encounteredError;
}

// ----------------------------------------------------------------------------
// STATEMENT
// 
// Statement = BlockStatement | ( SimpleStatement, ";" ) ;
//
// Block statements are self-delimiting (they contain their own braces).
// Simple statements must be followed by a semicolon.
// ----------------------------------------------------------------------------
bool parseStatement(Parser *parser) {
    GUARD(parser);

    // Empty statement: just consume the single ';' and finish
    if (OPT("SEMICOLON")) return true;
    // Distinguish between a variable declaration and an ensure statement
    if (isTypeStart(parser)) return parseDeclOrEnsureStmt(parser);

    if (CHECK_ANY("IF", "LOOP", "FOR", "MATCH", "STATE", "REACT", "FN")) {
        return parseBlockStatement(parser);
    }

    REQUIRE(parseSimpleStatement(parser));
    EXPECT_SYNC("SEMICOLON", "Expected ';' after statement.",
                "statement termination", "SEMICOLON", "RBRACE", "EOF");
    return true;
}

// ----------------------------------------------------------------------------
// BLOCK STATEMENT
// 
// BlockStatement = IfStmt | LoopStmt | ForStmt | MatchStmt | EnsureStmt
//                 | StateDecl | ReactDecl | FnDecl ;
// ----------------------------------------------------------------------------
bool parseBlockStatement(Parser *parser) {
    GUARD(parser);
    static const TokenRule rules[] = {
        {"IF", parseIfStmt},         {"LOOP", parseLoopStmt},
        {"FOR", parseForStmt},       {"MATCH", parseMatchStmt},
        {"STATE", parseStateDecl},
        {"REACT", parseReactDecl},   {"FN", parseFnDecl},
    };

    ParseFn fn = lookupRule(parser, rules, sizeof(rules) / sizeof(rules[0]));
    if (fn != NULL) return fn(parser);

    reportError(parser, "Expected a block statement keyword (if, loop, for, match, ensure, state, react, or fn).");
    return false;
}

// ----------------------------------------------------------------------------
// SIMPLE STATEMENT
// 
// SimpleStatement = VarDecl | Assignment | PrintStmt | TransitionStmt
//                  | ReturnStmt | ExprStmt ;
// ----------------------------------------------------------------------------
bool parseSimpleStatement(Parser *parser) {
    GUARD(parser);

    if (isTypeStart(parser)) return parseVarDecl(parser);

    static const TokenRule rules[] = {
        {"PRINT",      parsePrintStmt},
        {"TRANSITION", parseTransitionStmt},
        {"RETURN",     parseReturnStmt},
        {"SLEEP",      parseSleepStmt},
        {"IDENTIFIER", parseIdentStmt},
    };
    ParseFn fn = lookupRule(parser, rules, sizeof(rules) / sizeof(rules[0]));
    if (fn != NULL) return fn(parser);

    if (OPT("QUIT") || OPT("NOISE_WORD")) return true;

    if (CHECK_ANY("INT_LITERAL", "FLOAT_LITERAL", "STRING_LITERAL", "CHAR_LITERAL",
                  "TRUE", "FALSE", "NULL", "LPAREN", "LOGICAL_NOT", "SUBTRACTION",
                  "READLINE", "READINT")) {
        return parseExpression(parser);
    }

    reportError(parser, "Unexpected token. Expected a declaration, assignment, "
                        "'print', 'transition', 'return', 'sleep', 'quit', or expression.");
    return false;
}

// ----------------------------------------------------------------------------
// SLEEP STATEMENT
// 
// SleepStmt = "sleep", "(", Expression, ")" ;
// ----------------------------------------------------------------------------
bool parseSleepStmt(Parser *parser) {
    GUARD(parser);
    EXPECT("SLEEP",  "Expected 'sleep' keyword.");
    EXPECT("LPAREN", "Expected '(' after 'sleep'. Usage: sleep(milliseconds);");
    REQUIRE(parseExpression(parser));
    EXPECT("RPAREN", "Expected ')' after sleep argument.");
    return true;
}

// =============================================================================
// DECLARATIONS (VarDecl / EnsureStmt, StateDecl, ReactDecl, FnDecl, ParamList, Type)
// =============================================================================

// ----------------------------------------------------------------------------
// DECLARATION STATEMENT
// 
// VarDecl = Type, Identifier, ["is"], "=", Expression ;
//
// Handles all 3 forms:
//   1. int x = 42;           (const value)
//   2. int x = y;            (variable assignment)
//   3. int x = a + b * 2;    (expression assignment)
// ----------------------------------------------------------------------------

// Shared prefix: Type, Identifier, [ "is" ], "="
static bool parseDeclPrefix(Parser *parser) {
    REQUIRE(parseType(parser));
    EXPECT_LOG("IDENTIFIER", "Variable", "Expected a variable name after type.");
    OPT_NOISE("is");
    EXPECT("ASSIGNMENT", "Expected '=' in variable declaration.");
    return true;
}

// Handles both top-level/block declarations:
//   VarDecl    = Type, Identifier, [ "is" ], "=", Expression, ";" ;
//   EnsureStmt = Type, Identifier, [ "is" ], "=", "ensure", Expression, "else", Block ;
bool parseDeclOrEnsureStmt(Parser *parser) {
    GUARD(parser);
    REQUIRE(parseDeclPrefix(parser));

    if (OPT("ENSURE")) {
        REQUIRE(parseExpression(parser));
        EXPECT("ELSE", "Expected 'else' in ensure statement.");
        return parseBlock(parser);
    }

    REQUIRE(parseExpression(parser));
    EXPECT_SYNC("SEMICOLON", "Expected ';' after variable declaration.",
                "declaration termination", "SEMICOLON", "RBRACE", "EOF");
    return true;
}

// VarDecl (used inside match cases where the caller consumes ';')
bool parseVarDecl(Parser *parser) {
    GUARD(parser);
    REQUIRE(parseDeclPrefix(parser));
    REQUIRE(parseExpression(parser));
    return true;
}

// --------------------------------------------------------------------------
// BLOCK
// 
// Block = "{", { Statement }, "}" ;
// --------------------------------------------------------------------------
bool parseBlock(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── BLOCK ─────────────────────────────────────────────────────\n");

    EXPECT("LBRACE", "Expected '{' to start a block.");

    // Parse statements inside the block
    while (parser->hasMoreTokens && !checkToken(parser, "RBRACE") && !checkToken(parser, "EOF")) {
        if (!parseStatement(parser)) {
            // Error recovery inside block
            const char *sync[] = {"SEMICOLON", "RBRACE", "EOF"};
            panicRecovery(parser, sync, 3, "block statement");
        }
    }

    EXPECT("RBRACE", "Expected '}' to close the block. "
                     "Every opening '{' must have a matching '}'.");

    fprintf(parser->output, "    ── END BLOCK ──────────────────────────────────────────────────\n");
    return true;
}

// --------------------------------------------------------------------------
// STATE DECLARATION
// 
// StateDecl = "state", Identifier, ["do"], Block ;
// --------------------------------------------------------------------------
bool parseStateDecl(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── STATE DECLARATION ──────────────────────────────────────────\n");

    EXPECT("STATE", "Expected 'state' keyword. Syntax: state StateName { ... }");
    EXPECT_LOG("IDENTIFIER", "State name", "Expected a state name (identifier) after 'state'. "
                         "Syntax: state StateName { ... }");
    OPT_NOISE("do");
    REQUIRE(parseBlock(parser));

    fprintf(parser->output, "    ── END STATE DECLARATION ───────────────────────────────────────\n");
    return true;
}

// --------------------------------------------------------------------------
// REACT DECLARATION
// 
// ReactDecl = "react", "(", Identifier, ")", ["do"], Block ;
// --------------------------------------------------------------------------
bool parseReactDecl(Parser *parser) {
    GUARD(parser);
    fprintf(parser->output, "    ── REACT DECLARATION ──────────────────────────────────────────\n");

    EXPECT("REACT",  "Expected 'react' keyword. Syntax: react (variable) { ... }");
    EXPECT("LPAREN", "Expected '(' after 'react'. Syntax: react (variable) { ... }");
    EXPECT_LOG("IDENTIFIER", "Watching", "Expected a variable name inside react().");
    EXPECT("RPAREN", "Expected ')' after react variable.");
    OPT_NOISE("do");
    REQUIRE(parseBlock(parser));

    fprintf(parser->output, "    ── END REACT DECLARATION ───────────────────────────────────────\n");
    return true;
}

// --------------------------------------------------------------------------
// FUNCTION DECLARATION
// 
// FnDecl = "fn", Identifier, "(", [ParamList], ")", ":", Type, ["do"], Block ;
// --------------------------------------------------------------------------
bool parseFnDecl(Parser *parser) {
    GUARD(parser);
    fprintf(parser->output, "    ── FUNCTION DECLARATION ──\n");

    EXPECT("FN",         "Expected 'fn' keyword.");
    EXPECT("IDENTIFIER", "Expected a function name after 'fn'.");
    EXPECT("LPAREN",     "Expected '(' after function name.");
    if (!checkToken(parser, "RPAREN")) {
        REQUIRE(parseParamList(parser));
    }
    EXPECT("RPAREN",     "Expected ')' after function parameters.");
    EXPECT("COLON",      "Expected ':' before return type.");
    REQUIRE(parseType(parser));
    matchNoise(parser, "do");
    REQUIRE(parseBlock(parser));

    return true;
}

// --------------------------------------------------------------------------
// PARAMETER LIST
// 
// ParamList = Type, Identifier, { ",", Type, Identifier } ;
// --------------------------------------------------------------------------
bool parseParamList(Parser *parser) {
    GUARD(parser);
    fprintf(parser->output, "      Parameters:\n");
    do {
        REQUIRE(parseType(parser));
        EXPECT_LOG("IDENTIFIER", "Param", "Expected a parameter name after type.");
    } while (OPT("COMMA"));
    return true;
}

// --------------------------------------------------------------------------
// ARGUMENT LIST
// 
// ArgList = Expression, { ",", Expression } ;
// --------------------------------------------------------------------------
bool parseArgList(Parser *parser) {
    GUARD(parser);
    int argCount = 0;
    do {
        REQUIRE(parseExpression(parser));
        argCount++;
    } while (OPT("COMMA"));
    fprintf(parser->output, "      Arguments: %d\n", argCount);
    return true;
}

// --------------------------------------------------------------------------
// TYPE
// 
// Type = "int" | "float" | "string" | "bool" | "char"
//      | "int?" | "float?" | "string?" | "bool?" | "char?"
//      | "[", Type, "]", ["?"] ;
// --------------------------------------------------------------------------
bool parseType(Parser *parser) {
    GUARD(parser);

    if (OPT("LBRACKET")) {
        REQUIRE(parseType(parser));
        EXPECT("RBRACKET", "Expected ']' to close array type.");
        OPT("QUESTION");
        return true;
    }

    if (CHECK_ANY("INT", "FLOAT", "STRING", "BOOL", "CHAR")) {
        fprintf(parser->output, "      Type: %s", parser->currentToken.lexeme);
        advanceToken(parser);
        if (OPT("QUESTION")) fprintf(parser->output, "?");
        fprintf(parser->output, "\n");
        return true;
    }

    reportError(parser, "Expected a type keyword (int, float, string, bool, char) or an array type ([type]).");
    return false;
}

// ===========================================================================
// STATEMENTS (Assignment, Print, If, Loop, For, Match, Transition, Return, Block)
// ===========================================================================

// ----------------------------------------------------------------------------
// ASSIGNMENT STATEMENT
// 
// Assignment = Identifier, AssignOp, Expression ;
//
// Handles all 3 forms:
//   1. x = y;           (variable to variable)
//   2. x = 42;          (const value to variable)
//   3. x = a + b * 2;   (expression to variable)
//
// Also handles compound assignments: +=, -=, *=, /=, %=
// ----------------------------------------------------------------------------
// Parses the rest of an assignment after the leading IDENTIFIER is consumed:
// [ "[", Expression, "]" ], AssignOp, Expression
static bool parseAssignmentTail(Parser *parser) {
    if (OPT("LBRACKET")) {
        REQUIRE(parseExpression(parser));
        EXPECT("RBRACKET", "Expected ']' after array index. Syntax: arrayName[index] = value;");
    }

    if (!CHECK_ANY("ASSIGNMENT", "ADD_ASSIGN", "SUBTRACT_ASSIGN",
                   "MULTIPLY_ASSIGN", "DIVIDE_ASSIGN", "MODULO_ASSIGN")) {
        reportError(parser, "Expected an assignment operator (=, +=, -=, *=, /=, %=).");
        const char *sync[] = {"SEMICOLON", "RBRACE", "EOF"};
        panicRecovery(parser, sync, 3, "assignment operator");
        return false;
    }

    fprintf(parser->output, "    Operator: %s\n", parser->currentToken.lexeme);
    advanceToken(parser); // consume assignment operator

    REQUIRE(parseExpression(parser));
    return true;
}

// Used directly by parseForStmt: Identifier, [ "[", Expr, "]" ], AssignOp, Expr
bool parseAssignment(Parser *parser) {
    GUARD(parser);
    fprintf(parser->output, "    ── ASSIGNMENT ───────────────────────────────────────────────\n");
    EXPECT_LOG("IDENTIFIER", "Target", "Expected an identifier on the left side of assignment.");
    REQUIRE(parseAssignmentTail(parser));
    fprintf(parser->output, "    ── END ASSIGNMENT ────────────────────────────────────────────\n");
    return true;
}

// Used by parseSimpleStatement: handles both `func(args)` and `var = expr`
bool parseIdentStmt(Parser *parser) {
    GUARD(parser);
    EXPECT_LOG("IDENTIFIER", "Target", "Expected an identifier.");

    if (OPT("LPAREN")) {
        if (!checkToken(parser, "RPAREN")) REQUIRE(parseArgList(parser));
        EXPECT("RPAREN", "Expected ')' after function call arguments.");
        return true;
    }

    return parseAssignmentTail(parser);
}

// ----------------------------------------------------------------------------
// PRINT STATEMENT (OUTPUT STATEMENT)
// 
// PrintStmt = "print", "(", Expression, ")" ;
//
// Handles all 3 forms:
//   1. print("Hello, World!");         - Printing a string literal
//   2. print(myVariable);              - Printing a variable
//   3. print("Value: " + result);      - Printing concatenation (string + variable)
// ----------------------------------------------------------------------------
bool parsePrintStmt(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── PRINT STATEMENT ───────────────────────────────────────────\n");

    // Consume 'print'
    EXPECT("PRINT", "Expected 'print' keyword. Use 'print(expression);' to output values.");
    EXPECT("LPAREN", "Expected '(' after 'print'. Syntax: print(expression);");

    // Parse the expression inside print()
    // This handles all 3 output forms:
    //   1. String literal: print("hello");
    //   2. Variable:       print(x);
    //   3. Concatenation:  print("Value: " + x);
    REQUIRE(parseExpression(parser));
    EXPECT("RPAREN", "Expected ')' to close the print() call. "
                     "Syntax: print(expression);");

    // Classify the print statement for the trace
    // We check what was inside the parentheses by looking at what we parsed
    fprintf(parser->output, "    ── END PRINT STATEMENT ───────────────────────────────────────\n");
    return true;
}

// ----------------------------------------------------------------------------
// CONDITION STATEMENT
// 
// IfStmt = "if", "(", Expression, ")", ( Statement | Block ),
//          [ "else", ( Statement | Block ) ] ;
//
// Handles:
//   1. if (condition) statement;
//   2. if (condition) { block }
//   3. if (condition) { block } else { block }
//   4. if (condition) { block } else if (condition) { block } else { block }
// ----------------------------------------------------------------------------
bool parseIfStmt(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── IF STATEMENT ──────────────────────────────────────────────\n");

    EXPECT("IF",     "Expected 'if' keyword. Syntax: if (condition) { ... }");
    EXPECT("LPAREN", "Expected '(' after 'if'. Syntax: if (condition) { ... }");
    REQUIRE(parseExpression(parser));
    EXPECT("RPAREN", "Expected ')' after if condition. Syntax: if (condition) { ... }");

    REQUIRE(parseBlockOrStmt(parser));

    if (OPT("ELSE")) {
        fprintf(parser->output, "    ── ELSE BRANCH ──────────────────────────────────────────────\n");
        REQUIRE(checkToken(parser, "IF") ? parseIfStmt(parser) : parseBlockOrStmt(parser));
    }

    fprintf(parser->output, "    ── END IF STATEMENT ───────────────────────────────────────────\n");
    return true;
}

// ----------------------------------------------------------------------------
// ITERATIVE STATEMENTS
//
// LoopStmt = "loop", ["do"], Block ;
//
// An infinite loop that repeats the block until a break/transition/return.
// --------------------------------------------------------------------------
bool parseLoopStmt(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── LOOP STATEMENT ────────────────────────────────────────────\n");

    EXPECT("LOOP", "Expected 'loop' keyword. Syntax: loop { ... }");
    matchNoise(parser, "do");
    REQUIRE(parseBlock(parser));

    fprintf(parser->output, "    ── END LOOP STATEMENT ─────────────────────────────────────────\n");
    return true;
}

// --------------------------------------------------------------------------
// ForStmt = "for", "(", Type, Identifier, "=", Expression, ";",
//           Expression, ";", Assignment, ")", [ "do" ], Block ;
// --------------------------------------------------------------------------
bool parseForStmt(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── FOR STATEMENT ─────────────────────────────────────────────\n");

    EXPECT("FOR",    "Expected 'for' keyword. Syntax: for (type var = init; condition; increment) { ... }");
    EXPECT("LPAREN", "Expected '(' after 'for'. Syntax: for (type var = init; condition; increment) { ... }");

    REQUIRE(parseType(parser));

    EXPECT_SYNC("IDENTIFIER",
                "Expected a loop variable name after the type in for loop initialization.",
                "for loop variable",
                "SEMICOLON", "RPAREN", "RBRACE", "EOF");

    EXPECT_SYNC("ASSIGNMENT",
                "Expected '=' after the loop variable in for loop initialization.",
                "for loop assignment",
                "SEMICOLON", "RPAREN", "RBRACE", "EOF");

    REQUIRE(parseExpression(parser));

    EXPECT_SYNC("SEMICOLON",
                "Expected ';' after for loop initialization.",
                "for loop semicolon",
                "SEMICOLON", "RPAREN", "RBRACE", "EOF");

    REQUIRE(parseExpression(parser));

    EXPECT_SYNC("SEMICOLON",
                "Expected ';' after for loop condition.",
                "for loop semicolon",
                "RPAREN", "RBRACE", "EOF");

    REQUIRE(parseAssignment(parser));

    EXPECT("RPAREN", "Expected ')' after for loop header.");
    OPT_NOISE("do");
    REQUIRE(parseBlock(parser));

    fprintf(parser->output, "    ── END FOR STATEMENT ──────────────────────────────────────────\n");
    return true;
}

// --------------------------------------------------------------------------
// MATCH STATEMENT
// 
// MatchStmt = "match", ( "(", Expression, ")" | Identifier ), [ "do" ], "{", { MatchCase }, "}" ;
// MatchCase = Pattern, "->", ( Block | SimpleStatement, ";" ) ;
// --------------------------------------------------------------------------
bool parseMatchStmt(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── MATCH STATEMENT ───────────────────────────────────────────\n");

    EXPECT("MATCH", "Expected 'match' keyword. Syntax: match (expression) { case -> action; }");

    if (OPT("LPAREN")) {
        REQUIRE(parseExpression(parser));
        EXPECT("RPAREN", "Expected ')' after match expression.");
    } else {
        EXPECT_LOG("IDENTIFIER", "Match subject", "Expected an identifier or '(' after 'match'.");
    }

    OPT_NOISE("do");
    EXPECT("LBRACE", "Expected '{' to start match cases. Syntax: match expr { case -> action; }");

    int caseCount = 0;
    while (parser->hasMoreTokens && !CHECK_ANY("RBRACE", "EOF")) {
        if (CHECK_ANY("SUBTRACTION", "ADDITION")) {
            char sign = parser->currentToken.lexeme[0];
            advanceToken(parser);
            if (!CHECK_ANY("INT_LITERAL", "FLOAT_LITERAL")) {
                reportError(parser, "Expected a number after '+' or '-' in match case.");
                return false;
            }
            fprintf(parser->output, "      Case %d: %c%s\n", ++caseCount, sign, parser->currentToken.lexeme);
            advanceToken(parser);
        } else if (CHECK_ANY("STRING_LITERAL", "IDENTIFIER", "INT_LITERAL", "FLOAT_LITERAL",
                             "CHAR_LITERAL", "TRUE", "FALSE", "NULL")) {
            const char *label = (strcmp(parser->currentToken.lexeme, "_") == 0)
                                    ? "_ (default)"
                                    : parser->currentToken.lexeme;
            fprintf(parser->output, "      Case %d: %s\n", ++caseCount, label);
            advanceToken(parser);
        } else {
            break;
        }

        if (!OPT("ARROW")) {
            reportError(parser, "Expected '->' after match case pattern. Syntax: pattern -> action;");
            const char *sync[] = {"RBRACE", "SEMICOLON", "EOF"};
            panicRecovery(parser, sync, 3, "match case arrow");
            OPT("SEMICOLON");
            continue;
        }

        if (checkToken(parser, "LBRACE")) {
            REQUIRE(parseBlock(parser));
        } else {
            REQUIRE(parseSimpleStatement(parser));
            EXPECT("SEMICOLON", "Expected ';' after match case statement.");
        }
    }

    if (caseCount == 0) {
        reportError(parser, "Match statement must have at least one case. "
                            "Add cases like: \"value\" -> { ... } or _ -> { ... }");
    }

    EXPECT("RBRACE", "Expected '}' to close match statement.");

    fprintf(parser->output, "    ── END MATCH STATEMENT ────────────────────────────────────────\n");
    return true;
}

// --------------------------------------------------------------------------
// TRANSITION STATEMENT
// 
// TransitionStmt = "transition", ["to"], Identifier ;
// --------------------------------------------------------------------------
bool parseTransitionStmt(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── TRANSITION STATEMENT ──────────────────────────────────────\n");

    EXPECT("TRANSITION", "Expected 'transition' keyword. Syntax: transition to StateName;");
    OPT_NOISE("to");
    EXPECT_LOG("IDENTIFIER", "Target state", "Expected a state name after 'transition'.");

    fprintf(parser->output, "    ── END TRANSITION STATEMENT ───────────────────────────────────\n");
    return true;
}

// --------------------------------------------------------------------------
// RETURN STATEMENT
// 
// ReturnStmt = "return", Expression ;
// --------------------------------------------------------------------------
bool parseReturnStmt(Parser *parser) {
    GUARD(parser);
    fprintf(parser->output, "    ── RETURN STATEMENT ───────────────────────────────────────────\n");

    EXPECT("RETURN", "Expected 'return' keyword. Syntax: return expression;");
    if (!CHECK_ANY("SEMICOLON", "RBRACE", "EOF")) {
        REQUIRE(parseExpression(parser));
    }

    fprintf(parser->output, "    ── END RETURN STATEMENT ────────────────────────────────────────\n");
    return true;
}

// =============================================================================
// EXPRESSION PARSING (Operator Precedence)
//
// The expression parser implements the full operator precedence hierarchy:
//
//   Level 11: Ternary            (right-to-left)
//   Level 10: Pipeline           (left-to-right)
//   Level  9: Logical OR  (||)   (left-to-right)
//   Level  8: Logical AND (&&)   (left-to-right)
//   Level  7: Equality (==, !=)  (left-to-right)
//   Level  6: Relational (<, >, <=, >=) (left-to-right)
//   Level  5: Additive (+, -)    (left-to-right)
//   Level  4: Multiplicative (*, /, \, %) (left-to-right)
//   Level  3: Unary (!, -)       (right-to-left)
//   Level  2: Exponent (^)       (right-to-left)
//   Level  1: Primary            (literals, identifiers, grouping)
// ============================================================================

// --------------------------------------------------------------------------
// Expression (Level 11): Ternary
// Expression = TernaryExpr ;
// TernaryExpr = LogicalOrExpr, [ "?", Expression, ":", TernaryExpr ] ;
// --------------------------------------------------------------------------
bool parseExpression(Parser *parser) {
    return parseTernaryExpr(parser);
}

bool parseTernaryExpr(Parser *parser) {
    GUARD(parser);
    REQUIRE(parsePipelineExpr(parser));

    if (OPT("QUESTION")) {
        fprintf(parser->output, "        Ternary operator\n");
        REQUIRE(parseExpression(parser));
        EXPECT("COLON", "Expected ':' in ternary expression.");
        REQUIRE(parseTernaryExpr(parser));
    }
    return true;
}

// Level 10: PipelineExpr = LogicalOrExpr, { "|>", Identifier, "(", [ ArgList ], ")" } ;
bool parsePipelineExpr(Parser *parser) {
    GUARD(parser);
    REQUIRE(parseLogicalOrExpr(parser));

    while (OPT("PIPELINE")) {
        fprintf(parser->output, "        Pipeline operator\n");
        EXPECT_LOG("IDENTIFIER", "Pipeline fn", "Expected a function name after '|>' pipeline operator.");
        EXPECT("LPAREN", "Expected '(' after function name in pipeline.");
        if (!checkToken(parser, "RPAREN")) REQUIRE(parseArgList(parser));
        EXPECT("RPAREN", "Expected ')' after pipeline function arguments.");
    }
    return true;
}

// --------------------------------------------------------------------------
// Level 9: Logical OR
// PipelineExpr = LogicalOrExpr, { "|>", FunctionCall } ;
// --------------------------------------------------------------------------
bool parseLogicalOrExpr(Parser *parser) {
    return LEFT_ASSOC(parseLogicalAndExpr, "Logical OR", "Expected an expression after '||'.", "LOGICAL_OR");
}

// --------------------------------------------------------------------------
// Level 8: Logical AND (left-to-right)
// LogicalAndExpr = EqualityExpr, { "&&", EqualityExpr } ;
// --------------------------------------------------------------------------
bool parseLogicalAndExpr(Parser *parser) {
    return LEFT_ASSOC(parseEqualityExpr, "Logical AND", "Expected expression after '&&'.", "LOGICAL_AND");
}

// --------------------------------------------------------------------------
// Level 7: Equality (left-to-right)
// EqualityExpr = RelationalExpr, { ("==" | "!="), RelationalExpr } ;
// --------------------------------------------------------------------------
bool parseEqualityExpr(Parser *parser) {
    return LEFT_ASSOC(parseRelationalExpr, "Equality", "Expected expression after equality operator.", "EQUALITY", "INEQUALITY");
}

// --------------------------------------------------------------------------
// Level 6: Relational (left-to-right)
// RelationalExpr = AdditiveExpr, { ("<" | ">" | "<=" | ">="), AdditiveExpr } ;
// --------------------------------------------------------------------------
bool parseRelationalExpr(Parser *parser) {
    return LEFT_ASSOC(parseAdditiveExpr, "Relational", "Expected expression after relational operator.",
                      "LESS_THAN", "GREATER_THAN", "LESS_EQUAL", "GREATER_EQUAL");
}

// --------------------------------------------------------------------------
// Level 5: Additive (left-to-right)
// AdditiveExpr = MultiplicativeExpr, { ("+" | "-"), MultiplicativeExpr } ;
// --------------------------------------------------------------------------
bool parseAdditiveExpr(Parser *parser) {
    return LEFT_ASSOC(parseMultiplicativeExpr, "Additive", "Expected expression after '+' or '-'.", "ADDITION", "SUBTRACTION");
}

// --------------------------------------------------------------------------
// Level 4: Multiplicative (left-to-right)
// MultiplicativeExpr = UnaryExpr, { ("*" | "/" | "\" | "%"), UnaryExpr } ;
// --------------------------------------------------------------------------
bool parseMultiplicativeExpr(Parser *parser) {
    return LEFT_ASSOC(parseUnaryExpr, "Multiplicative", "Expected expression after arithmetic operator.",
                      "MULTIPLICATION", "DIVISION", "INTEGER_DIVISION", "MODULO");
}

// --------------------------------------------------------------------------
// Level 3: Unary (right-to-left)
// UnaryExpr = [ "!" | "-" ], ExponentExpr ;
// --------------------------------------------------------------------------
bool parseUnaryExpr(Parser *parser) {
    GUARD(parser);

    if (OPT("LOGICAL_NOT")) {
        fprintf(parser->output, "        Unary NOT\n");
        return parseUnaryExpr(parser);
    }
    if (OPT("SUBTRACTION")) {
        fprintf(parser->output, "        Unary minus\n");
        return parseUnaryExpr(parser);
    }
    return parseExponentExpr(parser);
}

// --------------------------------------------------------------------------
// Level 2: Exponent (right-to-left)
// ExponentExpr = PrimaryExpr, [ "^", ExponentExpr ] ;
// --------------------------------------------------------------------------
bool parseExponentExpr(Parser *parser) {
    GUARD(parser);
    REQUIRE(parsePrimaryExpr(parser));

    if (OPT("EXPONENT")) {
        fprintf(parser->output, "        Exponent\n");
        REQUIRE(parseExponentExpr(parser));
    }
    return true;
}

// --------------------------------------------------------------------------
// Level 1: Primary
// PrimaryExpr = Identifier
//             | Literal
//             | "(", Expression, ")"
//             | ArrayAccess
//             | FunctionCall ;
// --------------------------------------------------------------------------
static bool matchLiteral(Parser *parser) {
    static const struct { const char *type; const char *label; } literals[] = {
        {"STRING_LITERAL", "String literal"},
        {"INT_LITERAL",    "Integer literal"},
        {"FLOAT_LITERAL",  "Float literal"},
        {"CHAR_LITERAL",   "Char literal"},
        {"TRUE",           "Boolean literal"},
        {"FALSE",          "Boolean literal"},
        {"NULL",           "Null literal"},
    };
    for (size_t i = 0; i < sizeof(literals) / sizeof(literals[0]); i++) {
        if (checkToken(parser, literals[i].type)) {
            fprintf(parser->output, "        %s: %s\n", literals[i].label, parser->currentToken.lexeme);
            advanceToken(parser);
            return true;
        }
    }
    return false;
}

// PrimaryExpr = Literal
//             | "(", Expression, ")"
//             | "[", [ ArgList ], "]"
//             | ( "readLine" | "readInt" ), "(", [ ArgList ], ")"
//             | Identifier, [ "(", [ ArgList ], ")" | "[", Expression, "]" | ".", Identifier ] ;
bool parsePrimaryExpr(Parser *parser) {
    GUARD(parser);
    if (matchLiteral(parser)) return true;

    // Grouped expression: '(' Expression ')'
    if (OPT("LPAREN")) {
        REQUIRE(parseExpression(parser));
        EXPECT("RPAREN", "Expected ')' to close parenthesized expression.");
        return true;
    }

    // Array literal: '[' [ ArgList ] ']'
    if (OPT("LBRACKET")) {
        if (!checkToken(parser, "RBRACKET")) REQUIRE(parseArgList(parser));
        EXPECT("RBRACKET", "Expected ']' to close array literal.");
        return true;
    }

    // Built-in I/O calls: readLine(...) | readInt(...)
    if (CHECK_ANY("READLINE", "READINT")) {
        fprintf(parser->output, "        Built-in call: %s\n", parser->currentToken.lexeme);
        advanceToken(parser);
        EXPECT("LPAREN", "Expected '(' after built-in input function. Use readLine() or readInt().");
        if (!checkToken(parser, "RPAREN")) REQUIRE(parseArgList(parser));
        EXPECT("RPAREN", "Expected ')' after built-in function arguments.");
        return true;
    }

    // Identifier + optional postfix: call '()', index '[]', or property '.prop'
    if (checkToken(parser, "IDENTIFIER")) {
        fprintf(parser->output, "        Identifier: %s\n", parser->currentToken.lexeme);
        advanceToken(parser);

        if (OPT("LPAREN")) {
            if (!checkToken(parser, "RPAREN")) REQUIRE(parseArgList(parser));
            EXPECT("RPAREN", "Expected ')' after function call arguments.");
        } else if (OPT("LBRACKET")) {
            REQUIRE(parseExpression(parser));
            EXPECT("RBRACKET", "Expected ']' after array index.");
        } else if (OPT("DOT")) {
            EXPECT_LOG("IDENTIFIER", "Property", "Expected a property name after '.'.");
        }
        return true;
    }

    // Noise word fallback
    if (checkToken(parser, "NOISE_WORD")) {
        fprintf(parser->output, "        Noise word: %s\n", parser->currentToken.lexeme);
        advanceToken(parser);
        return true;
    }

    reportError(parser, "Expected a primary expression: a literal, identifier, '(...)', or '[...]'.");
    return false;
}