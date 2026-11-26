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
    do { if ((p) == NULL || !(p)->hasMoreTokens) return false; } while (0); \
    ParseNode *_prevNode = (p)->currentNode

#define NODE(name) \
    do { \
        (parser)->currentNode = addChildNodeAt( \
            (parser)->currentNode, (name), \
            (parser)->currentToken.sourceLine, \
            (parser)->currentToken.sourceColumn); \
    } while (0)

#define PASS() \
    do { (parser)->currentNode = _prevNode; return true; } while (0)

#define FAIL() \
    do { (parser)->currentNode = _prevNode; return false; } while (0)

#define RETURN(call) \
    do { bool _ok = (call); (parser)->currentNode = _prevNode; return _ok; } while (0)

// Require a terminal token, or report error and return false
#define EXPECT(tok, msg) \
    do { if (!expectToken(parser, (tok), (msg))) FAIL(); } while (0)

#define EXPECT_LOG(tok, label, msg) \
    do { if (!expectTokenLogged(parser, (tok), (label), (msg))) FAIL(); } while (0)

// Expect a terminal or immediately run panic recovery
#define EXPECT_SYNC(tok, msg, ctx, ...) \
    do { \
        if (!matchToken(parser, (tok))) { \
            reportError(parser, (msg)); \
            panicRecovery(parser, TOKEN_LIST(__VA_ARGS__), TOKEN_COUNT(__VA_ARGS__), (ctx)); \
            FAIL(); \
        } \
    } while (0)

// Require a non-terminal production, or propagate failure
#define REQUIRE(call) \
    do { if (!(call)) FAIL(); } while (0)

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
    size_t startIdx = parser->currentNode->childCount;
    bool matchedOperator = false;
    REQUIRE(parseOperand(parser));
    while (checkTokenAny(parser, ops, opCount)) {
        matchedOperator = true;
        advanceToken(parser);
        REQUIRE(parseOperand(parser));
    }
    if (matchedOperator) {
        wrapChildrenFrom(parser->currentNode, startIdx, label);
    }
    PASS();
}

// Expect a terminal, log its lexeme BEFORE advancing (fixes the double-advance bug)
static bool expectTokenLogged(Parser *parser, const char *expected, const char *label, const char *errMsg) {
    if (!checkToken(parser, expected)) {
        reportError(parser, errMsg);
        return false;
    }
    (void)label;
    advanceToken(parser);
    return true;
}

// Left-associative binary precedence level: EBNF  Next, { (op1 | op2), Next }
#define LEFT_ASSOC(operandFn, label, ...) \
    parseLeftAssoc(parser, (operandFn), TOKEN_LIST(__VA_ARGS__), TOKEN_COUNT(__VA_ARGS__), (label))

static bool expectSemicolon(Parser *parser, const char *errMsg, const char *ctx) {
    if (matchToken(parser, "SEMICOLON")) {
        return true;
    }

    bool afterLineEnd = !parser->hasMoreTokens ||
                        checkToken(parser, "EOF") ||
                        checkToken(parser, "RBRACE") ||
                        (parser->hasPrevToken &&
                         parser->currentToken.sourceLine > parser->prevToken.sourceLine);

    if (afterLineEnd && parser->hasPrevToken) {
        reportErrorAfterPrev(parser, errMsg);
        // Virtual semicolon: build still fails (encounteredError = true),
        // but we don't swallow the next line via panicRecovery.
        return true;
    }

    // Same-line error (e.g., stray tokens before ';'): report at current token & recover
    reportError(parser, errMsg);
    const char *sync[] = {"SEMICOLON", "RBRACE", "EOF"};
    panicRecovery(parser, sync, 3, ctx);
    return false;
}

#define EXPECT_SEMI(msg, ctx) \
    do { if (!expectSemicolon(parser, (msg), (ctx))) FAIL(); } while (0)

static bool expectClosing(Parser *parser, const char *closingTok,
                          const SymbolEntry *openTok, const char *errMsg) {
    if (matchToken(parser, closingTok)) {
        return true;
    }
    reportErrorAtToken(parser, openTok, errMsg);
    return false;
}

#define EXPECT_CLOSE(tok, openTok, msg) \
    do { if (!expectClosing(parser, (tok), &(openTok), (msg))) FAIL(); } while (0)

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

    // Parse statements until EOF
    while (parser->hasMoreTokens) {
        if (checkToken(parser, "EOF")) {
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
static bool parseIdentOrEnsureStmt(Parser *parser); // Forward Declaration
bool parseStatement(Parser *parser) {
    GUARD(parser);
    NODE("Statement");

    // Empty statement: just consume the single ';' and finish
    if (OPT("SEMICOLON")) PASS();
    // Distinguish between a variable declaration and an ensure statement
    if (isTypeStart(parser)) RETURN(parseDeclOrEnsureStmt(parser));
    if (checkToken(parser, "IDENTIFIER")) RETURN(parseIdentOrEnsureStmt(parser));
    if (CHECK_ANY("IF", "LOOP", "FOR", "MATCH", "STATE", "REACT", "FN")) {
        RETURN(parseBlockStatement(parser));
    }

    REQUIRE(parseSimpleStatement(parser));
    EXPECT_SEMI("Expected ';' after statement.", "statement termination");
    PASS();
}

// ----------------------------------------------------------------------------
// BLOCK STATEMENT
// 
// BlockStatement = IfStmt | LoopStmt | ForStmt | MatchStmt | EnsureStmt
//                 | StateDecl | ReactDecl | FnDecl ;
// ----------------------------------------------------------------------------
bool parseBlockStatement(Parser *parser) {
    GUARD(parser);
    NODE("BlockStatement");
    static const TokenRule rules[] = {
        {"IF", parseIfStmt},         {"LOOP", parseLoopStmt},
        {"FOR", parseForStmt},       {"MATCH", parseMatchStmt},
        {"STATE", parseStateDecl},
        {"REACT", parseReactDecl},   {"FN", parseFnDecl},
    };

    ParseFn fn = lookupRule(parser, rules, sizeof(rules) / sizeof(rules[0]));
    if (fn != NULL) RETURN(fn(parser));

    reportError(parser, "Expected a block statement keyword (if, loop, for, match, state, react, or fn).");
    FAIL();
}

// ----------------------------------------------------------------------------
// SIMPLE STATEMENT
// 
// SimpleStatement = VarDecl | Assignment | PrintStmt | TransitionStmt
//                  | ReturnStmt | ExprStmt ;
// ----------------------------------------------------------------------------
bool parseSimpleStatement(Parser *parser) {
    GUARD(parser);
    NODE("SimpleStatement");

    if (isTypeStart(parser)) RETURN(parseVarDecl(parser));

    static const TokenRule rules[] = {
        {"PRINT",      parsePrintStmt},
        {"TRANSITION", parseTransitionStmt},
        {"RETURN",     parseReturnStmt},
        {"SLEEP",      parseSleepStmt},
        {"IDENTIFIER", parseIdentStmt},
    };
    ParseFn fn = lookupRule(parser, rules, sizeof(rules) / sizeof(rules[0]));
    if (fn != NULL) RETURN(fn(parser));

    if (OPT("QUIT") || OPT("NOISE_WORD")) PASS();

    if (CHECK_ANY("INT_LITERAL", "FLOAT_LITERAL", "STRING_LITERAL", "CHAR_LITERAL",
                  "TRUE", "FALSE", "NULL", "LPAREN", "LOGICAL_NOT", "SUBTRACTION",
                  "READLINE", "READINT")) {
        RETURN(parseExpression(parser));
    }

    reportError(parser, "Unexpected token. Expected a declaration, assignment, "
                        "'print', 'transition', 'return', 'sleep', 'quit', or expression.");
    FAIL();
}

// ----------------------------------------------------------------------------
// SLEEP STATEMENT
// 
// SleepStmt = "sleep", "(", Expression, ")" ;
// ----------------------------------------------------------------------------
bool parseSleepStmt(Parser *parser) {
    GUARD(parser);
    NODE("SleepStmt");
    EXPECT("SLEEP",  "Expected 'sleep' keyword.");
    SymbolEntry openParen = parser->currentToken;
    EXPECT("LPAREN", "Expected '(' after 'sleep'. Usage: sleep(milliseconds);");
    REQUIRE(parseExpression(parser));
    EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after sleep argument.");
    PASS();
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

// Handles both top-level/block declarations:
//   VarDecl = Type, Identifier, [ "=", Expression ] ;
//   EnsureStmt = Type, Identifier, [ "=", "ensure", Expression, "else", Block ;
bool parseDeclOrEnsureStmt(Parser *parser) {
    GUARD(parser);
    NODE("SimpleStatement");
    ParseNode *kindNode = parser->currentNode;
    NODE("VarDecl");
    ParseNode *declNode = parser->currentNode;

    REQUIRE(parseType(parser));
    EXPECT_LOG("IDENTIFIER", "Variable", "Expected a variable name after type.");

    // Uninitialized declaration: e.g., [int, 10] scores;
    if (checkToken(parser, "SEMICOLON")) {
        parser->currentNode = _prevNode;
        EXPECT_SEMI("Expected ';' after variable declaration.", "declaration termination");
        PASS();
    }

    OPT_NOISE("is");
    EXPECT("ASSIGNMENT", "Expected '=' or ';' after variable name in declaration.");

    if (OPT("ENSURE")) {
        strncpy(kindNode->label, "BlockStatement", sizeof(kindNode->label) - 1);
        kindNode->label[sizeof(kindNode->label) - 1] = '\0';
        strncpy(declNode->label, "EnsureStmt", sizeof(declNode->label) - 1);
        declNode->label[sizeof(declNode->label) - 1] = '\0';
        REQUIRE(parseExpression(parser));
        EXPECT("ELSE", "Expected 'else' in ensure statement.");
        RETURN(parseBlock(parser));
    }

    REQUIRE(parseExpression(parser));
    parser->currentNode = _prevNode;
    EXPECT_SEMI("Expected ';' after variable declaration.", "declaration termination");
    PASS();
}

// VarDecl (used inside match cases where the caller consumes ';')
bool parseVarDecl(Parser *parser) {
    GUARD(parser);
    NODE("VarDecl");
    REQUIRE(parseType(parser));
    EXPECT_LOG("IDENTIFIER", "Variable", "Expected a variable name after type.");

    if (OPT_NOISE("is") || checkToken(parser, "ASSIGNMENT")) {
        EXPECT("ASSIGNMENT", "Expected '=' in variable declaration.");
        REQUIRE(parseExpression(parser));
    }
    PASS();
}

// --------------------------------------------------------------------------
// BLOCK
// 
// Block = "{", { Statement }, "}" ;
// --------------------------------------------------------------------------
bool parseBlock(Parser *parser) {
    GUARD(parser);
    NODE("Block");

    SymbolEntry openBrace = parser->currentToken;
    EXPECT("LBRACE", "Expected '{' to start a block.");

    // Parse statements inside the block
    while (parser->hasMoreTokens && !checkToken(parser, "RBRACE") && !checkToken(parser, "EOF")) {
        if (!parseStatement(parser)) {
            // Error recovery inside block
            const char *sync[] = {"SEMICOLON", "RBRACE", "EOF"};
            panicRecovery(parser, sync, 3, "block statement");
        }
    }

    EXPECT_CLOSE("RBRACE", openBrace, "Unclosed '{' — expected matching '}'.");
    PASS();
}

// --------------------------------------------------------------------------
// STATE DECLARATION
// 
// StateDecl = "state", Identifier, ["do"], Block ;
// --------------------------------------------------------------------------
bool parseStateDecl(Parser *parser) {
    GUARD(parser);
    NODE("StateDecl");

    EXPECT("STATE", "Expected 'state' keyword. Syntax: state StateName { ... }");
    EXPECT_LOG("IDENTIFIER", "State name", "Expected a state name (identifier) after 'state'. "
                         "Syntax: state StateName { ... }");
    OPT_NOISE("do");
    REQUIRE(parseBlock(parser));

    PASS();
}

// --------------------------------------------------------------------------
// REACT DECLARATION
// 
// ReactDecl = "react", "(", Identifier, ")", ["do"], Block ;
// --------------------------------------------------------------------------
bool parseReactDecl(Parser *parser) {
    GUARD(parser);
    NODE("ReactDecl");

    EXPECT("REACT",  "Expected 'react' keyword. Syntax: react (variable) { ... }");
    SymbolEntry openParen = parser->currentToken;
    EXPECT("LPAREN", "Expected '(' after 'react'. Syntax: react (variable) { ... }");
    EXPECT_LOG("IDENTIFIER", "Watching", "Expected a variable name inside react().");
    EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after react variable.");
    OPT_NOISE("do");
    REQUIRE(parseBlock(parser));

    PASS();
}

// --------------------------------------------------------------------------
// FUNCTION DECLARATION
// 
// FnDecl = "fn", Identifier, "(", [ParamList], ")", ":", Type, ["do"], Block ;
// --------------------------------------------------------------------------
bool parseFnDecl(Parser *parser) {
    GUARD(parser);
    NODE("FnDecl");

    EXPECT("FN",         "Expected 'fn' keyword.");
    EXPECT("IDENTIFIER", "Expected a function name after 'fn'.");
    SymbolEntry openParen = parser->currentToken;
    EXPECT("LPAREN",     "Expected '(' after function name.");
    if (!checkToken(parser, "RPAREN")) {
        REQUIRE(parseParamList(parser));
    }
    EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after function parameters.");
    EXPECT("COLON",      "Expected ':' before return type.");
    REQUIRE(parseType(parser));
    matchNoise(parser, "do");
    REQUIRE(parseBlock(parser));

    PASS();
}

// --------------------------------------------------------------------------
// PARAMETER LIST
// 
// ParamList = Type, Identifier, { ",", Type, Identifier } ;
// --------------------------------------------------------------------------
bool parseParamList(Parser *parser) {
    GUARD(parser);
    NODE("ParamList");
    do {
        REQUIRE(parseType(parser));
        EXPECT_LOG("IDENTIFIER", "Param", "Expected a parameter name after type.");
    } while (OPT("COMMA"));
    PASS();
}

// --------------------------------------------------------------------------
// ARGUMENT LIST
// 
// ArgList = Expression, { ",", Expression } ;
// --------------------------------------------------------------------------
bool parseArgList(Parser *parser) {
    GUARD(parser);
    NODE("ArgList");
    do {
        REQUIRE(parseExpression(parser));
    } while (OPT("COMMA"));
    PASS();
}

// --------------------------------------------------------------------------
// TYPE
// 
// Type    = "int" | "float" | "string" | "bool" | "char"
// | "int?" | "float?" | "string?" | "bool?" | "char?"
// | "[", Type, [ ",", IntLiteral ], "]", [ "?" ] ;
// --------------------------------------------------------------------------
bool parseType(Parser *parser) {
    GUARD(parser);
    NODE("Type");

    if (OPT("LBRACKET")) {
        SymbolEntry openBracket = parser->prevToken;
        REQUIRE(parseType(parser));
        if (OPT("COMMA")) {
            EXPECT("INT_LITERAL", "Expected an integer size after ',' in fixed-size array type.");
        }
        EXPECT_CLOSE("RBRACKET", openBracket, "Expected ']' to close array type.");
        OPT("QUESTION");
        PASS();
    }

    if (CHECK_ANY("INT", "FLOAT", "STRING", "BOOL", "CHAR")) {
        advanceToken(parser);
        OPT("QUESTION");
        PASS();
    }

    reportError(parser, "Expected a type keyword (int, float, string, bool, char) or an array type ([type]).");
    FAIL();
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
    GUARD(parser);
    while (OPT("LBRACKET")) {
        SymbolEntry openBracket = parser->prevToken;
        REQUIRE(parseExpression(parser));
        EXPECT_CLOSE("RBRACKET", openBracket, "Expected ']' after array index. Syntax: arrayName[index] = value;");
    }

    if (!CHECK_ANY("ASSIGNMENT", "ADD_ASSIGN", "SUBTRACT_ASSIGN",
                   "MULTIPLY_ASSIGN", "DIVIDE_ASSIGN", "MODULO_ASSIGN")) {
        reportError(parser, "Expected an assignment operator (=, +=, -=, *=, /=, %=).");
        const char *sync[] = {"SEMICOLON", "RBRACE", "EOF"};
        panicRecovery(parser, sync, 3, "assignment operator");
        FAIL();
    }

    advanceToken(parser); // consume assignment operator

    REQUIRE(parseExpression(parser));
    PASS();
}

// Used directly by parseForStmt: Identifier, [ "[", Expr, "]" ], AssignOp, Expr
bool parseAssignment(Parser *parser) {
    GUARD(parser);
    NODE("Assignment");
    EXPECT_LOG("IDENTIFIER", "Target", "Expected an identifier on the left side of assignment.");
    REQUIRE(parseAssignmentTail(parser));
    PASS();
}

// Used by parseSimpleStatement: handles `func(args)`, `var |> fn()`, and `var = expr`
static bool parsePipelineStages(Parser *parser);
bool parseIdentStmt(Parser *parser) {
    GUARD(parser);
    size_t startIdx = parser->currentNode->childCount;
    NODE("Assignment");
    ParseNode *stmtNode = parser->currentNode;
    EXPECT_LOG("IDENTIFIER", "Target", "Expected an identifier.");
    if (OPT("LPAREN")) {
        SymbolEntry openParen = parser->prevToken;
        strncpy(stmtNode->label, "FunctionCall", sizeof(stmtNode->label) - 1);
        stmtNode->label[sizeof(stmtNode->label) - 1] = '\0';
        if (!checkToken(parser, "RPAREN")) REQUIRE(parseArgList(parser));
        EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after function call arguments.");
        if (checkToken(parser, "PIPELINE")) {
            parser->currentNode = _prevNode;
            REQUIRE(parsePipelineStages(parser));
            wrapChildrenFrom(parser->currentNode, startIdx, "PipelineExpr");
        }
        PASS();
    }
    if (checkToken(parser, "PIPELINE")) {
        strncpy(stmtNode->label, "PipelineExpr", sizeof(stmtNode->label) - 1);
        stmtNode->label[sizeof(stmtNode->label) - 1] = '\0';
        RETURN(parsePipelineStages(parser));
    }
    RETURN(parseAssignmentTail(parser));
}

// Handles statements starting with IDENTIFIER:
//   1. func(args); / x |> fn();          -> SimpleStatement -> FunctionCall / PipelineExpr
//   2. x = ensure expr else { ... }      -> BlockStatement  -> EnsureStmt (no ';')
//   3. x = expr; / x[i] = expr; / x+=1;  -> SimpleStatement -> Assignment
static bool parseIdentOrEnsureStmt(Parser *parser) {
    GUARD(parser);
    NODE("SimpleStatement");
    ParseNode *kindNode = parser->currentNode;
    size_t stmtIdx = kindNode->childCount;
    NODE("Assignment");
    ParseNode *stmtNode = parser->currentNode;

    EXPECT_LOG("IDENTIFIER", "Target", "Expected an identifier.");

    if (OPT("LPAREN")) {
        SymbolEntry openParen = parser->prevToken;
        strncpy(stmtNode->label, "FunctionCall", sizeof(stmtNode->label) - 1);
        stmtNode->label[sizeof(stmtNode->label) - 1] = '\0';
        if (!checkToken(parser, "RPAREN")) REQUIRE(parseArgList(parser));
        EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after function call arguments.");
        if (checkToken(parser, "PIPELINE")) {
            parser->currentNode = kindNode;
            REQUIRE(parsePipelineStages(parser));
            wrapChildrenFrom(kindNode, stmtIdx, "PipelineExpr");
        }
        parser->currentNode = _prevNode;
        EXPECT_SEMI("Expected ';' after statement.", "statement termination");
        PASS();
    }

    if (checkToken(parser, "PIPELINE")) {
        strncpy(stmtNode->label, "PipelineExpr", sizeof(stmtNode->label) - 1);
        stmtNode->label[sizeof(stmtNode->label) - 1] = '\0';
        REQUIRE(parsePipelineStages(parser));
        parser->currentNode = _prevNode;
        EXPECT_SEMI("Expected ';' after statement.", "statement termination");
        PASS();
    }

    if (OPT("ASSIGNMENT")) {
        if (OPT("ENSURE")) {
            strncpy(kindNode->label, "BlockStatement", sizeof(kindNode->label) - 1);
            kindNode->label[sizeof(kindNode->label) - 1] = '\0';
            strncpy(stmtNode->label, "EnsureStmt", sizeof(stmtNode->label) - 1);
            stmtNode->label[sizeof(stmtNode->label) - 1] = '\0';
            REQUIRE(parseExpression(parser));
            EXPECT("ELSE", "Expected 'else' in ensure statement.");
            RETURN(parseBlock(parser));
        }

        REQUIRE(parseExpression(parser));
        parser->currentNode = _prevNode;
        EXPECT_SEMI("Expected ';' after statement.", "statement termination");
        PASS();
    }

    REQUIRE(parseAssignmentTail(parser));
    parser->currentNode = _prevNode;
    EXPECT_SEMI("Expected ';' after statement.", "statement termination");
    PASS();
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
    NODE("PrintStmt");

    // Consume 'print'
    EXPECT("PRINT", "Expected 'print' keyword. Use 'print(expression);' to output values.");
    SymbolEntry openParen = parser->currentToken;
    EXPECT("LPAREN", "Expected '(' after 'print'. Syntax: print(expression);");

    // Parse the expression inside print()
    // This handles all 3 output forms:
    //   1. String literal: print("hello");
    //   2. Variable:       print(x);
    //   3. Concatenation:  print("Value: " + x);
    REQUIRE(parseExpression(parser));
    
    if (OPT("COMMA")) {
        REQUIRE(parseExpression(parser));
    }
    
    EXPECT_CLOSE("RPAREN", openParen, "Expected ')' to close the print() call. ");
    PASS();
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
    NODE("IfStmt");

    EXPECT("IF",     "Expected 'if' keyword. Syntax: if (condition) { ... }");
    SymbolEntry openParen = parser->currentToken;
    EXPECT("LPAREN", "Expected '(' after 'if'. Syntax: if (condition) { ... }");
    REQUIRE(parseExpression(parser));
    EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after if condition. Syntax: if (condition) { ... }");

    REQUIRE(parseBlockOrStmt(parser));

    if (OPT("ELSE")) {
        REQUIRE(checkToken(parser, "IF") ? parseIfStmt(parser) : parseBlockOrStmt(parser));
    }
    PASS();
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
    NODE("LoopStmt");

    EXPECT("LOOP", "Expected 'loop' keyword. Syntax: loop { ... }");
    matchNoise(parser, "do");
    REQUIRE(parseBlock(parser));

    PASS();
}

// --------------------------------------------------------------------------
// ForStmt = "for", "(", Type, Identifier, "=", Expression, ";",
//           Expression, ";", Assignment, ")", [ "do" ], Block ;
// --------------------------------------------------------------------------
bool parseForStmt(Parser *parser) {
    GUARD(parser);
    NODE("ForStmt");

    EXPECT("FOR",    "Expected 'for' keyword. Syntax: for (type var = init; condition; increment) { ... }");
    SymbolEntry openParen = parser->currentToken;
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

    EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after for loop header.");
    OPT_NOISE("do");
    REQUIRE(parseBlock(parser));

    PASS();
}

// --------------------------------------------------------------------------
// MATCH STATEMENT
// 
// MatchStmt = "match", ( "(", Expression, ")" | Identifier ), [ "do" ], "{", { MatchCase }, "}" ;
// MatchCase = Pattern, "->", ( Block | SimpleStatement, ";" ) ;
// --------------------------------------------------------------------------
bool parseMatchStmt(Parser *parser) {
    GUARD(parser);
    NODE("MatchStmt");
    ParseNode *matchNode = parser->currentNode;

    EXPECT("MATCH", "Expected 'match' keyword. Syntax: match (expression) { case -> action; }");

    if (OPT("LPAREN")) {
        SymbolEntry openParen = parser->prevToken;
        REQUIRE(parseExpression(parser));
        EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after match expression.");
    } else {
        EXPECT_LOG("IDENTIFIER", "Match subject", "Expected an identifier or '(' after 'match'.");
    }

    OPT_NOISE("do");
    SymbolEntry openBrace = parser->currentToken;
    EXPECT("LBRACE", "Expected '{' to start match cases. Syntax: match expr { case -> action; }");

    int caseCount = 0;
    while (parser->hasMoreTokens && !CHECK_ANY("RBRACE", "EOF")) {
        parser->currentNode = matchNode;
        NODE("MatchCase");
        if (CHECK_ANY("SUBTRACTION", "ADDITION")) {
            advanceToken(parser);
            if (!CHECK_ANY("INT_LITERAL", "FLOAT_LITERAL")) {
                reportError(parser, "Expected a number after '+' or '-' in match case.");
                FAIL();
            }
            advanceToken(parser);
            caseCount++;
        } else if (CHECK_ANY("STRING_LITERAL", "IDENTIFIER", "INT_LITERAL", "FLOAT_LITERAL",
                             "CHAR_LITERAL", "TRUE", "FALSE", "NULL")) {
            advanceToken(parser);
            caseCount++;
        } else {
            reportError(parser, "Expected a match pattern (literal or '_') before '->'.");
            FAIL();
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
            EXPECT_SEMI("Expected ';' after match case statement.", "match case termination");
        }
    }

    parser->currentNode = matchNode;

    if (caseCount == 0) {
        reportError(parser, "Match statement must have at least one case. "
                            "Add cases like: \"value\" -> { ... } or _ -> { ... }");
    }

    EXPECT_CLOSE("RBRACE", openBrace, "Expected '}' to close match statement.");
    PASS();
}

// --------------------------------------------------------------------------
// TRANSITION STATEMENT
// 
// TransitionStmt = "transition", ["to"], Identifier ;
// --------------------------------------------------------------------------
bool parseTransitionStmt(Parser *parser) {
    GUARD(parser);
    NODE("TransitionStmt");

    EXPECT("TRANSITION", "Expected 'transition' keyword. Syntax: transition to StateName;");
    OPT_NOISE("to");
    EXPECT_LOG("IDENTIFIER", "Target state", "Expected a state name after 'transition'.");

    PASS();
}

// --------------------------------------------------------------------------
// RETURN STATEMENT
// 
// ReturnStmt = "return", Expression ;
// --------------------------------------------------------------------------
bool parseReturnStmt(Parser *parser) {
    GUARD(parser);
    NODE("ReturnStmt");

    EXPECT("RETURN", "Expected 'return' keyword. Syntax: return expression;");
    REQUIRE(parseExpression(parser));

    PASS();
}

// =============================================================================
// EXPRESSION PARSING (Operator Precedence)
//
// The expression parser implements the full operator precedence hierarchy:
//
//   Level 11: Pipeline           (left-to-right)
//   Level 10: Ternary            (right-to-left)
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
// Level 11 (Lowest): Pipeline (left-to-right)
// Expression     = PipelineExpr ;
// PipelineTarget = FunctionCall | ( "print", "(", [ ArgList ], ")" ) ;
// PipelineExpr   = TernaryExpr, { "|>", PipelineTarget } ;
// --------------------------------------------------------------------------
bool parseExpression(Parser *parser) {
    return parsePipelineExpr(parser);
}

static bool parsePipelineStages(Parser *parser) {
    GUARD(parser);
    while (OPT("PIPELINE")) {
        ParseNode *pipeParent = parser->currentNode;
        NODE(checkToken(parser, "PRINT") ? "PrintStmt" : "FunctionCall");

        if (CHECK_ANY("IDENTIFIER", "PRINT")) {
            advanceToken(parser);
        } else {
            reportError(parser, "Expected a function name or 'print' after '|>' pipeline operator.");
            parser->currentNode = pipeParent;
            FAIL();
        }
        
        SymbolEntry openParen = parser->currentToken;
        EXPECT("LPAREN", "Expected '(' after function name in pipeline.");
        if (!checkToken(parser, "RPAREN")) REQUIRE(parseArgList(parser));
        EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after pipeline function arguments.");
        parser->currentNode = pipeParent;
    }
    PASS();
}

bool parsePipelineExpr(Parser *parser) {
    GUARD(parser);
    size_t startIdx = parser->currentNode->childCount;
    REQUIRE(parseTernaryExpr(parser));

    if (checkToken(parser, "PIPELINE")) {
        REQUIRE(parsePipelineStages(parser));
        wrapChildrenFrom(parser->currentNode, startIdx, "PipelineExpr");
    }
    PASS();
}

// --------------------------------------------------------------------------
// Level 10: Ternary Conditional (right-to-left)
// TernaryExpr = LogicalOrExpr, [ "?", Expression, ":", TernaryExpr ] ;
// --------------------------------------------------------------------------
bool parseTernaryExpr(Parser *parser) {
    GUARD(parser);
    size_t startIdx = parser->currentNode->childCount;
    bool matchedOperator = false;
    REQUIRE(parseLogicalOrExpr(parser));

    if (OPT("QUESTION")) {
        matchedOperator = true;
        REQUIRE(parseExpression(parser));
        EXPECT("COLON", "Expected ':' in ternary expression.");
        REQUIRE(parseTernaryExpr(parser));
    }

    if (matchedOperator) wrapChildrenFrom(parser->currentNode, startIdx, "TernaryExpr");
    PASS();
}

// --------------------------------------------------------------------------
// Level 9: Logical OR
// LogicalOrExpr = LogicalAndExpr, { "||", LogicalAndExpr } ;
// --------------------------------------------------------------------------
bool parseLogicalOrExpr(Parser *parser) {
    return LEFT_ASSOC(parseLogicalAndExpr, "LogicalOrExpr", "LOGICAL_OR");
}

// --------------------------------------------------------------------------
// Level 8: Logical AND (left-to-right)
// LogicalAndExpr = EqualityExpr, { "&&", EqualityExpr } ;
// --------------------------------------------------------------------------
bool parseLogicalAndExpr(Parser *parser) {
    return LEFT_ASSOC(parseEqualityExpr, "LogicalAndExpr", "LOGICAL_AND");
}

// --------------------------------------------------------------------------
// Level 7: Equality (left-to-right)
// EqualityExpr = RelationalExpr, { ("==" | "!="), RelationalExpr } ;
// --------------------------------------------------------------------------
bool parseEqualityExpr(Parser *parser) {
    return LEFT_ASSOC(parseRelationalExpr, "EqualityExpr", "EQUALITY", "INEQUALITY");
}

// --------------------------------------------------------------------------
// Level 6: Relational (left-to-right)
// RelationalExpr = AdditiveExpr, { ("<" | ">" | "<=" | ">="), AdditiveExpr } ;
// --------------------------------------------------------------------------
bool parseRelationalExpr(Parser *parser) {
    return LEFT_ASSOC(parseAdditiveExpr, "RelationalExpr", "LESS_THAN", "GREATER_THAN", "LESS_EQUAL", "GREATER_EQUAL");
}

// --------------------------------------------------------------------------
// Level 5: Additive (left-to-right)
// AdditiveExpr = MultiplicativeExpr, { ("+" | "-"), MultiplicativeExpr } ;
// --------------------------------------------------------------------------
bool parseAdditiveExpr(Parser *parser) {
    return LEFT_ASSOC(parseMultiplicativeExpr, "AdditiveExpr", "ADDITION", "SUBTRACTION");
}

// --------------------------------------------------------------------------
// Level 4: Multiplicative (left-to-right)
// MultiplicativeExpr = UnaryExpr, { ("*" | "/" | "\" | "%"), UnaryExpr } ;
// --------------------------------------------------------------------------
bool parseMultiplicativeExpr(Parser *parser) {
    return LEFT_ASSOC(parseUnaryExpr, "MultiplicativeExpr", "MULTIPLICATION", "DIVISION", "INTEGER_DIVISION", "MODULO");
}

// --------------------------------------------------------------------------
// Level 3: Unary (right-to-left)
// UnaryExpr = [ "!" | "-" ], ExponentExpr ;
// --------------------------------------------------------------------------
bool parseUnaryExpr(Parser *parser) {
    GUARD(parser);

    if (CHECK_ANY("LOGICAL_NOT", "SUBTRACTION")) {
        NODE("UnaryExpr");
        advanceToken(parser);
        RETURN(parseUnaryExpr(parser));
    }
    return parseExponentExpr(parser);
}

// --------------------------------------------------------------------------
// Level 2: Exponent (right-to-left)
// ExponentExpr = PrimaryExpr, [ "^", ExponentExpr ] ;
// --------------------------------------------------------------------------
static bool parsePostfixExpr(Parser *parser); // Forward declaration
bool parseExponentExpr(Parser *parser) {
    GUARD(parser);
    size_t startIdx = parser->currentNode->childCount;
    bool matchedOperator = false;
    REQUIRE(parsePostfixExpr(parser));

    if (OPT("EXPONENT")) {
        matchedOperator = true;
        REQUIRE(parseExponentExpr(parser));
    }
    if (matchedOperator) wrapChildrenFrom(parser->currentNode, startIdx, "ExponentExpr");
    PASS();
}

// --------------------------------------------------------------------------
// Level 1.5: Postfix Access (left-to-right)
// PostfixExpr = PrimaryExpr, { "[", Expression, "]" | ".", Identifier | "(", [ ArgList ], ")" } ;
// --------------------------------------------------------------------------
static bool parsePostfixExpr(Parser *parser) {
    GUARD(parser);
    size_t startIdx = parser->currentNode->childCount;
    REQUIRE(parsePrimaryExpr(parser));

    while (CHECK_ANY("LPAREN", "LBRACKET", "DOT")) {
        if (OPT("LPAREN")) {
            SymbolEntry openParen = parser->prevToken;
            if (!checkToken(parser, "RPAREN")) REQUIRE(parseArgList(parser));
            EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after function call arguments.");
            wrapChildrenFrom(parser->currentNode, startIdx, "FunctionCall");
        } else if (OPT("LBRACKET")) {
            SymbolEntry openBracket = parser->prevToken;
            REQUIRE(parseExpression(parser));
            EXPECT_CLOSE("RBRACKET", openBracket, "Expected ']' after array index.");
            wrapChildrenFrom(parser->currentNode, startIdx, "ArrayAccess");
        } else if (OPT("DOT")) {
            EXPECT_LOG("IDENTIFIER", "Property", "Expected a property name after '.'.");
            wrapChildrenFrom(parser->currentNode, startIdx, "PropertyAccess");
        }
    }

    PASS();
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
    if (matchLiteral(parser)) PASS();

    // Grouped expression: '(' Expression ')'
    if (checkToken(parser, "LPAREN")) {
        SymbolEntry openParen = parser->currentToken;
        advanceToken(parser);
        REQUIRE(parseExpression(parser));
        EXPECT_CLOSE("RPAREN", openParen, "Unclosed '(' — expected matching ')'.");
        PASS();
    }

    // Array literal: '[' [ ArgList ] ']'
    if (checkToken(parser, "LBRACKET")) {
        SymbolEntry openBracket = parser->currentToken;
        NODE("ArrayLiteral");
        advanceToken(parser);
        if (!checkToken(parser, "RBRACKET")) REQUIRE(parseArgList(parser));
        EXPECT_CLOSE("RBRACKET", openBracket, "Unclosed '[' — expected matching ']'.");
        PASS();
    }

    // Built-in I/O calls: readLine(...) | readInt(...)
    if (CHECK_ANY("READLINE", "READINT")) {
        NODE("InputExpr");
        advanceToken(parser);
        SymbolEntry openParen = parser->currentToken;
        EXPECT("LPAREN", "Expected '(' after built-in input function. Use readLine() or readInt().");
        if (!checkToken(parser, "RPAREN")) REQUIRE(parseArgList(parser));
        EXPECT_CLOSE("RPAREN", openParen, "Expected ')' after built-in function arguments.");
        PASS();
    }

    // Bare identifier (postfixes are handled by parsePostfixExpr)
    if (checkToken(parser, "IDENTIFIER")) {
        advanceToken(parser);
        PASS();
    }

    if (CHECK_ANY("SEMICOLON", "RPAREN", "RBRACKET", "RBRACE", "COMMA")) {
        reportError(parser, "Missing value or expression.");
    } else {
        reportError(parser, "Expected a value or expression (e.g., a literal, variable, or function call).");
    }
    FAIL();
}