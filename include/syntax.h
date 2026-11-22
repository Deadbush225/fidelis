#ifndef SYNTAX_H
#define SYNTAX_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// ============================================================================
// PORTABILITY: Case-insensitive string comparison
// ============================================================================
// Windows uses _stricmp, POSIX uses strcasecmp. This macro provides a
// portable wrapper that works on both platforms.
// ============================================================================
#ifdef _WIN32
    #define strcasecmp _stricmp
#endif

// ============================================================================
// SYMBOL TABLE ENTRY
// ============================================================================
// Represents a single token read from the symbol table (output of the lexer).
// Each entry contains the token type name, its lexeme (source text), and the
// DFA state transition trace that produced it.
// ============================================================================
typedef struct {
    char type[64];          // Token type name (e.g. "INT", "IDENTIFIER", "SEMICOLON")
    char lexeme[256];       // The actual source text of the token
    char transitions[512];  // DFA state transition path from the lexer
    int sourceLine;         // Line number in the original .fdls file
    int sourceColumn;       // Column number in the original .fdls file
    int symbolTableLine;    // Line number in symbol_table.txt
} SymbolEntry;

typedef struct ParseNode {
    char label[128];        // Non-terminal ("VarDecl") or token type ("IDENTIFIER")
    char lexeme[256];       // Raw token text for tokens ("" for non-terminals)
    int line;               // Starting line in the .fdls file
    int column;             // Starting column in the .fdls file
    bool isToken;           // true = terminal token node, false = grammar non-terminal
    struct ParseNode **children;
    size_t childCount;
    size_t childCapacity;
} ParseNode;

// ============================================================================
// PARSER STATE
// ============================================================================
// Maintains the full state of the recursive descent parser, including the
// current and lookahead tokens, the symbol table file handle, error tracking,
// and output file for the parse trace.
// ============================================================================
typedef struct {
    FILE *symbolTable;          // Input: symbol table file (lexer output)
    FILE *output;               // Output: parse trace / results file
    char sourceFilePath[256];   // Path to the original .fdls source file
    int symbolTableLine;        // Current line cursor in symbol_table.txt
    SymbolEntry currentToken;   // The token currently being examined
    SymbolEntry prevToken;
    bool hasPrevToken;
    bool hasMoreTokens;         // Whether there are more tokens to read
    bool encounteredError;      // Whether a syntax error has been encountered
    int errorCount;             // Total number of errors detected
    int recoveryAttempts;       // Number of panic recovery attempts
    char defaultRecoveryChoice; // Uppercase recovery choice to reuse ('C', 'S', or 'A')
    bool nonInteractive;        // true in --check mode (never prompts stdin)
    ParseNode *root;
    ParseNode *currentNode;
    bool suppressTree;
} Parser;

ParseNode *addChildNode(ParseNode *parent, const char *label);
ParseNode *wrapChildrenFrom(ParseNode *parent, size_t startIdx, const char *label);
// Optional convenience helper for stamping location on newly created nodes
ParseNode *addChildNodeAt(ParseNode *parent, const char *label, int line, int column);
void printParseTree(FILE *out, const ParseNode *root);
void freeParseTree(ParseNode *node);

void reportError(Parser *parser, const char *message);
void reportErrorAfterPrev(Parser *parser, const char *message);
void reportErrorAtToken(Parser *parser, const SymbolEntry *tok, const char *message);

// ============================================================================
// PARSER API
// ============================================================================

/*
initParser - Initializes the parser by opening the symbol table file and
             reading the first token.

Parameters:
    Parser *parser          - Pointer to the Parser struct to initialize.
    const char *symTablePath - Path to the symbol table file (lexer output).
    const char *outputPath   - Path for the parse trace output file.

Returns:
    bool - true if initialization succeeded, false on error.
*/
bool initParser(Parser *parser, const char *sourceFilePath, const char *symTablePath, const char *outputPath);

/*
advanceToken - Consumes the current token and reads the next one from the
               symbol table. Prints a trace of the consumed token.

Parameters:
    Parser *parser - Pointer to the parser state.
*/
void advanceToken(Parser *parser);

/*
parseProgram - Top-level entry point for the recursive descent parser.
               Parses the entire token stream as a Program according to:
               Program = { Statement } ;

Parameters:
    Parser *parser - Pointer to the parser state.

Returns:
    bool - true if the program was parsed successfully (no errors).
*/
bool parseProgram(Parser *parser);

/*
closeParser - Closes all open file handles and cleans up parser state.

Parameters:
    Parser *parser - Pointer to the parser state to close.
*/
void closeParser(Parser *parser);

/*
parseSourceFile - High-level entry point that initializes the parser with the
                  symbol table file, runs the recursive descent parser, and
                  writes the parse trace to the output file.

Parameters:
    const char *symbolTablePath - Path to the symbol table file (lexer output).
    const char *outputPath      - Path for the parse trace output file.

Returns:
    bool - true if parsing completed successfully (no syntax errors).
*/
bool parseSourceFile(const char *sourceFilePath,
                     const char *symbolTablePath,
                     const char *outputPath,
                     ParseNode **outRoot,
                     bool nonInteractive);

void printSourceSnippet(FILE *dest, const char *sourceFilePath,
                        int targetLine, int targetCol,
                        const char *lexeme, bool useColor);

// ============================================================================
// TOKEN MATCHING HELPERS
// ============================================================================

/*
matchToken - Checks if the current token's type matches the expected string.
             If it matches, the token is consumed (advanceToken is called).
             This is the primary matching function used in recursive descent.

Parameters:
    Parser *parser      - Pointer to the parser state.
    const char *expected - Expected token type name (e.g. "INT", "SEMICOLON").

Returns:
    bool - true if the token matched and was consumed.
*/
bool matchToken(Parser *parser, const char *expected);

/*
checkToken - Peeks at the current token type without consuming it.

Parameters:
    Parser *parser      - Pointer to the parser state.
    const char *expected - Expected token type name.

Returns:
    bool - true if the current token matches the expected type.
*/
bool checkToken(Parser *parser, const char *expected);

/*
checkTokenAny - Checks if the current token matches any of the given types.

Parameters:
    Parser *parser - Pointer to the parser state.
    const char *types[] - Array of expected token type names.
    int count          - Number of types in the array.

Returns:
    bool - true if the current token matches any of the given types.
*/
bool checkTokenAny(Parser *parser, const char *types[], int count);

// ============================================================================
// RECURSIVE DESCENT PARSING FUNCTIONS
// ============================================================================

/*
Each function below implements one production rule from the Fidelis grammar.
They return bool indicating whether the production was parsed successfully.
*/

bool parseStatement(Parser *parser);
bool parseBlockStatement(Parser *parser);
bool parseSimpleStatement(Parser *parser);
bool parseVarDecl(Parser *parser);
bool parseAssignment(Parser *parser);
bool parsePrintStmt(Parser *parser);
bool parseIfStmt(Parser *parser);
bool parseSleepStmt(Parser *parser);
bool parseIdentStmt(Parser *parser);
bool parseLoopStmt(Parser *parser);
bool parseForStmt(Parser *parser);
bool parseMatchStmt(Parser *parser);
bool parseTransitionStmt(Parser *parser);
bool parseReturnStmt(Parser *parser);
bool parseBlock(Parser *parser);
bool parseExpression(Parser *parser);
bool parseTernaryExpr(Parser *parser);
bool parseLogicalOrExpr(Parser *parser);
bool parseLogicalAndExpr(Parser *parser);
bool parseEqualityExpr(Parser *parser);
bool parseRelationalExpr(Parser *parser);
bool parseAdditiveExpr(Parser *parser);
bool parseMultiplicativeExpr(Parser *parser);
bool parseUnaryExpr(Parser *parser);
bool parseExponentExpr(Parser *parser);
bool parsePrimaryExpr(Parser *parser);
bool parseType(Parser *parser);
bool parseParamList(Parser *parser);
bool parseArgList(Parser *parser);
bool parseFnDecl(Parser *parser);
bool parseReactDecl(Parser *parser);
bool parseStateDecl(Parser *parser);
bool parsePipelineExpr(Parser *parser);
bool parseDeclOrEnsureStmt(Parser *parser);

// ============================================================================
// ERROR RECOVERY
// ============================================================================

/*
panicRecovery - Implements panic-mode error recovery. Skips tokens until one
                of the synchronizing tokens is found, then resumes parsing.
                The user is prompted to confirm the recovery action.

Parameters:
    Parser *parser       - Pointer to the parser state.
    const char *syncTokens[] - Array of token types that act as sync points.
    int syncCount        - Number of sync token types.
    const char *context  - Description of what was being parsed.
*/
void panicRecovery(Parser *parser, const char *syncTokens[], int syncCount, const char *context);

/*
reportError - Reports a syntax error with the current token's position and a
              descriptive message about how to fix the issue.

Parameters:
    Parser *parser      - Pointer to the parser state.
    const char *message - Error description and fix suggestion.
*/
void reportError(Parser *parser, const char *message);

// ============================================================================
// UTILITY
// ============================================================================

/*
trimWhitespace - Trims leading and trailing whitespace from a string in-place.

Parameters:
    char *str - String to trim (modified in-place).
*/
void trimWhitespace(char *str);

#endif // SYNTAX_H