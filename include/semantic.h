#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "syntax.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define SEM_MAX_PARAMS 32

// ============================================================================
// TYPE REPRESENTATION
// ============================================================================
typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_VOID,
    TYPE_NULL,        // Type of the `null` literal
    TYPE_EMPTY_ARRAY, // Type of `[]` (compatible with any [T] or [T]?)
    TYPE_ANY,         // Internal wildcard for generic built-in parameters
    TYPE_ERROR        // Sentinel to suppress cascading type errors
} BaseType;

typedef struct {
    BaseType base;
    int arrayDepth;    // 0 = scalar, 1 = [T], 2 = [[T]], ...
    int fixedSize;     // -1 = inferred/dynamic, > 0 = fixed size [T, N]
    bool isNullable;   // Outer nullability: e.g., int?, [int]?
    bool elemNullable; // Inner element nullability: e.g., [int?]
} SemType;

// ============================================================================
// SYMBOL TABLE & LEXICAL SCOPES
// ============================================================================
typedef enum {
    SYM_VAR,
    SYM_PARAM,
    SYM_FN,
    SYM_STATE
} SymbolKind;

typedef struct {
    char name[128];
    SymbolKind kind;
    SemType type;                      // Variable/param type, or function return type
    SemType paramTypes[SEM_MAX_PARAMS];
    int paramCount;
    int line;
    int column;
    bool isUsed;
    bool isBuiltin;
} Symbol;

typedef struct Scope {
    char name[64];
    int depth;
    struct Scope *parent;
    Symbol *symbols;
    size_t count;
    size_t capacity;
} Scope;

// ============================================================================
// SEMANTIC ANALYZER STATE
// ============================================================================
typedef struct {
    FILE *output;
    char sourceFilePath[256];

    Scope *globalScope;
    Scope *currentScope;
    Scope **allScopes; // Retained for printing the complete scoped symbol table
    size_t scopeCount;
    size_t scopeCapacity;

    int errorCount;
    int warningCount;

    // Context flags for control-flow validation
    bool inFunction;
    SemType currentFnReturnType;
    char currentFnName[128];
    bool inState;
    char currentStateName[128];
    bool inReact;
    int loopDepth;
} SemanticAnalyzer;

// ============================================================================
// PUBLIC API
// ============================================================================

/*
analyzeSemantics - Runs two-pass semantic analysis over the parse tree built
                   by Phase 2 and writes a detailed report to outputPath.

Returns:
    bool - true if zero semantic errors were found, false otherwise.
*/
bool analyzeSemantics(const char *sourceFilePath,
                      const ParseNode *root,
                      const char *outputPath);

/*
formatSemType - Formats a SemType into human-readable Fidelis syntax
                (e.g., "int", "string?", "[int]", "[int]?").
*/
void formatSemType(SemType type, char *buf, size_t bufSize);
static const ParseNode *findChild(const ParseNode *node, const char *label, size_t occurrence);
static const char *nodeLexeme(const ParseNode *node);

#endif // SEMANTIC_H