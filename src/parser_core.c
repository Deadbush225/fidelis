#include "syntax.h"
#include "helper.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static ParseNode *createNode(const char *label) {
    ParseNode *node = (ParseNode *)calloc(1, sizeof(ParseNode));
    if (node == NULL) return NULL;
    if (label != NULL) {
        strncpy(node->label, label, sizeof(node->label) - 1);
        node->label[sizeof(node->label) - 1] = '\0';
    }
    return node;
}

ParseNode *addChildNode(ParseNode *parent, const char *label) {
    if (parent == NULL) return NULL;

    if (parent->childCount == parent->childCapacity) {
        size_t newCapacity = parent->childCapacity == 0 ? 4 : parent->childCapacity * 2;
        ParseNode **newChildren = (ParseNode **)realloc(
            parent->children, newCapacity * sizeof(ParseNode *));
        if (newChildren == NULL) return NULL;
        parent->children = newChildren;
        parent->childCapacity = newCapacity;
    }

    ParseNode *child = createNode(label);
    if (child == NULL) return NULL;
    parent->children[parent->childCount++] = child;
    return child;
}

ParseNode *wrapChildrenFrom(ParseNode *parent, size_t startIdx, const char *label) {
    if (parent == NULL || startIdx > parent->childCount) return NULL;

    size_t movedCount = parent->childCount - startIdx;
    ParseNode *wrapper = addChildNode(parent, label);
    if (wrapper == NULL) return NULL;

    if (movedCount > 0) {
        wrapper->children = (ParseNode **)calloc(movedCount, sizeof(ParseNode *));
        if (wrapper->children == NULL) {
            return NULL;
        }
        for (size_t i = 0; i < movedCount; i++) {
            wrapper->children[i] = parent->children[startIdx + i];
        }
        wrapper->childCount = movedCount;
        wrapper->childCapacity = movedCount;
    }

    parent->childCount = startIdx;
    parent->children[startIdx] = wrapper;
    parent->childCount = startIdx + 1;
    return wrapper;
}

static void printParseTreeRecursive(FILE *out, const ParseNode *node,
                                    bool branchStack[256], size_t depth) {
    if (out == NULL || node == NULL) return;

    for (size_t i = 1; i < depth; i++) {
        fputs(branchStack[i] ? "│   " : "    ", out);
    }
    if (depth > 0) {
        fputs(branchStack[depth] ? "├── " : "└── ", out);
    }
    fprintf(out, "%s\n", node->label);

    for (size_t i = 0; i < node->childCount; i++) {
        branchStack[depth + 1] = (i + 1 < node->childCount);
        printParseTreeRecursive(out, node->children[i], branchStack, depth + 1);
    }
}

void printParseTree(FILE *out, const ParseNode *root) {
    bool branchStack[256] = {false};
    if (out == NULL || root == NULL) return;
    printParseTreeRecursive(out, root, branchStack, 0);
}

void freeParseTree(ParseNode *node) {
    if (node == NULL) return;
    for (size_t i = 0; i < node->childCount; i++) {
        freeParseTree(node->children[i]);
    }
    free(node->children);
    free(node);
}

static bool isTreeIgnoredToken(const char *type) {
    static const char *ignored[] = {
        "EOF", "SEMICOLON", "LPAREN", "RPAREN", "LBRACE",
        "RBRACE", "COMMA", "COLON"
    };
    if (type == NULL) return true;
    for (size_t i = 0; i < sizeof(ignored) / sizeof(ignored[0]); i++) {
        if (strcasecmp(type, ignored[i]) == 0) return true;
    }
    return false;
}

static bool isTreeValuedToken(const char *type) {
    static const char *valued[] = {
        "IDENTIFIER", "INT_LITERAL", "FLOAT_LITERAL", "STRING_LITERAL",
        "CHAR_LITERAL", "TRUE", "FALSE", "NULL", "INT", "FLOAT",
        "STRING", "BOOL", "CHAR"
    };
    if (type == NULL) return false;
    for (size_t i = 0; i < sizeof(valued) / sizeof(valued[0]); i++) {
        if (strcasecmp(type, valued[i]) == 0) return true;
    }
    return false;
}

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
    parser->root = createNode("Program");
    parser->currentNode = parser->root;
    parser->suppressTree = false;

    if (!readNextEntry(parser, &parser->currentToken)) {
        parser->hasMoreTokens = false;
    }

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

    if (!parser->suppressTree && parser->currentNode != NULL &&
        !isTreeIgnoredToken(parser->currentToken.type)) {
        ParseNode *typeNode = addChildNode(parser->currentNode, parser->currentToken.type);
        if (typeNode != NULL && isTreeValuedToken(parser->currentToken.type)) {
            addChildNode(typeNode, parser->currentToken.lexeme);
        }
    }

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
        fclose(parser->output);
        parser->output = NULL;
    }

    freeParseTree(parser->root);
    parser->root = NULL;
    parser->currentNode = NULL;
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
static void printErrorBox(FILE *dest, Parser *parser, const char *message, bool useColor) {
    const char *type   = parser->hasMoreTokens ? parser->currentToken.type   : "EOF";
    const char *lexeme = parser->hasMoreTokens ? parser->currentToken.lexeme : "(end of file)";
    const char *file   = parser->sourceFilePath[0] ? parser->sourceFilePath  : "(source)";
    const char *indent = useColor ? "" : "  ";
    const char *red    = useColor ? "\e[1;31m" : "";
    const char *yellow = useColor ? "\e[1;33m" : "";
    const char *reset  = useColor ? "\e[0m"    : "";
    int srcLine = parser->currentToken.sourceLine;
    int srcCol  = parser->currentToken.sourceColumn;
    int symLine = parser->currentToken.symbolTableLine;

    fprintf(dest, "\n%s|==============================================================|\n", indent);
    fprintf(dest, "%s|  %sSYNTAX ERROR%s                                                |\n", indent, red, reset);
    fprintf(dest, "%s|==============================================================|\n", indent);
    fprintf(dest, "%s|  %sLocation:%s  %s:%d:%d (Line %d, Column %d | Symbol Table Line %d)\n",
            indent, yellow, reset, file, srcLine, srcCol, srcLine, srcCol, symLine);
    fprintf(dest, "%s|  %sToken:%s     %s (\"%s\")\n", indent, yellow, reset, type, lexeme);
    fprintf(dest, "%s|  %sProblem:%s   %s\n", indent, yellow, reset, message);
    fprintf(dest, "%s|\n", indent);
    printSourceSnippet(dest, parser->sourceFilePath, srcLine, srcCol, lexeme, useColor);
    fprintf(dest, "%s|==============================================================|\n\n", indent);
}

void reportError(Parser *parser, const char *message) {
    if (parser == NULL) return;
    parser->encounteredError = true;
    parser->errorCount++;
    printErrorBox(stderr, parser, message, true);
    printErrorBox(parser->output, parser, message, false);
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
    if (parser == NULL) return;
    parser->suppressTree = true;
    if (syncTokens == NULL || syncCount == 0) {
        parser->suppressTree = false;
        return;
    }

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
        parser->suppressTree = false;
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
        parser->suppressTree = false;
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
    parser->suppressTree = false;
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

    bool success = parseProgram(&parser);
    printParseTree(parser.output, parser.root);

    closeParser(&parser);
    return success;
}