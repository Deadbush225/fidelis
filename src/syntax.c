#include "syntax.h"
#include "helper.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// ============================================================================
// UTILITY MACROS
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

// Require a non-terminal with a custom error message on failure
#define REQUIRE_MSG(call, msg) \
    do { if (!(call)) { reportError(parser, (msg)); return false; } } while (0)

// Helper: ( Block | Statement )
static bool parseBlockOrStmt(Parser *parser) {
    return checkToken(parser, "LBRACE") ? parseBlock(parser) : parseStatement(parser);
}

// ============================================================================

// Variadic helpers - no more manual arrays or magic element counts
#define TOKEN_LIST(...)  ((const char *[]){ __VA_ARGS__ })
#define TOKEN_COUNT(...) ((int)(sizeof(TOKEN_LIST(__VA_ARGS__)) / sizeof(const char *)))

// EBNF primitives
#define CHECK_ANY(...)   checkTokenAny(parser, TOKEN_LIST(__VA_ARGS__), TOKEN_COUNT(__VA_ARGS__))
#define OPT(tok)         matchToken(parser, (tok))       // EBNF: [ "tok" ]
#define OPT_NOISE(word)  matchNoise(parser, (word))      // EBNF: [ "do" | "is" | "to" ]

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
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
    const char *label,
    const char *errMsg
) {
    GUARD(parser);
    REQUIRE(parseOperand(parser));
    while (checkTokenAny(parser, ops, opCount)) {
        fprintf(parser->output, "        %s: %s\n", label, parser->currentToken.lexeme);
        advanceToken(parser);
        REQUIRE_MSG(parseOperand(parser), errMsg);
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
#define LEFT_ASSOC(operandFn, label, errMsg, ...) \
    parseLeftAssoc(parser, (operandFn), TOKEN_LIST(__VA_ARGS__), TOKEN_COUNT(__VA_ARGS__), (label), (errMsg))

// ============================================================================

/*
trimWhitespace - Trims leading and trailing whitespace from a string in-place.

Parameters:
    char *str - String to trim (modified in-place).
*/
void trimWhitespace(char *str) {
    if (str == NULL || *str == '\0') return;

    // Trim leading whitespace
    char *start = str;
    while (isspace((unsigned char)*start)) {
        start++;
    }

    // If the string was all whitespace
    if (*start == '\0') {
        *str = '\0';
        return;
    }

    // Trim trailing whitespace
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }

    // Move the trimmed string to the beginning
    memmove(str, start, end - start + 1);
    str[end - start + 1] = '\0';
}

// ============================================================================
// ============================================================================
// SYMBOL TABLE PARSING
// ============================================================================
// ============================================================================

/*
readNextEntry - Reads the next token entry from the symbol table file.
                The symbol table has a fixed-width column format:
                "%-32s | %-50s | %s\n"

                Lines 1-2 are headers and are skipped automatically.
                Blank lines are also skipped.

Parameters:
    Parser *parser - Pointer to the parser state.
    SymbolEntry *entry - Pointer to the entry struct to fill.

Returns:
    bool - true if an entry was successfully read, false at EOF or on error.
*/
static bool readNextEntry(Parser *parser, SymbolEntry *entry) {
    char line[1024];

    while (fgets(line, sizeof(line), parser->symbolTable) != NULL) {
        parser->symbolTableLine++;

        // Skip empty lines
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') {
            continue;
        }

        // Skip header and divider lines
        if (strstr(line, "TOKEN TYPE") != NULL || strstr(line, "===") != NULL) {
            continue;
        }

        // Format: LINE | COL | TOKEN TYPE | LEXEME | STATE TRANSITIONS
        char *p1 = strchr(line, '|');
        if (p1 == NULL) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (p2 == NULL) continue;
        char *p3 = strchr(p2 + 1, '|');
        if (p3 == NULL) continue;

        // Extract LINE, COL, and TOKEN TYPE
        char lineField[32] = {0};
        char colField[32] = {0};
        char typeField[64] = {0};

        size_t lineLen = (size_t)(p1 - line);
        if (lineLen >= sizeof(lineField)) lineLen = sizeof(lineField) - 1;
        strncpy(lineField, line, lineLen);
        trimWhitespace(lineField);

        size_t colLen = (size_t)(p2 - (p1 + 1));
        if (colLen >= sizeof(colField)) colLen = sizeof(colField) - 1;
        strncpy(colField, p1 + 1, colLen);
        trimWhitespace(colField);

        size_t typeLen = (size_t)(p3 - (p2 + 1));
        if (typeLen >= sizeof(typeField)) typeLen = sizeof(typeField) - 1;
        strncpy(typeField, p2 + 1, typeLen);
        trimWhitespace(typeField);

        // Wrapped continuation rows have an empty TOKEN TYPE column
        if (typeField[0] == '\0') {
            continue;
        }

        // Start of LEXEME column (skip the single space after '|')
        char *lexemeStart = p3 + 1;
        if (*lexemeStart == ' ') lexemeStart++;

        // Find the separator before STATE TRANSITIONS safely (even if lexeme has '|')
        char *transSep = strstr(lexemeStart, " | STATE_");
        char *p4 = NULL;
        if (transSep != NULL) {
            p4 = transSep + 1; // points to '|'
        } else {
            p4 = strrchr(lexemeStart, '|');
        }

        char lexemeField[256] = {0};
        if (p4 != NULL && p4 > lexemeStart) {
            size_t lexLen = (size_t)(p4 - lexemeStart);
            if (lexLen >= sizeof(lexemeField)) lexLen = sizeof(lexemeField) - 1;
            strncpy(lexemeField, lexemeStart, lexLen);
        } else {
            strncpy(lexemeField, lexemeStart, sizeof(lexemeField) - 1);
        }
        trimWhitespace(lexemeField);

        // Fill entry fields
        entry->sourceLine = atoi(lineField);
        entry->sourceColumn = atoi(colField);
        entry->symbolTableLine = parser->symbolTableLine;

        strncpy(entry->type, typeField, sizeof(entry->type) - 1);
        entry->type[sizeof(entry->type) - 1] = '\0';

        strncpy(entry->lexeme, lexemeField, sizeof(entry->lexeme) - 1);
        entry->lexeme[sizeof(entry->lexeme) - 1] = '\0';

        if (p4 != NULL) {
            char *transStart = p4 + 1;
            while (*transStart == ' ') transStart++;
            strncpy(entry->transitions, transStart, sizeof(entry->transitions) - 1);
            entry->transitions[sizeof(entry->transitions) - 1] = '\0';
            size_t tlen = strlen(entry->transitions);
            while (tlen > 0 && (entry->transitions[tlen - 1] == '\n' || entry->transitions[tlen - 1] == '\r')) {
                entry->transitions[--tlen] = '\0';
            }
        } else {
            entry->transitions[0] = '\0';
        }

        // Comments are lexer output, not parser statements
        if (strcmp(entry->type, "LINE_COMMENT") == 0 ||
            strcmp(entry->type, "BLOCK_COMMENT") == 0) {
            continue;
        }

        return true;
    }

    return false; // EOF
}

// ============================================================================
// ============================================================================
// PARSER INITIALIZATION & CORE OPERATIONS
// ============================================================================
// ============================================================================

/*
initParser - Initializes the parser by opening the symbol table and output
             files, then reading the first token.

Parameters:
    Parser *parser          - Pointer to the Parser struct to initialize.
    const char *symTablePath - Path to the symbol table file.
    const char *outputPath   - Path for the parse trace output file.

Returns:
    bool - true if initialization succeeded.
*/
bool initParser(Parser *parser, const char *sourceFilePath, const char *symTablePath, const char *outputPath) {
    if (parser == NULL || symTablePath == NULL || outputPath == NULL) {
        return false;
    }

    memset(parser, 0, sizeof(Parser));

    if (sourceFilePath != NULL) {
        strncpy(parser->sourceFilePath, sourceFilePath, sizeof(parser->sourceFilePath) - 1);
        parser->sourceFilePath[sizeof(parser->sourceFilePath) - 1] = '\0';
    }

    parser->symbolTable = fopen(symTablePath, "r");
    if (parser->symbolTable == NULL) {
        fprintf(stderr, "Error: Could not open symbol table file: %s\n", symTablePath);
        return false;
    }

    parser->output = fopen(outputPath, "w");
    if (parser->output == NULL) {
        fprintf(stderr, "Error: Could not create output file: %s\n", outputPath);
        fclose(parser->symbolTable);
        parser->symbolTable = NULL;
        return false;
    }

    parser->hasMoreTokens = true;
    parser->encounteredError = false;
    parser->errorCount = 0;
    parser->recoveryAttempts = 0;

    if (!readNextEntry(parser, &parser->currentToken)) {
        parser->hasMoreTokens = false;
    }

    fprintf(parser->output, "================================================================================\n");
    fprintf(parser->output, "  FIDELIS RECURSIVE DESCENT PARSER - PARSE TRACE\n");
    fprintf(parser->output, "  Source: %s\n", parser->sourceFilePath[0] ? parser->sourceFilePath : "(unknown)");
    fprintf(parser->output, "  Input:  %s\n", symTablePath);
    fprintf(parser->output, "================================================================================\n\n");

    return true;
}

/*
advanceToken - Consumes the current token and reads the next one from the
               symbol table. Prints a trace of the consumed token.

Parameters:
    Parser *parser - Pointer to the parser state.
*/
void advanceToken(Parser *parser) {
    if (parser == NULL || !parser->hasMoreTokens) return;

    fprintf(parser->output, "  [Line %3d, Col %3d] Consumed: %-20s | Lexeme: %s\n",
            parser->currentToken.sourceLine,
            parser->currentToken.sourceColumn,
            parser->currentToken.type,
            parser->currentToken.lexeme);

    if (!readNextEntry(parser, &parser->currentToken)) {
        parser->hasMoreTokens = false;
    }
}

/*
closeParser - Closes all open file handles and cleans up parser state.

Parameters:
    Parser *parser - Pointer to the parser state to close.
*/
void closeParser(Parser *parser) {
    if (parser == NULL) return;

    if (parser->symbolTable != NULL) {
        fclose(parser->symbolTable);
        parser->symbolTable = NULL;
    }

    if (parser->output != NULL) {
        // Write summary
        fprintf(parser->output, "\n================================================================================\n");
        fprintf(parser->output, "  PARSE SUMMARY\n");
        fprintf(parser->output, "================================================================================\n");
        if (parser->errorCount == 0) {
            fprintf(parser->output, "  [OK] Parsing completed successfully - no syntax errors detected.\n");
        } else {
            fprintf(parser->output, "  ✗ Parsing completed with %d error(s).\n", parser->errorCount);
            fprintf(parser->output, "    Recovery attempts: %d\n", parser->recoveryAttempts);
        }
        fprintf(parser->output, "================================================================================\n");
        fclose(parser->output);
        parser->output = NULL;
    }
}

// ============================================================================
// ============================================================================
// TOKEN MATCHING HELPERS
// ============================================================================
// ============================================================================

/*
checkToken - Peeks at the current token type without consuming it.

Parameters:
    Parser *parser      - Pointer to the parser state.
    const char *expected - Expected token type name.

Returns:
    bool - true if the current token matches the expected type.
*/
bool checkToken(Parser *parser, const char *expected) {
    if (parser == NULL || !parser->hasMoreTokens || expected == NULL) {
        return false;
    }
    return (strcasecmp(parser->currentToken.type, expected) == 0);
}

/*
checkTokenAny - Checks if the current token matches any of the given types.

Parameters:
    Parser *parser - Pointer to the parser state.
    const char *types[] - Array of expected token type names.
    int count          - Number of types in the array.

Returns:
    bool - true if the current token matches any of the given types.
*/
bool checkTokenAny(Parser *parser, const char *types[], int count) {
    if (parser == NULL || !parser->hasMoreTokens || types == NULL) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        if (strcasecmp(parser->currentToken.type, types[i]) == 0) {
            return true;
        }
    }
    return false;
}

/*
matchToken - Checks if the current token's type matches the expected string.
             If it matches, the token is consumed.

Parameters:
    Parser *parser      - Pointer to the parser state.
    const char *expected - Expected token type name.

Returns:
    bool - true if the token matched and was consumed.
*/
bool matchToken(Parser *parser, const char *expected) {
    if (checkToken(parser, expected)) {
        advanceToken(parser);
        return true;
    }
    return false;
}

// ============================================================================
// ============================================================================
// ERROR REPORTING & RECOVERY
// ============================================================================
// ============================================================================

static void printSourceSnippet(FILE *dest, const char *sourceFilePath, int targetLine, int targetCol, const char *lexeme, bool useColor) {
    if (sourceFilePath == NULL || sourceFilePath[0] == '\0' || targetLine <= 0) {
        return;
    }

    FILE *src = fopen(sourceFilePath, "r");
    if (src == NULL) return;

    char lineBuf[1024];
    int currentLine = 0;
    bool found = false;

    while (fgets(lineBuf, sizeof(lineBuf), src) != NULL) {
        currentLine++;
        if (currentLine == targetLine) {
            found = true;
            break;
        }
    }
    fclose(src);

    if (!found) return;

    size_t len = strlen(lineBuf);
    while (len > 0 && (lineBuf[len - 1] == '\n' || lineBuf[len - 1] == '\r')) {
        lineBuf[--len] = '\0';
    }

    fprintf(dest, "|  %4d | %s\n", targetLine, lineBuf);
    fprintf(dest, "|       | ");

    int col = (targetCol > 0) ? targetCol : 1;
    for (int i = 0; i < col - 1 && (size_t)i < len; i++) {
        fputc((lineBuf[i] == '\t') ? '\t' : ' ', dest);
    }
    for (int i = (int)len; i < col - 1; i++) {
        fputc(' ', dest);
    }

    int underlineLen = (lexeme != NULL && lexeme[0] != '\0' && strcmp(lexeme, "(end of file)") != 0)
                           ? (int)strlen(lexeme)
                           : 1;

    if (useColor) fprintf(dest, "\e[1;31m");
    fputc('^', dest);
    for (int i = 1; i < underlineLen; i++) {
        fputc('~', dest);
    }
    if (useColor) fprintf(dest, "\e[0m");
    fputc('\n', dest);
}

/*
reportError - Reports a syntax error with the current token's position and a
              descriptive message about how to fix the issue.

Parameters:
    Parser *parser      - Pointer to the parser state.
    const char *message - Error description and fix suggestion.
*/
void reportError(Parser *parser, const char *message) {
    if (parser == NULL) return;

    parser->encounteredError = true;
    parser->errorCount++;

    const char *type = parser->hasMoreTokens ? parser->currentToken.type : "EOF";
    const char *lexeme = parser->hasMoreTokens ? parser->currentToken.lexeme : "(end of file)";
    const char *file = parser->sourceFilePath[0] ? parser->sourceFilePath : "(source)";
    int srcLine = parser->currentToken.sourceLine;
    int srcCol = parser->currentToken.sourceColumn;
    int symLine = parser->currentToken.symbolTableLine;

    // Print to stderr
    fprintf(stderr, "\n");
    fprintf(stderr, "|==============================================================|\n");
    fprintf(stderr, "|  \e[1;31mSYNTAX ERROR\e[0m                                                |\n");
    fprintf(stderr, "|==============================================================|\n");
    fprintf(stderr, "|  \e[1;33mLocation:\e[0m  %s:%d:%d (Line %d, Column %d | Symbol Table Line %d)\n",
            file, srcLine, srcCol, srcLine, srcCol, symLine);
    fprintf(stderr, "|  \e[1;33mToken:\e[0m     %s (\"%s\")\n", type, lexeme);
    fprintf(stderr, "|  \e[1;33mProblem:\e[0m   %s\n", message);
    fprintf(stderr, "|\n");
    printSourceSnippet(stderr, parser->sourceFilePath, srcLine, srcCol, lexeme, true);
    fprintf(stderr, "|==============================================================|\n\n");

    // Write to parse trace file
    fprintf(parser->output, "\n  |==============================================================|\n");
    fprintf(parser->output, "  |  SYNTAX ERROR                                                |\n");
    fprintf(parser->output, "  |==============================================================|\n");
    fprintf(parser->output, "  |  Location:  %s:%d:%d (Line %d, Column %d | Symbol Table Line %d)\n",
            file, srcLine, srcCol, srcLine, srcCol, symLine);
    fprintf(parser->output, "  |  Token:     %s (\"%s\")\n", type, lexeme);
    fprintf(parser->output, "  |  Problem:   %s\n", message);
    fprintf(parser->output, "  |\n");
    printSourceSnippet(parser->output, parser->sourceFilePath, srcLine, srcCol, lexeme, false);
    fprintf(parser->output, "  |==============================================================|\n\n");
}

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
void panicRecovery(Parser *parser, const char *syncTokens[], int syncCount, const char *context) {
    if (parser == NULL || syncTokens == NULL || syncCount == 0) return;

    parser->recoveryAttempts++;

    fprintf(stderr, "\n  \e[1;33m!! PANIC MODE ERROR RECOVERY\e[0m\n");
    fprintf(stderr, "  Context: %s\n", context);
    fprintf(stderr, "  Skipping tokens until a synchronization point is found...\n");

    fprintf(parser->output, "\n  --- PANIC MODE RECOVERY [%s] ---\n", context);
    fprintf(parser->output, "  Skipping tokens until synchronization point...\n");

    int userChoice = parser->defaultRecoveryChoice;

    if (userChoice != 0) {
        fprintf(stderr, "  \e[1;36mUsing default recovery option: [%c]\e[0m\n", userChoice);
    } else {
        // Prompt the user for recovery action
        fprintf(stderr, "\n  \e[1;36mRecovery Options:\e[0m\n");
        fprintf(stderr, "  [C] Continue - Skip to next sync token and continue parsing\n");
        fprintf(stderr, "  [S] Skip statement - Discard current statement and continue\n");
        fprintf(stderr, "  [A] Abort - Stop parsing entirely\n");
        fprintf(stderr, "  Uppercase choice sets the default; lowercase choice applies once\n");
        fprintf(stderr, "  \e[1;36mChoose [C/S/A]:\e[0m ");

        userChoice = fgetc(stdin);
        // Consume the rest of the line
        int ch;
        while ((ch = fgetc(stdin)) != '\n' && ch != EOF);

        if (isupper((unsigned char)userChoice)) {
            if (userChoice == 'C' || userChoice == 'S' || userChoice == 'A') {
                parser->defaultRecoveryChoice = (char)userChoice;
                fprintf(stderr, "  \e[1;32m%c saved as the default recovery option.\e[0m\n", userChoice);
            }
        }
    }

    if (userChoice == 'A' || userChoice == 'a') {
        fprintf(stderr, "  \e[1;31mParsing aborted by user.\e[0m\n");
        fprintf(parser->output, "  Parsing aborted by user during recovery.\n");
        parser->hasMoreTokens = false;
        return;
    } else if (userChoice == 'S' || userChoice == 's') {
        // Skip until we find a semicolon or a block delimiter
        const char *stmtSync[] = {"SEMICOLON", "RBRACE", "EOF"};
        while (parser->hasMoreTokens) {
            if (checkTokenAny(parser, stmtSync, 3)) {
                if (checkToken(parser, "SEMICOLON")) {
                    advanceToken(parser); // consume the semicolon
                }
                break;
            }
            advanceToken(parser);
        }
        fprintf(stderr, "  \e[1;33mStatement skipped.\e[0m\n");
        fprintf(parser->output, "  Statement skipped by user.\n");
        return;
    }

    // Default (C): Skip until a sync token is found
    int tokensSkipped = 0;
    while (parser->hasMoreTokens) {
        if (checkTokenAny(parser, syncTokens, syncCount)) {
            break;
        }
        fprintf(parser->output, "    Skipping: %s (\"%s\")\n",
                parser->currentToken.type, parser->currentToken.lexeme);
        advanceToken(parser);
        tokensSkipped++;
    }

    if (tokensSkipped > 0) {
        fprintf(stderr, "  \e[1;33mRecovery complete - %d token(s) skipped.\e[0m\n", tokensSkipped);
    } else {
        fprintf(stderr, "  \e[1;33mRecovery complete - no tokens needed skipping.\e[0m\n");
    }
    fprintf(parser->output, "  Recovery complete - %d token(s) skipped.\n", tokensSkipped);
}

// ============================================================================
// ============================================================================
// RECURSIVE DESCENT PARSING FUNCTIONS
// ============================================================================
// ============================================================================

// Forward declarations for mutually recursive functions
static bool parseStatementList(Parser *parser);

// ============================================================================
// PROGRAM
// ============================================================================
// Program = { Statement } ;
//
// The top-level production. Parses statements until EOF is reached.
// Returns true if the entire program was parsed without errors.
// ============================================================================
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
            // If parsing a statement fails, attempt recovery
            const char *sync[] = {"SEMICOLON", "RBRACE", "EOF"};
            panicRecovery(parser, sync, 3, "top-level statement");
        }
    }

    fprintf(parser->output, "─── END OF PROGRAM ───────────────────────────────────────────────\n");
    return !parser->encounteredError;
}

// ============================================================================
// STATEMENT
// ============================================================================
// Statement = BlockStatement | ( SimpleStatement, ";" ) ;
//
// Block statements are self-delimiting (they contain their own braces).
// Simple statements must be followed by a semicolon.
// ============================================================================
bool parseStatement(Parser *parser) {
    GUARD(parser);

    // Empty statement: just consume the single ';' and finish
    if (OPT("SEMICOLON")) return true;

    if (CHECK_ANY("IF", "LOOP", "FOR", "MATCH", "ENSURE", "STATE", "REACT", "FN")) {
        return parseBlockStatement(parser);
    }

    // Correctly distinguish between a variable declaration and an ensure statement
    if (isTypeStart(parser)) {
        REQUIRE(parseType(parser));
        EXPECT_LOG("IDENTIFIER", "Variable", "Expected a variable name after type.");
        OPT_NOISE("is");
        EXPECT("ASSIGNMENT", "Expected '=' in declaration.");

        if (OPT("ENSURE")) {
            // EnsureStmt — ends with Block (no ';' needed)
            REQUIRE(parseExpression(parser));
            EXPECT("ELSE", "Expected 'else' in ensure statement.");
            return parseBlock(parser);
        }

        // Normal VarDecl — ends with ';'
        REQUIRE(parseExpression(parser));
        EXPECT_SYNC("SEMICOLON", "Expected ';' after variable declaration.",
                    "declaration termination", "SEMICOLON", "RBRACE", "EOF");
        return true;
    }

    REQUIRE(parseSimpleStatement(parser));
    EXPECT_SYNC("SEMICOLON",
                "Expected ';' after statement.",
                "statement termination",
                "SEMICOLON", "RBRACE", "EOF");
    return true;
}

// ============================================================================
// BLOCK STATEMENT
// ============================================================================
// BlockStatement = IfStmt | LoopStmt | ForStmt | MatchStmt | EnsureStmt
//                 | StateDecl | ReactDecl | FnDecl ;
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

// BlockStatement = IfStmt | LoopStmt | ForStmt | MatchStmt | EnsureStmt | StateDecl | ReactDecl | FnDecl ;
bool parseBlockStatement(Parser *parser) {
    GUARD(parser);
    static const TokenRule rules[] = {
        {"IF", parseIfStmt},         {"LOOP", parseLoopStmt},
        {"FOR", parseForStmt},       {"MATCH", parseMatchStmt},
        {"ENSURE", parseEnsureStmt}, {"STATE", parseStateDecl},
        {"REACT", parseReactDecl},   {"FN", parseFnDecl},
    };

    ParseFn fn = lookupRule(parser, rules, sizeof(rules) / sizeof(rules[0]));
    if (fn != NULL) return fn(parser);

    reportError(parser, "Expected a block statement keyword (if, loop, for, match, ensure, state, react, or fn).");
    return false;
}

// ============================================================================
// SIMPLE STATEMENT
// ============================================================================
// SimpleStatement = VarDecl | Assignment | PrintStmt | TransitionStmt
//                  | ReturnStmt | ExprStmt ;
// ============================================================================
bool parseSimpleStatement(Parser *parser) {
    if (parser == NULL || !parser->hasMoreTokens) return false;

    // VarDecl: starts with a type keyword (int, float, string, bool, char, or '[' for array)
    const char *typeKeywords[] = {"INT", "FLOAT", "STRING", "BOOL", "CHAR"};
    if (checkTokenAny(parser, typeKeywords, 5) || checkToken(parser, "LBRACKET")) {
        return parseVarDecl(parser);
    }

    // PrintStmt: starts with "print"
    if (checkToken(parser, "PRINT")) {
        return parsePrintStmt(parser);
    }

    // TransitionStmt: starts with "transition"
    if (checkToken(parser, "TRANSITION")) {
        return parseTransitionStmt(parser);
    }

    // ReturnStmt: starts with "return"
    if (checkToken(parser, "RETURN")) {
        return parseReturnStmt(parser);
    }

    // SleepStmt: starts with "sleep"
    if (checkToken(parser, "SLEEP")) {
        // sleep(Expression);
        advanceToken(parser); // consume 'sleep'
        if (!matchToken(parser, "LPAREN")) {
            reportError(parser, "Expected '(' after 'sleep'. Usage: sleep(milliseconds);");
            return false;
        }
        parseExpression(parser);
        if (!matchToken(parser, "RPAREN")) {
            reportError(parser, "Expected ')' after sleep argument. Usage: sleep(milliseconds);");
            return false;
        }
        return true;
    }

    // QuitStmt: starts with "quit"
    if (checkToken(parser, "QUIT")) {
        advanceToken(parser); // consume 'quit'
        return true;
    }

    // Assignment: starts with an identifier
    if (checkToken(parser, "IDENTIFIER")) {
        return parseAssignment(parser);
    }

    // Noise word (like "do", "is", "to") - skip it silently
    if (checkToken(parser, "NOISE_WORD")) {
        advanceToken(parser);
        return true;
    }

    // Expression statement (an expression used as a statement)
    const char *exprStart[] = {
        "INT_LITERAL", "FLOAT_LITERAL", "STRING_LITERAL", "CHAR_LITERAL",
        "TRUE", "FALSE", "NULL", "LPAREN", "LBRACKET", "LBRACE",
        "LOGICAL_NOT", "SUBTRACTION", "IDENTIFIER", "READLINE", "READINT"
    };
    if (checkTokenAny(parser, exprStart, 15)) {
        parseExpression(parser);
        return true;
    }

    reportError(parser, "Unexpected token. Expected the start of a statement: "
                        "a type keyword (int, float, string, bool, char), "
                        "'print', 'transition', 'return', 'sleep', 'quit', "
                        "an identifier, or an expression.");
    return false;
}

// ============================================================================
// DECLARATION STATEMENT
// ============================================================================
// VarDecl = Type, Identifier, ["is"], "=", Expression ;
//
// Handles all 3 forms:
//   1. int x = 42;           (const value)
//   2. int x = y;            (variable assignment)
//   3. int x = a + b * 2;    (expression assignment)
// ============================================================================
bool parseVarDecl(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── VARIABLE DECLARATION ─────────────────────────────────────\n");

    // Parse the type
    REQUIRE_MSG(parseType(parser), "Invalid type in variable declaration. "
                                    "Valid types: int, float, string, bool, char, or array types like [int].");

    // Parse the identifier (variable name)
    if (!checkToken(parser, "IDENTIFIER")) {
        reportError(parser, "Expected a variable name (identifier) after the type. "
                            "Example: int myVariable = 10;");
        const char *sync[] = {"SEMICOLON", "RBRACE", "EOF"};
        panicRecovery(parser, sync, 3, "variable name");
        return false;
    }
    fprintf(parser->output, "    Variable: %s\n", parser->currentToken.lexeme);
    advanceToken(parser);

    matchNoise(parser, "is");

    // Expect '='
    if (!checkToken(parser, "ASSIGNMENT") &&
        !checkToken(parser, "ASSIGN")) {
        reportError(parser, "Expected '=' in variable declaration. "
                            "Syntax: type identifier = expression;");
        const char *sync[] = {"SEMICOLON", "RBRACE", "EOF"};
        panicRecovery(parser, sync, 3, "assignment operator");
        return false;
    }
    advanceToken(parser); // consume '='

    // Parse the expression (right-hand side)
    // This handles all 3 forms:
    //   - const value: int x = 42;
    //   - variable:    int x = y;
    //   - expression:  int x = a + b * 2;
    if (!parseExpression(parser)) {
        reportError(parser, "Expected an expression after '=' in variable declaration. "
                            "Provide a value, variable, or expression to initialize the variable.");
        return false;
    }

    fprintf(parser->output, "    ── END DECLARATION ──────────────────────────────────────────\n");
    return true;
}

// ============================================================================
// ASSIGNMENT STATEMENT
// ============================================================================
// Assignment = Identifier, AssignOp, Expression ;
//
// Handles all 3 forms:
//   1. x = y;           (variable to variable)
//   2. x = 42;          (const value to variable)
//   3. x = a + b * 2;   (expression to variable)
//
// Also handles compound assignments: +=, -=, *=, /=, %=
// ============================================================================
bool parseAssignment(Parser *parser) {
    if (parser == NULL || !parser->hasMoreTokens) return false;

    fprintf(parser->output, "    ── ASSIGNMENT ───────────────────────────────────────────────\n");

    // Parse the left-hand side identifier
    if (!checkToken(parser, "IDENTIFIER")) {
        reportError(parser, "Expected an identifier (variable name) on the left side of assignment.");
        return false;
    }
    fprintf(parser->output, "    Target: %s\n", parser->currentToken.lexeme);
    advanceToken(parser);

    // Check for array access: identifier '[' Expression ']'
    if (checkToken(parser, "LBRACKET")) {
        advanceToken(parser); // consume '['
        parseExpression(parser);
        if (!matchToken(parser, "RBRACKET")) {
            reportError(parser, "Expected ']' after array index. "
                                "Syntax: arrayName[index] = value;");
            return false;
        }
    }

    // Parse the assignment operator (=, +=, -=, *=, /=, %=)
    const char *assignOps[] = {
        "ASSIGNMENT", "ASSIGN",
        "ADD_ASSIGN", "PLUS_ASSIGN",
        "SUBTRACT_ASSIGN", "MINUS_ASSIGN",
        "MULTIPLY_ASSIGN", "STAR_ASSIGN",
        "DIVIDE_ASSIGN", "SLASH_ASSIGN",
        "MODULO_ASSIGN", "PERCENT_ASSIGN"
    };

    if (!checkTokenAny(parser, assignOps, 12)) {
        reportError(parser, "Expected an assignment operator (=, +=, -=, *=, /=, %=). "
                            "Use '=' for simple assignment or a compound operator.");
        const char *sync[] = {"SEMICOLON", "RBRACE", "EOF"};
        panicRecovery(parser, sync, 3, "assignment operator");
        return false;
    }

    const char *opLexeme = parser->currentToken.lexeme;
    fprintf(parser->output, "    Operator: %s\n", opLexeme);
    advanceToken(parser); // consume the assignment operator

    // Parse the right-hand side expression
    // This handles all 3 forms:
    //   - variable:    x = y;
    //   - const value: x = 42;
    //   - expression:  x = a + b * 2;
    if (!parseExpression(parser)) {
        reportError(parser, "Expected an expression after the assignment operator. "
                            "Provide a value, variable, or expression to assign.");
        return false;
    }

    fprintf(parser->output, "    ── END ASSIGNMENT ────────────────────────────────────────────\n");
    return true;
}

// ============================================================================
// PRINT STATEMENT (OUTPUT STATEMENT)
// ============================================================================
// PrintStmt = "print", "(", Expression, ")" ;
//
// Handles all 3 forms:
//   1. print("Hello, World!");         - Printing a string literal
//   2. print(myVariable);              - Printing a variable
//   3. print("Value: " + result);      - Printing concatenation (string + variable)
// ============================================================================
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
    REQUIRE_MSG(parseExpression(parser), "Expected an expression inside print(). "
                                         "Provide a string, variable, or concatenation to output.");
    EXPECT("RPAREN", "Expected ')' to close the print() call. "
                     "Syntax: print(expression);");

    // Classify the print statement for the trace
    // We check what was inside the parentheses by looking at what we parsed
    fprintf(parser->output, "    ── END PRINT STATEMENT ───────────────────────────────────────\n");
    return true;
}

// ============================================================================
// CONDITION STATEMENT
// ============================================================================
// IfStmt = "if", "(", Expression, ")", ( Statement | Block ),
//          [ "else", ( Statement | Block ) ] ;
//
// Handles:
//   1. if (condition) statement;
//   2. if (condition) { block }
//   3. if (condition) { block } else { block }
//   4. if (condition) { block } else if (condition) { block } else { block }
// ============================================================================
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

// ============================================================================
// ITERATIVE STATEMENTS
// ============================================================================

// --------------------------------------------------------------------------
// LoopStmt = "loop", ["do"], Block ;
//
// An infinite loop that repeats the block until a break/transition/return.
// --------------------------------------------------------------------------
bool parseLoopStmt(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── LOOP STATEMENT ────────────────────────────────────────────\n");

    EXPECT("LOOP", "Expected 'loop' keyword. Syntax: loop { ... }");
    matchNoise(parser, "do");
    REQUIRE_MSG(parseBlock(parser),
                "Expected '{' after 'loop'. The loop body must be a block: loop { ... }");

    fprintf(parser->output, "    ── END LOOP STATEMENT ─────────────────────────────────────────\n");
    return true;
}

// --------------------------------------------------------------------------
// ForStmt = "for", "(", Type, Identifier, "=", Expression, ";",
//                 Expression, ";", Assignment, ")",
//           ["do"], Block ;
//
// A C-style for loop with initialization, condition, and increment.
// --------------------------------------------------------------------------
// ForStmt = "for", "(", Type, Identifier, "=", Expression, ";",
//           Expression, ";", Assignment, ")", [ "do" ], Block ;
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

// ============================================================================
// MATCH STATEMENT
// ============================================================================
// MatchStmt = "match", Identifier, ["do"], "{", { MatchCase }, "}" ;
// MatchCase = ( StringLiteral | "_" ), "->", ( Block | SimpleStatement, ";" ) ;
// ============================================================================
// MatchStmt = "match", ( "(", Expression, ")" | Identifier ), [ "do" ], "{", { MatchCase }, "}" ;
// MatchCase = Pattern, "->", ( Block | SimpleStatement, ";" ) ;
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

// ============================================================================
// ENSURE STATEMENT
// ============================================================================
// EnsureStmt = Type, Identifier, ["is"], "=", "ensure", Expression, "else", Block ;
// ============================================================================
bool parseEnsureStmt(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── ENSURE STATEMENT ──────────────────────────────────────────\n");

    // Parse: Type Identifier ["is"] "=" "ensure"
    REQUIRE_MSG(parseType(parser), "Expected a type in ensure statement. "
                                    "Syntax: type var = ensure expression else { ... }");
    EXPECT_LOG("IDENTIFIER", "Variable name", "Expected a variable name after the type in ensure statement.");
    OPT_NOISE("is");
    EXPECT("ASSIGNMENT", "Expected '=' in ensure statement. "
                        "Syntax: type var = ensure expression else { ... }");
    EXPECT("ENSURE", "Expected 'ensure' keyword in ensure statement. "
                        "Syntax: type var = ensure expression else { ... }");
    REQUIRE(parseExpression(parser));
    EXPECT("ELSE", "Expected 'else' keyword in ensure statement. "
                        "Syntax: type var = ensure expression else { ... }");
    REQUIRE(parseBlock(parser));

    fprintf(parser->output, "    ── END ENSURE STATEMENT ───────────────────────────────────────\n");
    return true;
}

// ============================================================================
// TRANSITION STATEMENT
// ============================================================================
// TransitionStmt = "transition", ["to"], Identifier ;
// ============================================================================
bool parseTransitionStmt(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── TRANSITION STATEMENT ──────────────────────────────────────\n");

    EXPECT("TRANSITION", "Expected 'transition' keyword. Syntax: transition to StateName;");
    OPT_NOISE("to");
    EXPECT_LOG("IDENTIFIER", "Target state", "Expected a state name after 'transition'.");
    fprintf(parser->output, "    Target state: %s\n", parser->currentToken.lexeme);

    fprintf(parser->output, "    ── END TRANSITION STATEMENT ───────────────────────────────────\n");
    return true;
}

// ============================================================================
// RETURN STATEMENT
// ============================================================================
// ReturnStmt = "return", Expression ;
// ============================================================================
bool parseReturnStmt(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── RETURN STATEMENT ───────────────────────────────────────────\n");

    EXPECT("RETURN", "Expected 'return' keyword. Syntax: return expression;");

    // Parse the return value expression
    if (!checkToken(parser, "SEMICOLON") && !checkToken(parser, "RBRACE") && !checkToken(parser, "EOF")) {
        if (!parseExpression(parser)) {
            reportError(parser, "Expected an expression after 'return'. "
                                "Syntax: return value; or return;");
            return false;
        }
    }

    fprintf(parser->output, "    ── END RETURN STATEMENT ────────────────────────────────────────\n");
    return true;
}

// ============================================================================
// BLOCK
// ============================================================================
// Block = "{", { Statement }, "}" ;
// ============================================================================
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

// ============================================================================
// STATE DECLARATION
// ============================================================================
// StateDecl = "state", Identifier, ["do"], Block ;
// ============================================================================
bool parseStateDecl(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── STATE DECLARATION ──────────────────────────────────────────\n");

    EXPECT("STATE", "Expected 'state' keyword. Syntax: state StateName { ... }");
    EXPECT_LOG("IDENTIFIER", "State name", "Expected a state name (identifier) after 'state'. "
                         "Syntax: state StateName { ... }");
    fprintf(parser->output, "    State name: %s\n", parser->currentToken.lexeme);
    OPT_NOISE("do");
    REQUIRE(parseBlock(parser));

    fprintf(parser->output, "    ── END STATE DECLARATION ───────────────────────────────────────\n");
    return true;
}

// ============================================================================
// REACT DECLARATION
// ============================================================================
// ReactDecl = "react", "(", Identifier, ")", ["do"], Block ;
// ============================================================================
bool parseReactDecl(Parser *parser) {
    GUARD(parser);

    fprintf(parser->output, "    ── REACT DECLARATION ──────────────────────────────────────────\n");

    EXPECT("REACT", "Expected 'react' keyword. Syntax: react (variable) { ... }");
    EXPECT("LPAREN", "Expected '(' after 'react'. Syntax: react (variable) { ... }");

    if (!checkToken(parser, "IDENTIFIER")) {
        reportError(parser, "Expected a variable name inside react(). "
                            "Syntax: react (variableName) { ... }");
        return false;
    }
    fprintf(parser->output, "    Watching: %s\n", parser->currentToken.lexeme);
    advanceToken(parser);

    if (!matchToken(parser, "RPAREN")) {
        reportError(parser, "Expected ')' after react variable. "
                            "Syntax: react (variableName) { ... }");
        return false;
    }

    matchNoise(parser, "do");
    REQUIRE_MSG(parseBlock(parser),
                "Expected '{' for react body. Syntax: react (var) { ... }");

    fprintf(parser->output, "    ── END REACT DECLARATION ───────────────────────────────────────\n");
    return true;
}

// ============================================================================
// FUNCTION DECLARATION
// ============================================================================
// FnDecl = "fn", Identifier, "(", [ParamList], ")", ":", Type, ["do"], Block ;
// ============================================================================
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
    REQUIRE_MSG(parseType(parser), "Expected a return type after ':'.");
    matchNoise(parser, "do");
    REQUIRE(parseBlock(parser));

    return true;
}

// ============================================================================
// PARAMETER LIST
// ============================================================================
// ParamList = Type, Identifier, { ",", Type, Identifier } ;
// ============================================================================
bool parseParamList(Parser *parser) {
    GUARD(parser);
    fprintf(parser->output, "      Parameters:\n");

    int paramCount = 0;
    while (true) {
        REQUIRE_MSG(parseType(parser), "Expected a parameter type.");
        if (!checkToken(parser, "IDENTIFIER")) {
            reportError(parser, "Expected a parameter name after type.");
            return false;
        }
        paramCount++;
        char paramName[256];
        strncpy(paramName, parser->currentToken.lexeme, sizeof(paramName) - 1);
        paramName[sizeof(paramName) - 1] = '\0';
        advanceToken(parser);
        fprintf(parser->output, "        Param %d: %s\n", paramCount, paramName);
        if (!matchToken(parser, "COMMA")) break;
    }

    return true;
}

// ============================================================================
// ARGUMENT LIST
// ============================================================================
// ArgList = Expression, { ",", Expression } ;
// ============================================================================
bool parseArgList(Parser *parser) {
    GUARD(parser);
    int argCount = 0;
    while (true) {
        REQUIRE_MSG(parseExpression(parser), "Expected an expression as argument.");
        argCount++;
        if (!matchToken(parser, "COMMA")) break;
    }

    fprintf(parser->output, "      Arguments: %d\n", argCount);
    return true;
}

// ============================================================================
// TYPE
// ============================================================================
// Type = "int" | "float" | "string" | "bool" | "char"
//      | "int?" | "float?" | "string?" | "bool?" | "char?"
//      | "[", Type, "]", ["?"] ;
// ============================================================================
bool parseType(Parser *parser) {
    GUARD(parser);

    // Check for array type: '[' Type ']'
    if (checkToken(parser, "LBRACKET")) {
        advanceToken(parser); // consume '['
        REQUIRE_MSG(parseType(parser), "Expected a type inside '[...]'. Example: [int], [string]");
        EXPECT("RBRACKET", "Expected ']' to close array type.");
        // Optional '?' for nullable array type
        matchToken(parser, "QUESTION");
        return true;
    }

    // Check for primitive types
    if (isTypeStart(parser)) {
        static const char *types[] = {"INT", "FLOAT", "STRING", "BOOL", "CHAR"};
        if (!checkTokenAny(parser, types, 5)) {
            reportError(parser, "Expected a primitive type keyword.");
            return false;
        }
        fprintf(parser->output, "      Type: %s", parser->currentToken.lexeme);
        advanceToken(parser);

        // Optional '?' for nullable type
        if (matchToken(parser, "QUESTION")) fprintf(parser->output, "?");
        fprintf(parser->output, "\n");
        return true;
    }

    reportError(parser, "Expected a type keyword (int, float, string, bool, char) "
                        "or an array type ([type]).");
    return false;
}

// ============================================================================
// EXPRESSION PARSING (Operator Precedence)
// ============================================================================
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
    REQUIRE(parseLogicalOrExpr(parser));

    // Handle ternary: condition ? trueExpr : falseExpr
    if (checkToken(parser, "QUESTION")) {
        fprintf(parser->output, "        Ternary operator\n");
        advanceToken(parser); // consume '?'
        REQUIRE_MSG(parseExpression(parser), "Expected an expression after '?' in ternary. "
                                             "Syntax: condition ? valueIfTrue : valueIfFalse");
        EXPECT("COLON", "Expected ':' in ternary expression. "
                         "Syntax: condition ? valueIfTrue : valueIfFalse");
        REQUIRE_MSG(parseTernaryExpr(parser), "Expected an expression after ':' in ternary.");
    }

    return true;
}

// --------------------------------------------------------------------------
// Level 10: Pipeline (left-to-right)
// PipelineExpr = LogicalOrExpr, { "|>", FunctionCall } ;
// --------------------------------------------------------------------------
bool parseLogicalOrExpr(Parser *parser) {
    GUARD(parser);
    REQUIRE(parseLogicalAndExpr(parser));

    // Handle pipeline operator
    while (checkToken(parser, "PIPELINE")) {
        fprintf(parser->output, "        Pipeline operator\n");
        advanceToken(parser); // consume '|>'

        // After pipeline, expect a function call (identifier '(' ... ')')
        EXPECT("IDENTIFIER", "Expected a function call after '|>' pipeline operator. "
                             "Syntax: value |> functionName()");
        EXPECT("LPAREN", "Expected '(' after function name in pipeline.");
        if (!checkToken(parser, "RPAREN")) REQUIRE(parseArgList(parser));
        EXPECT("RPAREN", "Expected ')' after pipeline function arguments.");
    }

    // Handle logical OR
    while (checkToken(parser, "LOGICAL_OR") || checkToken(parser, "OR")) {
        fprintf(parser->output, "        Logical OR\n");
        advanceToken(parser); // consume '||'
        if (!parseLogicalAndExpr(parser)) {
            reportError(parser, "Expected an expression after '||'.");
            return false;
        }
    }

    return true;
}

// --------------------------------------------------------------------------
// Level 9: Logical AND (left-to-right)
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

    if (checkToken(parser, "LOGICAL_NOT") || checkToken(parser, "NOT")) {
        fprintf(parser->output, "        Unary NOT\n");
        advanceToken(parser); // consume '!'
        REQUIRE(parseUnaryExpr(parser)); // Right-to-left associativity
        return true;
    }

    if (checkToken(parser, "SUBTRACTION") || checkToken(parser, "MINUS")) {
        fprintf(parser->output, "        Unary minus\n");
        advanceToken(parser); // consume '-'
        REQUIRE(parseUnaryExpr(parser)); // Right-to-left associativity
        return true;
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

    if (checkToken(parser, "EXPONENT") || checkToken(parser, "CARET")) {
        fprintf(parser->output, "        Exponent\n");
        advanceToken(parser); // consume '^'
        REQUIRE_MSG(parseExponentExpr(parser),
                    "Expected an expression after '^' exponent operator.");
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

// ============================================================================
// ============================================================================
// PUBLIC API ENTRY POINT
// ============================================================================
// ============================================================================

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
bool parseSourceFile(const char *sourceFilePath, const char *symbolTablePath, const char *outputPath) {
    if (symbolTablePath == NULL || outputPath == NULL) {
        fprintf(stderr, "Error: Invalid file paths provided to parser.\n");
        return false;
    }

    Parser parser;
    if (!initParser(&parser, sourceFilePath, symbolTablePath, outputPath)) {
        fprintf(stderr, "Error: Failed to initialize parser.\n");
        return false;
    }

    fprintf(parser.output, "Starting recursive descent parsing...\n");
    fprintf(parser.output, "Input symbol table: %s\n\n", symbolTablePath);

    bool success = parseProgram(&parser);

    fprintf(parser.output, "\n");
    if (success) {
        fprintf(parser.output, "[OK] Program parsed successfully - no syntax errors.\n");
    } else {
        fprintf(parser.output, "✗ Program parsed with %d error(s).\n", parser.errorCount);
    }

    closeParser(&parser);
    return success;
}