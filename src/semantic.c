#include "semantic.h"
#include "helper.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ━━━━━━━━━━━━━━━━━━━━━━━━━ Helpers ━━━━━━━━━━━━━━━━━━━━━━━━ */
// Find the first child after a given token label (e.g., expression after "ASSIGNMENT")
const ParseNode *childAfter(const ParseNode *node, const char *markerLabel) {
    if (node == NULL || markerLabel == NULL) return NULL;
    for (size_t i = 0; i + 1 < node->childCount; i++) {
        if (strcmp(node->children[i]->label, markerLabel) == 0) {
            return node->children[i + 1];
        }
    }
    return NULL;
}
/* ━━━━━━━━━━━━━━━━━━━━━━━ End Helper ━━━━━━━━━━━━━━━━━━━━━━━ */

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
static void analyzeStatement(SemanticAnalyzer *sa, const ParseNode *stmtNode);
static void analyzeBlock(SemanticAnalyzer *sa, const ParseNode *blockNode,
                         const char *scopeName, bool createScope);
static SemType analyzeExpression(SemanticAnalyzer *sa, const ParseNode *exprNode);
static bool nodeAlwaysExits(const ParseNode *node);
static bool nodeAlwaysReturns(const ParseNode *node);

// ============================================================================
// TYPE CONSTRUCTORS & COMPARISON HELPERS
// ============================================================================
static SemType makeType(BaseType base, int arrayDepth, bool isNullable, bool elemNullable) {
    SemType t;
    t.base = base;
    t.arrayDepth = arrayDepth;
    t.fixedSize = -1;
    t.isNullable = isNullable;
    t.elemNullable = elemNullable;
    return t;
}

static SemType makePrimitive(BaseType base) {
    return makeType(base, 0, false, false);
}

static SemType makeNullablePrimitive(BaseType base) {
    return makeType(base, 0, true, false);
}

static SemType makeErrorType(void) {
    return makeType(TYPE_ERROR, 0, false, false);
}

static bool isErrorType(SemType t) {
    return t.base == TYPE_ERROR;
}

static bool isNumeric(SemType t) {
    return t.arrayDepth == 0 && !t.isNullable &&
           (t.base == TYPE_INT || t.base == TYPE_FLOAT);
}

static bool isInt(SemType t) {
    return t.arrayDepth == 0 && !t.isNullable && t.base == TYPE_INT;
}

static bool isBool(SemType t) {
    return t.arrayDepth == 0 && !t.isNullable && t.base == TYPE_BOOL;
}

static bool isString(SemType t) {
    return t.arrayDepth == 0 && !t.isNullable && t.base == TYPE_STRING;
}

static bool typesEqualExact(SemType a, SemType b) {
    return a.base == b.base &&
           a.arrayDepth == b.arrayDepth &&
           a.isNullable == b.isNullable &&
           a.elemNullable == b.elemNullable;
}

/*
isAssignable - Checks if a value of type `src` can be assigned/passed to `dst`.
Rules:
  - TYPE_ERROR always succeeds (suppresses cascading errors).
  - `null` is assignable to any nullable target (T? or [T]?).
  - `[]` (TYPE_EMPTY_ARRAY) is assignable to any array target ([T] or [T]?).
  - Non-nullable T is assignable to nullable T? (subtyping).
  - Nullable T? is NOT assignable to non-nullable T (requires `ensure`).
  - `int` widens to `float` (and `int` widens to `float?`).
*/
static bool isAssignable(SemType dst, SemType src) {
    if (isErrorType(dst) || isErrorType(src)) return true;
    if (dst.base == TYPE_ANY || src.base == TYPE_ANY) return true;

    if (src.base == TYPE_NULL) {
        return dst.isNullable;
    }

    if (src.base == TYPE_EMPTY_ARRAY) {
        return dst.arrayDepth > 0;
    }

    if (dst.arrayDepth != src.arrayDepth) {
        return false;
    }

    // Cannot assign nullable value to a non-nullable destination
    if (src.isNullable && !dst.isNullable) {
        return false;
    }

    if (dst.arrayDepth > 0 && src.elemNullable && !dst.elemNullable) {
        return false;
    }

    if (dst.base == src.base) {
        return true;
    }

    // Scalar numeric widening: int -> float (or float?)
    if (dst.arrayDepth == 0 && dst.base == TYPE_FLOAT && src.base == TYPE_INT) {
        return true;
    }

    return false;
}

static SemType commonType(SemType a, SemType b, bool *ok) {
    *ok = true;
    if (isErrorType(a) || isErrorType(b)) return makeErrorType();

    if (a.base == TYPE_NULL && b.base == TYPE_NULL) {
        return makeType(TYPE_NULL, 0, true, false);
    }
    if (a.base == TYPE_NULL) {
        SemType res = b;
        res.isNullable = true;
        return res;
    }
    if (b.base == TYPE_NULL) {
        SemType res = a;
        res.isNullable = true;
        return res;
    }
    if (a.base == TYPE_EMPTY_ARRAY && b.arrayDepth > 0) return b;
    if (b.base == TYPE_EMPTY_ARRAY && a.arrayDepth > 0) return a;

    if (isAssignable(a, b)) {
        SemType res = a;
        res.isNullable = a.isNullable || b.isNullable;
        return res;
    }
    if (isAssignable(b, a)) {
        SemType res = b;
        res.isNullable = a.isNullable || b.isNullable;
        return res;
    }

    *ok = false;
    return makeErrorType();
}

void formatSemType(SemType type, char *buf, size_t bufSize) {
    if (buf == NULL || bufSize == 0) return;

    const char *baseStr = "unknown";
    switch (type.base) {
        case TYPE_INT:         baseStr = "int"; break;
        case TYPE_FLOAT:       baseStr = "float"; break;
        case TYPE_STRING:      baseStr = "string"; break;
        case TYPE_BOOL:        baseStr = "bool"; break;
        case TYPE_CHAR:        baseStr = "char"; break;
        case TYPE_VOID:        baseStr = "void"; break;
        case TYPE_NULL:        baseStr = "null"; break;
        case TYPE_EMPTY_ARRAY: baseStr = "[]"; break;
        case TYPE_ANY:         baseStr = "any"; break;
        case TYPE_ERROR:       baseStr = "<error>"; break;
    }

    if (type.arrayDepth <= 0) {
        snprintf(buf, bufSize, "%s%s", baseStr, type.isNullable ? "?" : "");
        return;
    }

    char inner[128];
    snprintf(inner, sizeof(inner), "%s%s", baseStr, type.elemNullable ? "?" : "");

    char tmp[128];
    for (int d = 0; d < type.arrayDepth; d++) {
        if (d == type.arrayDepth - 1 && type.fixedSize > 0) {
            snprintf(tmp, sizeof(tmp), "[%s, %d]", inner, type.fixedSize);
        } else {
            snprintf(tmp, sizeof(tmp), "[%s]", inner);
        }
        strncpy(inner, tmp, sizeof(inner) - 1);
        inner[sizeof(inner) - 1] = '\0';
    }

    snprintf(buf, bufSize, "%s%s", inner, type.isNullable ? "?" : "");
}

// ============================================================================
// PARSE TREE QUERY HELPERS
// ============================================================================
static bool isPunctuationNode(const ParseNode *node) {
    if (node == NULL) return true;
    static const char *punct[] = {
        "LPAREN", "RPAREN", "LBRACE", "RBRACE",
        "SEMICOLON", "COMMA", "COLON"
    };
    for (size_t i = 0; i < sizeof(punct) / sizeof(punct[0]); i++) {
        if (strcmp(node->label, punct[i]) == 0) return true;
    }
    return false;
}

static bool isLabel(const ParseNode *node, const char *label) {
    return node != NULL && label != NULL && strcmp(node->label, label) == 0;
}

// Safely retrieves token text whether stamped on node->lexeme or stored as child leaf
static const char *nodeLexeme(const ParseNode *node) {
    if (node == NULL) return "";
    if (node->lexeme[0] != '\0') return node->lexeme;
    if (node->childCount > 0 && node->children[0] != NULL) {
        return node->children[0]->label;
    }
    return node->label;
}

static const ParseNode *findChild(const ParseNode *node, const char *label, size_t occurrence) {
    if (node == NULL || label == NULL) return NULL;
    size_t seen = 0;
    for (size_t i = 0; i < node->childCount; i++) {
        if (isLabel(node->children[i], label)) {
            if (seen == occurrence) return node->children[i];
            seen++;
        }
    }
    return NULL;
}

// Returns the first non-punctuation child after `markerLabel` (e.g., after "ASSIGNMENT")
static const ParseNode *firstMeaningfulAfter(const ParseNode *node, const char *markerLabel, size_t occurrence) {
    if (node == NULL || markerLabel == NULL) return NULL;
    size_t seen = 0;
    for (size_t i = 0; i < node->childCount; i++) {
        if (isLabel(node->children[i], markerLabel)) {
            if (seen == occurrence) {
                for (size_t j = i + 1; j < node->childCount; j++) {
                    if (!isPunctuationNode(node->children[j])) {
                        return node->children[j];
                    }
                }
                return NULL;
            }
            seen++;
        }
    }
    return NULL;
}

// Finds the first child between two indices that is not punctuation
static const ParseNode *firstMeaningfulInRange(const ParseNode *node, size_t start, size_t end) {
    if (node == NULL) return NULL;
    if (end > node->childCount) end = node->childCount;
    for (size_t i = start; i < end; i++) {
        if (!isPunctuationNode(node->children[i])) {
            return node->children[i];
        }
    }
    return NULL;
}

static int findChildIndex(const ParseNode *node, const char *label, size_t occurrence) {
    if (node == NULL || label == NULL) return -1;
    size_t seen = 0;
    for (size_t i = 0; i < node->childCount; i++) {
        if (isLabel(node->children[i], label)) {
            if (seen == occurrence) return (int)i;
            seen++;
        }
    }
    return -1;
}

// ============================================================================
// DIAGNOSTICS (MATCHING parser_core.c BOX FORMAT)
// ============================================================================
static void printDiagBox(FILE *dest, SemanticAnalyzer *sa, const ParseNode *node,
                         const char *severity, const char *message, bool useColor) {
    const char *file   = sa->sourceFilePath[0] ? sa->sourceFilePath : "(source)";
    const char *indent = useColor ? "" : "  ";
    const char *isErr  = (strcmp(severity, "ERROR") == 0) ? "1" : NULL;
    const char *color  = useColor ? (isErr ? "\e[1;31m" : "\e[1;35m") : "";
    const char *yellow = useColor ? "\e[1;33m" : "";
    const char *reset  = useColor ? "\e[0m" : "";

    int line = (node != NULL) ? node->line : 0;
    int col  = (node != NULL) ? node->column : 0;
    const char *lex = (node != NULL) ? nodeLexeme(node) : "";

    fprintf(dest, "\n%s|==============================================================|\n", indent);
    fprintf(dest, "%s|  %sSEMANTIC %s%s\n", indent, color, severity, reset);
    fprintf(dest, "%s|==============================================================|\n", indent);
    if (line > 0) {
        fprintf(dest, "%s|  %sLocation:%s  %s:%d:%d (Line %d, Column %d)\n",
                indent, yellow, reset, file, line, col, line, col);
    } else {
        fprintf(dest, "%s|  %sLocation:%s  %s\n", indent, yellow, reset, file);
    }
    fprintf(dest, "%s|  %sProblem:%s   %s\n", indent, yellow, reset, message);
    if (line > 0 && sa->sourceFilePath[0] != '\0') {
        fprintf(dest, "%s|\n", indent);
        printSourceSnippet(dest, sa->sourceFilePath, line, col, lex, useColor);
    }
    fprintf(dest, "%s|==============================================================|\n\n", indent);
}

static void semError(SemanticAnalyzer *sa, const ParseNode *node, const char *fmt, ...) {
    char msg[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    sa->errorCount++;
    printDiagBox(stderr, sa, node, "ERROR", msg, true);
    if (sa->output != NULL) {
        printDiagBox(sa->output, sa, node, "ERROR", msg, false);
    }
}

static void semWarning(SemanticAnalyzer *sa, const ParseNode *node, const char *fmt, ...) {
    char msg[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    sa->warningCount++;
    printDiagBox(stderr, sa, node, "WARNING", msg, true);
    if (sa->output != NULL) {
        printDiagBox(sa->output, sa, node, "WARNING", msg, false);
    }
}

// ============================================================================
// SCOPE & SYMBOL TABLE MANAGEMENT
// ============================================================================
static Scope *pushScope(SemanticAnalyzer *sa, const char *name) {
    Scope *scope = (Scope *)calloc(1, sizeof(Scope));
    if (scope == NULL) return NULL;

    strncpy(scope->name, name ? name : "block", sizeof(scope->name) - 1);
    scope->parent = sa->currentScope;
    scope->depth = (sa->currentScope != NULL) ? sa->currentScope->depth + 1 : 0;

    if (sa->scopeCount == sa->scopeCapacity) {
        size_t newCap = (sa->scopeCapacity == 0) ? 8 : sa->scopeCapacity * 2;
        Scope **grown = (Scope **)realloc(sa->allScopes, newCap * sizeof(Scope *));
        if (grown == NULL) {
            free(scope);
            return NULL;
        }
        sa->allScopes = grown;
        sa->scopeCapacity = newCap;
    }
    sa->allScopes[sa->scopeCount++] = scope;
    sa->currentScope = scope;
    return scope;
}

static void popScope(SemanticAnalyzer *sa) {
    if (sa->currentScope != NULL && sa->currentScope->parent != NULL) {
        sa->currentScope = sa->currentScope->parent;
    }
}

static Symbol *lookupInScope(Scope *scope, const char *name) {
    if (scope == NULL || name == NULL) return NULL;
    for (size_t i = 0; i < scope->count; i++) {
        if (strcmp(scope->symbols[i].name, name) == 0) {
            return &scope->symbols[i];
        }
    }
    return NULL;
}

static Symbol *lookupSymbol(Scope *scope, const char *name) {
    for (Scope *s = scope; s != NULL; s = s->parent) {
        Symbol *found = lookupInScope(s, name);
        if (found != NULL) return found;
    }
    return NULL;
}

static Symbol *defineSymbol(SemanticAnalyzer *sa, Scope *scope, const Symbol *sym,
                            const ParseNode *errNode) {
    if (scope == NULL || sym == NULL) return NULL;

    Symbol *existing = lookupInScope(scope, sym->name);
    if (existing != NULL) {
        semError(sa, errNode,
                 "Redeclaration of '%s' in scope '%s' (previously declared at line %d).",
                 sym->name, scope->name, existing->line);
        return NULL;
    }

    if (scope->count == scope->capacity) {
        size_t newCap = (scope->capacity == 0) ? 8 : scope->capacity * 2;
        Symbol *grown = (Symbol *)realloc(scope->symbols, newCap * sizeof(Symbol));
        if (grown == NULL) return NULL;
        scope->symbols = grown;
        scope->capacity = newCap;
    }

    scope->symbols[scope->count] = *sym;
    return &scope->symbols[scope->count++];
}

static void registerBuiltins(SemanticAnalyzer *sa) {
    Symbol sym;

    // readLine() : string
    memset(&sym, 0, sizeof(sym));
    strcpy(sym.name, "readLine");
    sym.kind = SYM_FN;
    sym.type = makePrimitive(TYPE_STRING);
    sym.paramCount = 0;
    sym.isBuiltin = true;
    defineSymbol(sa, sa->globalScope, &sym, NULL);

    // readInt() : int?
    memset(&sym, 0, sizeof(sym));
    strcpy(sym.name, "readInt");
    sym.kind = SYM_FN;
    sym.type = makeNullablePrimitive(TYPE_INT);
    sym.paramCount = 0;
    sym.isBuiltin = true;
    defineSymbol(sa, sa->globalScope, &sym, NULL);

    // length(array | string) : int  (validated specially for array or string)
    memset(&sym, 0, sizeof(sym));
    strcpy(sym.name, "length");
    sym.kind = SYM_FN;
    sym.type = makePrimitive(TYPE_INT);
    sym.paramCount = 1;
    sym.paramTypes[0] = makePrimitive(TYPE_ANY);
    sym.isBuiltin = true;
    defineSymbol(sa, sa->globalScope, &sym, NULL);

    // replace(string, string, string) : string
    memset(&sym, 0, sizeof(sym));
    strcpy(sym.name, "replace");
    sym.kind = SYM_FN;
    sym.type = makePrimitive(TYPE_STRING);
    sym.paramCount = 3;
    sym.paramTypes[0] = makePrimitive(TYPE_STRING);
    sym.paramTypes[1] = makePrimitive(TYPE_STRING);
    sym.paramTypes[2] = makePrimitive(TYPE_STRING);
    sym.isBuiltin = true;
    defineSymbol(sa, sa->globalScope, &sym, NULL);
}

// ============================================================================
// TYPE NODE RESOLUTION
// ============================================================================
// Type = ( "int" | "float" | "string" | "bool" | "char" ), [ "?" ]
//      | "[", Type, "]", [ "?" ] ;
static SemType resolveTypeNode(SemanticAnalyzer *sa, const ParseNode *typeNode) {
    if (typeNode == NULL || !isLabel(typeNode, "Type")) {
        return makeErrorType();
    }

    bool outerNullable = (findChild(typeNode, "QUESTION", 0) != NULL);

    // Array type: LBRACKET Type RBRACKET [QUESTION]
    const ParseNode *innerTypeNode = findChild(typeNode, "Type", 0);
    if (innerTypeNode != NULL) {
        SemType inner = resolveTypeNode(sa, innerTypeNode);
        if (isErrorType(inner)) return inner;

        SemType arr = inner;
        if (inner.arrayDepth == 0) {
            arr.elemNullable = inner.isNullable;
        }
        arr.arrayDepth = inner.arrayDepth + 1;
        arr.isNullable = outerNullable;
        arr.fixedSize = -1;

        const ParseNode *sizeNode = findChild(typeNode, "INT_LITERAL", 0);
        if (sizeNode != NULL) {
            long long sz = strtoll(nodeLexeme(sizeNode), NULL, 10);
            if (sz <= 0) {
                semError(sa, sizeNode, "Fixed-size array length must be a positive integer, got %lld.", sz);
                return makeErrorType();
            }
            arr.fixedSize = (int)sz;
        }
        return arr;
    }

    // Primitive type
    if (findChild(typeNode, "INT", 0) != NULL)    return makeType(TYPE_INT, 0, outerNullable, false);
    if (findChild(typeNode, "FLOAT", 0) != NULL)  return makeType(TYPE_FLOAT, 0, outerNullable, false);
    if (findChild(typeNode, "STRING", 0) != NULL) return makeType(TYPE_STRING, 0, outerNullable, false);
    if (findChild(typeNode, "BOOL", 0) != NULL)   return makeType(TYPE_BOOL, 0, outerNullable, false);
    if (findChild(typeNode, "CHAR", 0) != NULL)   return makeType(TYPE_CHAR, 0, outerNullable, false);

    semError(sa, typeNode, "Unrecognized type specification.");
    return makeErrorType();
}

// ============================================================================
// FUNCTION CALL & PIPELINE VALIDATION
// ============================================================================
static SemType validateCall(SemanticAnalyzer *sa, const ParseNode *callNode,
                            const SemType *pipedArgType) {
    if (callNode == NULL) return makeErrorType();

    // Determine callee name (built-in READLINE/READINT token or IDENTIFIER)
    const ParseNode *calleeNode = findChild(callNode, "READLINE", 0);
    const char *fnName = NULL;
    if (calleeNode != NULL) {
        fnName = "readLine";
    } else if ((calleeNode = findChild(callNode, "READINT", 0)) != NULL) {
        fnName = "readInt";
    } else {
        calleeNode = findChild(callNode, "IDENTIFIER", 0);
        fnName = nodeLexeme(calleeNode);
    }

    if (calleeNode == NULL || fnName == NULL || fnName[0] == '\0') {
        semError(sa, callNode, "Malformed function call: missing function name.");
        return makeErrorType();
    }

    // Collect explicit argument expressions from ArgList
    const ParseNode *argNodes[SEM_MAX_PARAMS];
    SemType argTypes[SEM_MAX_PARAMS];
    int totalArgs = 0;

    if (pipedArgType != NULL) {
        argNodes[totalArgs] = callNode;
        argTypes[totalArgs] = *pipedArgType;
        totalArgs++;
    }

    const ParseNode *argList = findChild(callNode, "ArgList", 0);
    if (argList != NULL) {
        for (size_t i = 0; i < argList->childCount; i++) {
            const ParseNode *child = argList->children[i];
            if (isPunctuationNode(child)) continue;
            if (totalArgs >= SEM_MAX_PARAMS) {
                semError(sa, child, "Too many arguments in call to '%s' (max %d).",
                         fnName, SEM_MAX_PARAMS);
                break;
            }
            argNodes[totalArgs] = child;
            argTypes[totalArgs] = analyzeExpression(sa, child);
            totalArgs++;
        }
    }

    Symbol *fnSym = lookupSymbol(sa->currentScope, fnName);
    if (fnSym == NULL) {
        semError(sa, calleeNode, "Call to undeclared function '%s'.", fnName);
        return makeErrorType();
    }
    if (fnSym->kind != SYM_FN) {
        semError(sa, calleeNode, "'%s' is not a function and cannot be called.", fnName);
        return makeErrorType();
    }
    fnSym->isUsed = true;

    // Arity check
    if (totalArgs != fnSym->paramCount) {
        semError(sa, calleeNode,
                 "Function '%s' expects %d argument(s), but got %d.",
                 fnName, fnSym->paramCount, totalArgs);
        return fnSym->type;
    }

    // Special check for built-in length(x): requires non-nullable array or string
    if (fnSym->isBuiltin && strcmp(fnName, "length") == 0) {
        SemType t = argTypes[0];
        if (!isErrorType(t)) {
            if (t.isNullable) {
                char got[64];
                formatSemType(t, got, sizeof(got));
                semError(sa, argNodes[0],
                         "Cannot pass nullable value of type '%s' to length(). Unwrap it with 'ensure' first.",
                         got);
            } else if (t.arrayDepth == 0 && t.base != TYPE_STRING) {
                char got[64];
                formatSemType(t, got, sizeof(got));
                semError(sa, argNodes[0],
                         "length() expects an array or string, but got '%s'.", got);
            }
        }
        return fnSym->type;
    }

    // Argument type checks
    for (int i = 0; i < totalArgs; i++) {
        if (!isAssignable(fnSym->paramTypes[i], argTypes[i])) {
            char expBuf[64], gotBuf[64];
            formatSemType(fnSym->paramTypes[i], expBuf, sizeof(expBuf));
            formatSemType(argTypes[i], gotBuf, sizeof(gotBuf));
            semError(sa, argNodes[i],
                     "Argument %d of '%s' expects type '%s', but got '%s'.",
                     i + 1, fnName, expBuf, gotBuf);
        }
    }

    return fnSym->type;
}

// ============================================================================
// EXPRESSION ANALYSIS
// ============================================================================
static SemType analyzeBinaryChain(SemanticAnalyzer *sa, const ParseNode *node) {
    // Collect meaningful children: expr0, op0, expr1, op1, expr2, ...
    const ParseNode *items[128];
    size_t count = 0;
    for (size_t i = 0; i < node->childCount && count < 128; i++) {
        if (!isPunctuationNode(node->children[i])) {
            items[count++] = node->children[i];
        }
    }
    if (count == 0) return makeErrorType();

    SemType acc = analyzeExpression(sa, items[0]);

    for (size_t i = 1; i + 1 < count; i += 2) {
        const ParseNode *opNode = items[i];
        const ParseNode *rhsNode = items[i + 1];
        SemType rhs = analyzeExpression(sa, rhsNode);

        if (isErrorType(acc) || isErrorType(rhs)) {
            acc = makeErrorType();
            continue;
        }

        const char *op = opNode->label;

        // Logical: ||, &&
        if (strcmp(op, "LOGICAL_OR") == 0 || strcmp(op, "LOGICAL_AND") == 0) {
            if (!isBool(acc) || !isBool(rhs)) {
                char lBuf[64], rBuf[64];
                formatSemType(acc, lBuf, sizeof(lBuf));
                formatSemType(rhs, rBuf, sizeof(rBuf));
                semError(sa, opNode,
                         "Logical operator '%s' requires 'bool' operands, got '%s' and '%s'.",
                         nodeLexeme(opNode), lBuf, rBuf);
                acc = makeErrorType();
            } else {
                acc = makePrimitive(TYPE_BOOL);
            }
            continue;
        }

        // Equality: ==, !=
        if (strcmp(op, "EQUALITY") == 0 || strcmp(op, "INEQUALITY") == 0) {
            bool ok = false;
            if (acc.base == TYPE_NULL && rhs.isNullable) ok = true;
            else if (rhs.base == TYPE_NULL && acc.isNullable) ok = true;
            else if (acc.base == TYPE_NULL && rhs.base == TYPE_NULL) ok = true;
            else if (isNumeric(acc) && isNumeric(rhs)) ok = true;
            else if (isAssignable(acc, rhs) || isAssignable(rhs, acc)) ok = true;

            if (!ok) {
                char lBuf[64], rBuf[64];
                formatSemType(acc, lBuf, sizeof(lBuf));
                formatSemType(rhs, rBuf, sizeof(rBuf));
                semError(sa, opNode,
                         "Cannot compare incompatible types '%s' and '%s' with '%s'.",
                         lBuf, rBuf, nodeLexeme(opNode));
                acc = makeErrorType();
            } else {
                acc = makePrimitive(TYPE_BOOL);
            }
            continue;
        }

        // Relational: <, >, <=, >=
        if (strcmp(op, "LESS_THAN") == 0 || strcmp(op, "GREATER_THAN") == 0 ||
            strcmp(op, "LESS_EQUAL") == 0 || strcmp(op, "GREATER_EQUAL") == 0) {
            bool valid = (isNumeric(acc) && isNumeric(rhs)) ||
                         (acc.arrayDepth == 0 && rhs.arrayDepth == 0 &&
                          !acc.isNullable && !rhs.isNullable &&
                          acc.base == TYPE_CHAR && rhs.base == TYPE_CHAR);
            if (!valid) {
                char lBuf[64], rBuf[64];
                formatSemType(acc, lBuf, sizeof(lBuf));
                formatSemType(rhs, rBuf, sizeof(rBuf));
                semError(sa, opNode,
                         "Relational operator '%s' requires non-nullable numeric (or char) operands, got '%s' and '%s'.",
                         nodeLexeme(opNode), lBuf, rBuf);
                acc = makeErrorType();
            } else {
                acc = makePrimitive(TYPE_BOOL);
            }
            continue;
        }

        // Addition / String Concatenation: +
        if (strcmp(op, "ADDITION") == 0) {
            if (isString(acc) || isString(rhs)) {
                if (acc.isNullable || rhs.isNullable || acc.base == TYPE_NULL || rhs.base == TYPE_NULL) {
                    semError(sa, opNode,
                             "Cannot concatenate a nullable or null value with '+' directly. Unwrap with 'ensure' first.");
                    acc = makeErrorType();
                } else {
                    acc = makePrimitive(TYPE_STRING);
                }
            } else if (isNumeric(acc) && isNumeric(rhs)) {
                acc = (acc.base == TYPE_FLOAT || rhs.base == TYPE_FLOAT)
                          ? makePrimitive(TYPE_FLOAT)
                          : makePrimitive(TYPE_INT);
            } else {
                char lBuf[64], rBuf[64];
                formatSemType(acc, lBuf, sizeof(lBuf));
                formatSemType(rhs, rBuf, sizeof(rBuf));
                semError(sa, opNode,
                         "Operator '+' cannot be applied to '%s' and '%s'.", lBuf, rBuf);
                acc = makeErrorType();
            }
            continue;
        }

        // Subtraction, Multiplication, Division, Exponent: -, *, /, ^
        if (strcmp(op, "SUBTRACTION") == 0 || strcmp(op, "MULTIPLICATION") == 0 ||
            strcmp(op, "DIVISION") == 0 || strcmp(op, "EXPONENT") == 0) {
            if (!isNumeric(acc) || !isNumeric(rhs)) {
                char lBuf[64], rBuf[64];
                formatSemType(acc, lBuf, sizeof(lBuf));
                formatSemType(rhs, rBuf, sizeof(rBuf));
                semError(sa, opNode,
                         "Arithmetic operator '%s' requires non-nullable numeric operands, got '%s' and '%s'.",
                         nodeLexeme(opNode), lBuf, rBuf);
                acc = makeErrorType();
            } else {
                acc = (acc.base == TYPE_FLOAT || rhs.base == TYPE_FLOAT)
                          ? makePrimitive(TYPE_FLOAT)
                          : makePrimitive(TYPE_INT);
            }
            continue;
        }

        // Integer division (\) and Modulo (%)
        if (strcmp(op, "INTEGER_DIVISION") == 0 || strcmp(op, "MODULO") == 0) {
            if (!isInt(acc) || !isInt(rhs)) {
                char lBuf[64], rBuf[64];
                formatSemType(acc, lBuf, sizeof(lBuf));
                formatSemType(rhs, rBuf, sizeof(rBuf));
                semError(sa, opNode,
                         "Operator '%s' requires non-nullable 'int' operands, got '%s' and '%s'.",
                         nodeLexeme(opNode), lBuf, rBuf);
                acc = makeErrorType();
            } else {
                acc = makePrimitive(TYPE_INT);
            }
            continue;
        }
    }

    return acc;
}

static SemType analyzeExpression(SemanticAnalyzer *sa, const ParseNode *node) {
    if (node == NULL) return makeErrorType();

    // ---- Literals ----
    if (isLabel(node, "INT_LITERAL"))    return makePrimitive(TYPE_INT);
    if (isLabel(node, "FLOAT_LITERAL"))  return makePrimitive(TYPE_FLOAT);
    if (isLabel(node, "STRING_LITERAL")) return makePrimitive(TYPE_STRING);
    if (isLabel(node, "CHAR_LITERAL"))   return makePrimitive(TYPE_CHAR);
    if (isLabel(node, "TRUE"))           return makePrimitive(TYPE_BOOL);
    if (isLabel(node, "FALSE"))          return makePrimitive(TYPE_BOOL);
    if (isLabel(node, "NULL"))           return makeType(TYPE_NULL, 0, true, false);

    // ---- Identifier lookup ----
    if (isLabel(node, "IDENTIFIER")) {
        const char *name = nodeLexeme(node);
        Symbol *sym = lookupSymbol(sa->currentScope, name);
        if (sym == NULL) {
            semError(sa, node, "Use of undeclared identifier '%s'.", name);
            return makeErrorType();
        }
        if (sym->kind == SYM_STATE) {
            semError(sa, node, "State '%s' cannot be used as a value in an expression.", name);
            return makeErrorType();
        }
        if (sym->kind == SYM_FN) {
            semError(sa, node, "Function '%s' must be called with '()'.", name);
            return makeErrorType();
        }
        sym->isUsed = true;
        return sym->type;
    }

    // ---- Binary & exponent chains ----
    if (isLabel(node, "LogicalOrExpr") || isLabel(node, "LogicalAndExpr") ||
        isLabel(node, "EqualityExpr")  || isLabel(node, "RelationalExpr") ||
        isLabel(node, "AdditiveExpr")  || isLabel(node, "MultiplicativeExpr") ||
        isLabel(node, "ExponentExpr")) {
        return analyzeBinaryChain(sa, node);
    }

    // ---- UnaryExpr: ('!' | '-') operand ----
    if (isLabel(node, "UnaryExpr")) {
        const ParseNode *opNode = (node->childCount > 0) ? node->children[0] : NULL;
        const ParseNode *operand = firstMeaningfulInRange(node, 1, node->childCount);
        SemType opType = analyzeExpression(sa, operand);
        if (isErrorType(opType)) return makeErrorType();

        if (isLabel(opNode, "LOGICAL_NOT")) {
            if (!isBool(opType)) {
                char buf[64];
                formatSemType(opType, buf, sizeof(buf));
                semError(sa, opNode, "Unary '!' requires a 'bool' operand, got '%s'.", buf);
                return makeErrorType();
            }
            return makePrimitive(TYPE_BOOL);
        }
        if (isLabel(opNode, "SUBTRACTION")) {
            if (!isNumeric(opType)) {
                char buf[64];
                formatSemType(opType, buf, sizeof(buf));
                semError(sa, opNode, "Unary '-' requires a non-nullable numeric operand, got '%s'.", buf);
                return makeErrorType();
            }
            return opType;
        }
        return makeErrorType();
    }

    // ---- TernaryExpr: cond ? trueExpr : falseExpr ----
    if (isLabel(node, "TernaryExpr")) {
        int qIdx = findChildIndex(node, "QUESTION", 0);
        int cIdx = findChildIndex(node, "COLON", 0);
        if (qIdx < 0 || cIdx < 0) return makeErrorType();

        const ParseNode *condNode  = firstMeaningfulInRange(node, 0, (size_t)qIdx);
        const ParseNode *trueNode  = firstMeaningfulInRange(node, (size_t)qIdx + 1, (size_t)cIdx);
        const ParseNode *falseNode = firstMeaningfulInRange(node, (size_t)cIdx + 1, node->childCount);

        SemType condType  = analyzeExpression(sa, condNode);
        SemType trueType  = analyzeExpression(sa, trueNode);
        SemType falseType = analyzeExpression(sa, falseNode);

        if (!isErrorType(condType) && !isBool(condType)) {
            char buf[64];
            formatSemType(condType, buf, sizeof(buf));
            semError(sa, condNode, "Ternary condition must evaluate to 'bool', got '%s'.", buf);
        }

        bool ok = false;
        SemType result = commonType(trueType, falseType, &ok);
        if (!ok) {
            char tBuf[64], fBuf[64];
            formatSemType(trueType, tBuf, sizeof(tBuf));
            formatSemType(falseType, fBuf, sizeof(fBuf));
            semError(sa, node,
                     "Ternary branches have incompatible types: '%s' and '%s'.", tBuf, fBuf);
            return makeErrorType();
        }
        return result;
    }

    // ---- PipelineExpr: lhs |> fn1(...) |> fn2(...) ----
    if (isLabel(node, "PipelineExpr")) {
        int firstPipe = findChildIndex(node, "PIPELINE", 0);
        if (firstPipe < 0) return makeErrorType();

        const ParseNode *lhsNode = firstMeaningfulInRange(node, 0, (size_t)firstPipe);
        SemType current = analyzeExpression(sa, lhsNode);

        for (size_t i = (size_t)firstPipe; i < node->childCount; i++) {
            if (isLabel(node->children[i], "PIPELINE")) {
                const ParseNode *stage = firstMeaningfulInRange(node, i + 1, node->childCount);
                if (stage != NULL && isLabel(stage, "FunctionCall")) {
                    current = validateCall(sa, stage, &current);
                } else {
                    semError(sa, node->children[i], "Expected a function call after '|>' operator.");
                    return makeErrorType();
                }
            }
        }
        return current;
    }

    // ---- FunctionCall ----
    if (isLabel(node, "FunctionCall")) {
        return validateCall(sa, node, NULL);
    }

    // ---- ArrayLiteral: [ ArgList? ] ----
    if (isLabel(node, "ArrayLiteral")) {
        const ParseNode *argList = findChild(node, "ArgList", 0);
        if (argList == NULL) {
            return makeType(TYPE_EMPTY_ARRAY, 1, false, false);
        }

        SemType elemType = makeErrorType();
        bool hasFirst = false;

        for (size_t i = 0; i < argList->childCount; i++) {
            const ParseNode *elemNode = argList->children[i];
            if (isPunctuationNode(elemNode)) continue;

            SemType cur = analyzeExpression(sa, elemNode);
            if (isErrorType(cur)) return makeErrorType();

            if (!hasFirst) {
                elemType = cur;
                hasFirst = true;
            } else {
                bool ok = false;
                SemType merged = commonType(elemType, cur, &ok);
                if (!ok) {
                    char eBuf[64], cBuf[64];
                    formatSemType(elemType, eBuf, sizeof(eBuf));
                    formatSemType(cur, cBuf, sizeof(cBuf));
                    semError(sa, elemNode,
                             "Array literal elements must have compatible types (expected '%s', got '%s').",
                             eBuf, cBuf);
                    return makeErrorType();
                }
                elemType = merged;
            }
        }

        if (!hasFirst) return makeType(TYPE_EMPTY_ARRAY, 1, false, false);

        SemType arr = elemType;
        if (elemType.arrayDepth == 0) {
            arr.elemNullable = elemType.isNullable;
        }
        arr.arrayDepth = elemType.arrayDepth + 1;
        arr.isNullable = false;
        return arr;
    }

    // ---- ArrayAccess: IDENTIFIER [ expr ] ----
    if (isLabel(node, "ArrayAccess")) {
        const ParseNode *arrIdent = findChild(node, "IDENTIFIER", 0);
        const ParseNode *idxExpr  = firstMeaningfulAfter(node, "LBRACKET", 0);

        SemType arrType = analyzeExpression(sa, arrIdent);
        SemType idxType = analyzeExpression(sa, idxExpr);

        if (!isErrorType(idxType) && !isInt(idxType)) {
            char buf[64];
            formatSemType(idxType, buf, sizeof(buf));
            semError(sa, idxExpr, "Array index must be of type 'int', got '%s'.", buf);
        }

        if (isErrorType(arrType)) return makeErrorType();

        if (arrType.isNullable) {
            char buf[64];
            formatSemType(arrType, buf, sizeof(buf));
            semError(sa, arrIdent,
                     "Cannot index nullable value of type '%s'. Unwrap with 'ensure' first.", buf);
            return makeErrorType();
        }
        if (arrType.arrayDepth <= 0) {
            char buf[64];
            formatSemType(arrType, buf, sizeof(buf));
            semError(sa, arrIdent, "Cannot index non-array type '%s'.", buf);
            return makeErrorType();
        }

        SemType elem = arrType;
        elem.arrayDepth--;
        elem.isNullable = (elem.arrayDepth == 0) ? arrType.elemNullable : false;
        return elem;
    }

    // ---- PropertyAccess: IDENTIFIER . prop ----
    if (isLabel(node, "PropertyAccess")) {
        const ParseNode *objNode  = findChild(node, "IDENTIFIER", 0);
        const ParseNode *propNode = findChild(node, "IDENTIFIER", 1);

        SemType objType = analyzeExpression(sa, objNode);
        if (isErrorType(objType)) return makeErrorType();

        const char *prop = nodeLexeme(propNode);
        if (objType.isNullable) {
            char buf[64];
            formatSemType(objType, buf, sizeof(buf));
            semError(sa, propNode,
                     "Cannot access property '.%s' on nullable type '%s'. Unwrap with 'ensure' first.",
                     prop, buf);
            return makeErrorType();
        }

        if (strcmp(prop, "length") == 0) {
            if (objType.arrayDepth > 0 || objType.base == TYPE_STRING) {
                return makePrimitive(TYPE_INT);
            }
            char buf[64];
            formatSemType(objType, buf, sizeof(buf));
            semError(sa, propNode, "Property '.length' is only valid on arrays and strings, not '%s'.", buf);
            return makeErrorType();
        }

        char buf[64];
        formatSemType(objType, buf, sizeof(buf));
        semError(sa, propNode, "Unknown property '.%s' on type '%s'.", prop, buf);
        return makeErrorType();
    }

    return makeErrorType();
}

// ============================================================================
// CONTROL-FLOW EXIT & RETURN-PATH CHECKERS
// ============================================================================
static bool nodeAlwaysExits(const ParseNode *node) {
    if (node == NULL) return false;

    if (isLabel(node, "TransitionStmt") || isLabel(node, "ReturnStmt") || isLabel(node, "QUIT")) {
        return true;
    }

    if (isLabel(node, "Statement") || isLabel(node, "SimpleStatement") || isLabel(node, "BlockStatement")) {
        for (size_t i = 0; i < node->childCount; i++) {
            if (nodeAlwaysExits(node->children[i])) return true;
        }
        return false;
    }

    if (isLabel(node, "Block")) {
        for (size_t i = 0; i < node->childCount; i++) {
            if (isLabel(node->children[i], "Statement") && nodeAlwaysExits(node->children[i])) {
                return true;
            }
        }
        return false;
    }

    if (isLabel(node, "IfStmt")) {
        int elseIdx = findChildIndex(node, "ELSE", 0);
        if (elseIdx < 0) return false;

        const ParseNode *thenBranch = NULL;
        for (size_t i = 0; i < (size_t)elseIdx; i++) {
            if (isLabel(node->children[i], "Block") || isLabel(node->children[i], "Statement")) {
                thenBranch = node->children[i];
                break;
            }
        }
        const ParseNode *elseBranch = firstMeaningfulInRange(node, (size_t)elseIdx + 1, node->childCount);
        return nodeAlwaysExits(thenBranch) && nodeAlwaysExits(elseBranch);
    }

    if (isLabel(node, "MatchStmt")) {
        bool hasDefault = false;
        bool allExit = true;
        size_t cases = 0;
        for (size_t i = 0; i < node->childCount; i++) {
            const ParseNode *mc = node->children[i];
            if (!isLabel(mc, "MatchCase")) continue;
            cases++;
            const ParseNode *pat = (mc->childCount > 0) ? mc->children[0] : NULL;
            if (isLabel(pat, "IDENTIFIER") && strcmp(nodeLexeme(pat), "_") == 0) {
                hasDefault = true;
            }
            const ParseNode *body = firstMeaningfulAfter(mc, "ARROW", 0);
            if (!nodeAlwaysExits(body)) allExit = false;
        }
        return cases > 0 && hasDefault && allExit;
    }

    return false;
}

static bool nodeAlwaysReturns(const ParseNode *node) {
    if (node == NULL) return false;

    if (isLabel(node, "ReturnStmt")) return true;

    if (isLabel(node, "Statement") || isLabel(node, "SimpleStatement") || isLabel(node, "BlockStatement")) {
        for (size_t i = 0; i < node->childCount; i++) {
            if (nodeAlwaysReturns(node->children[i])) return true;
        }
        return false;
    }

    if (isLabel(node, "Block")) {
        for (size_t i = 0; i < node->childCount; i++) {
            if (isLabel(node->children[i], "Statement") && nodeAlwaysReturns(node->children[i])) {
                return true;
            }
        }
        return false;
    }

    if (isLabel(node, "IfStmt")) {
        int elseIdx = findChildIndex(node, "ELSE", 0);
        if (elseIdx < 0) return false;

        const ParseNode *thenBranch = NULL;
        for (size_t i = 0; i < (size_t)elseIdx; i++) {
            if (isLabel(node->children[i], "Block") || isLabel(node->children[i], "Statement")) {
                thenBranch = node->children[i];
                break;
            }
        }
        const ParseNode *elseBranch = firstMeaningfulInRange(node, (size_t)elseIdx + 1, node->childCount);
        return nodeAlwaysReturns(thenBranch) && nodeAlwaysReturns(elseBranch);
    }

    if (isLabel(node, "MatchStmt")) {
        bool hasDefault = false;
        bool allReturn = true;
        size_t cases = 0;
        for (size_t i = 0; i < node->childCount; i++) {
            const ParseNode *mc = node->children[i];
            if (!isLabel(mc, "MatchCase")) continue;
            cases++;
            const ParseNode *pat = (mc->childCount > 0) ? mc->children[0] : NULL;
            if (isLabel(pat, "IDENTIFIER") && strcmp(nodeLexeme(pat), "_") == 0) {
                hasDefault = true;
            }
            const ParseNode *body = firstMeaningfulAfter(mc, "ARROW", 0);
            if (!nodeAlwaysReturns(body)) allReturn = false;
        }
        return cases > 0 && hasDefault && allReturn;
    }

    return false;
}

// ============================================================================
// STATEMENT & DECLARATION ANALYSIS
// ============================================================================
static void analyzeVarDecl(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *typeNode = findChild(node, "Type", 0);
    const ParseNode *nameNode = findChild(node, "IDENTIFIER", 0);
    const ParseNode *initExpr = firstMeaningfulAfter(node, "ASSIGNMENT", 0);

    SemType declType = resolveTypeNode(sa, typeNode);

    if (initExpr == NULL) {
        if (!isErrorType(declType) && !(declType.arrayDepth > 0 && declType.fixedSize > 0)) {
            char dBuf[64];
            formatSemType(declType, dBuf, sizeof(dBuf));
            semError(sa, nameNode ? nameNode : node,
                     "Variable '%s' of type '%s' requires an initializer. Only fixed-size arrays (e.g., '[int, 10]') may be declared empty.",
                     nodeLexeme(nameNode), dBuf);
        }
    } else {
        SemType initType = analyzeExpression(sa, initExpr);
        if (!isAssignable(declType, initType)) {
            char dBuf[64], iBuf[64];
            formatSemType(declType, dBuf, sizeof(dBuf));
            formatSemType(initType, iBuf, sizeof(iBuf));
            semError(sa, initExpr,
                     "Cannot initialize variable '%s' of type '%s' with value of type '%s'.",
                     nodeLexeme(nameNode), dBuf, iBuf);
        }

        // Fixed-size array literal bounds check
        if (!isErrorType(declType) && declType.arrayDepth > 0 && declType.fixedSize > 0 &&
            isLabel(initExpr, "ArrayLiteral")) {
            int elemCount = 0;
            const ParseNode *argList = findChild(initExpr, "ArgList", 0);
            if (argList != NULL) {
                for (size_t i = 0; i < argList->childCount; i++) {
                    if (!isPunctuationNode(argList->children[i])) elemCount++;
                }
            }
            if (elemCount > declType.fixedSize) {
                semError(sa, initExpr,
                         "Array literal has %d element(s), exceeding fixed capacity %d of '%s'.",
                         elemCount, declType.fixedSize, nodeLexeme(nameNode));
            }
        }
    }

    Symbol sym;
    memset(&sym, 0, sizeof(sym));
    strncpy(sym.name, nodeLexeme(nameNode), sizeof(sym.name) - 1);
    sym.kind = SYM_VAR;
    sym.type = declType;
    sym.line = nameNode ? nameNode->line : node->line;
    sym.column = nameNode ? nameNode->column : node->column;
    defineSymbol(sa, sa->currentScope, &sym, nameNode);
}

static void analyzeEnsureStmt(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *typeNode  = findChild(node, "Type", 0);
    const ParseNode *nameNode  = findChild(node, "IDENTIFIER", 0);
    const ParseNode *exprNode  = firstMeaningfulAfter(node, "ENSURE", 0);
    const ParseNode *elseBlock = findChild(node, "Block", 0);

    SemType targetType;
    if (typeNode != NULL) {
        targetType = resolveTypeNode(sa, typeNode);
        if (!isErrorType(targetType) && targetType.isNullable) {
            semWarning(sa, typeNode,
                       "Variable '%s' in 'ensure' statement is declared nullable; 'ensure' is meant to unwrap into a non-nullable type.",
                       nodeLexeme(nameNode));
        }
    } else {
        Symbol *existing = lookupSymbol(sa->currentScope, nodeLexeme(nameNode));
        if (existing == NULL || existing->kind != SYM_VAR) {
            semError(sa, nameNode ? nameNode : node,
                     "Undeclared variable '%s' in 'ensure' statement.",
                     nodeLexeme(nameNode));
            targetType = makeErrorType();
        } else {
            targetType = existing->type;
            if (!isErrorType(targetType) && targetType.isNullable) {
                semWarning(sa, nameNode,
                           "Variable '%s' in 'ensure' statement is nullable; 'ensure' is meant to unwrap into a non-nullable type.",
                           nodeLexeme(nameNode));
            }
        }
    }

    SemType exprType = analyzeExpression(sa, exprNode);

    if (!isErrorType(exprType)) {
        if (!exprType.isNullable && exprType.base != TYPE_NULL) {
            char eBuf[64];
            formatSemType(exprType, eBuf, sizeof(eBuf));
            semWarning(sa, exprNode,
                       "Expression in 'ensure' has non-nullable type '%s' (unwrapping is redundant).", eBuf);
        }

        // Unwrapped type must be assignable to target variable's type
        SemType unwrapped = exprType;
        unwrapped.isNullable = false;
        if (!isErrorType(targetType) && exprType.base != TYPE_NULL && !isAssignable(targetType, unwrapped)) {
            char dBuf[64], uBuf[64];
            formatSemType(targetType, dBuf, sizeof(dBuf));
            formatSemType(unwrapped, uBuf, sizeof(uBuf));
            semError(sa, exprNode,
                     "'ensure' unwrapped type '%s' does not match variable type '%s'.",
                     uBuf, dBuf);
        }
    }

    // Validate the else block BEFORE defining a newly declared variable in scope
    if (elseBlock != NULL) {
        analyzeBlock(sa, elseBlock, "ensure_else", true);
        if (!nodeAlwaysExits(elseBlock)) {
            semError(sa, elseBlock,
                     "The 'else' block of an 'ensure' statement must exit the current path (using 'transition', 'return', or 'quit').");
        }
    }

    // Only register a new symbol if 'Type' was provided
    if (typeNode != NULL) {
        Symbol sym;
        memset(&sym, 0, sizeof(sym));
        strncpy(sym.name, nodeLexeme(nameNode), sizeof(sym.name) - 1);
        sym.kind = SYM_VAR;
        sym.type = targetType;
        sym.line = nameNode ? nameNode->line : node->line;
        sym.column = nameNode ? nameNode->column : node->column;
        defineSymbol(sa, sa->currentScope, &sym, nameNode);
    }
}

static void analyzeAssignment(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *targetNode = findChild(node, "IDENTIFIER", 0);
    if (targetNode == NULL) return;

    const char *name = nodeLexeme(targetNode);
    Symbol *sym = lookupSymbol(sa->currentScope, name);
    SemType targetType = makeErrorType();

    if (sym == NULL) {
        semError(sa, targetNode, "Assignment to undeclared variable '%s'.", name);
    } else if (sym->kind != SYM_VAR && sym->kind != SYM_PARAM) {
        semError(sa, targetNode, "'%s' is not a variable and cannot be assigned to.", name);
    } else {
        sym->isUsed = true;
        targetType = sym->type;
    }

    // Check optional array indexing: target[index] op rhs
    int lbIdx = findChildIndex(node, "LBRACKET", 0);
    int rbIdx = findChildIndex(node, "RBRACKET", 0);
    size_t opSearchStart = 1;

    if (lbIdx >= 0 && rbIdx > lbIdx) {
        const ParseNode *idxExpr = firstMeaningfulInRange(node, (size_t)lbIdx + 1, (size_t)rbIdx);
        SemType idxType = analyzeExpression(sa, idxExpr);
        if (!isErrorType(idxType) && !isInt(idxType)) {
            char buf[64];
            formatSemType(idxType, buf, sizeof(buf));
            semError(sa, idxExpr, "Array index in assignment must be 'int', got '%s'.", buf);
        }

        if (!isErrorType(targetType)) {
            if (targetType.isNullable) {
                char buf[64];
                formatSemType(targetType, buf, sizeof(buf));
                semError(sa, targetNode,
                         "Cannot index nullable array '%s' of type '%s'. Unwrap with 'ensure' first.",
                         name, buf);
                targetType = makeErrorType();
            } else if (targetType.arrayDepth <= 0) {
                char buf[64];
                formatSemType(targetType, buf, sizeof(buf));
                semError(sa, targetNode, "Cannot index non-array variable '%s' of type '%s'.", name, buf);
                targetType = makeErrorType();
            } else {
                targetType.arrayDepth--;
                targetType.isNullable = (targetType.arrayDepth == 0) ? sym->type.elemNullable : false;
            }
        }
        opSearchStart = (size_t)rbIdx + 1;
    }

    // Locate assignment operator and RHS expression
    const ParseNode *opNode = NULL;
    size_t opIdx = 0;
    for (size_t i = opSearchStart; i < node->childCount; i++) {
        const char *lbl = node->children[i]->label;
        if (strcmp(lbl, "ASSIGNMENT") == 0 || strcmp(lbl, "ADD_ASSIGN") == 0 ||
            strcmp(lbl, "SUBTRACT_ASSIGN") == 0 || strcmp(lbl, "MULTIPLY_ASSIGN") == 0 ||
            strcmp(lbl, "DIVIDE_ASSIGN") == 0 || strcmp(lbl, "MODULO_ASSIGN") == 0) {
            opNode = node->children[i];
            opIdx = i;
            break;
        }
    }
    if (opNode == NULL) return;

    const ParseNode *rhsExpr = firstMeaningfulInRange(node, opIdx + 1, node->childCount);
    SemType rhsType = analyzeExpression(sa, rhsExpr);
    if (isErrorType(targetType) || isErrorType(rhsType)) return;

    const char *op = opNode->label;
    if (strcmp(op, "ASSIGNMENT") == 0) {
        if (!isAssignable(targetType, rhsType)) {
            char tBuf[64], rBuf[64];
            formatSemType(targetType, tBuf, sizeof(tBuf));
            formatSemType(rhsType, rBuf, sizeof(rBuf));
            semError(sa, rhsExpr ? rhsExpr : opNode,
                     "Cannot assign value of type '%s' to '%s' (type '%s').", rBuf, name, tBuf);
        }
    } else if (strcmp(op, "ADD_ASSIGN") == 0) {
        bool valid = (isNumeric(targetType) && isNumeric(rhsType) && isAssignable(targetType, rhsType)) ||
                     (isString(targetType) && !rhsType.isNullable && rhsType.base != TYPE_NULL);
        if (!valid) {
            char tBuf[64], rBuf[64];
            formatSemType(targetType, tBuf, sizeof(tBuf));
            formatSemType(rhsType, rBuf, sizeof(rBuf));
            semError(sa, opNode, "Operator '+=' cannot combine '%s' with '%s'.", tBuf, rBuf);
        }
    } else if (strcmp(op, "MODULO_ASSIGN") == 0) {
        if (!isInt(targetType) || !isInt(rhsType)) {
            char tBuf[64], rBuf[64];
            formatSemType(targetType, tBuf, sizeof(tBuf));
            formatSemType(rhsType, rBuf, sizeof(rBuf));
            semError(sa, opNode, "Operator '%%=' requires 'int' operands, got '%s' and '%s'.", tBuf, rBuf);
        }
    } else {
        // -=, *=, /=
        if (!isNumeric(targetType) || !isNumeric(rhsType) || !isAssignable(targetType, rhsType)) {
            char tBuf[64], rBuf[64];
            formatSemType(targetType, tBuf, sizeof(tBuf));
            formatSemType(rhsType, rBuf, sizeof(rBuf));
            semError(sa, opNode,
                     "Compound operator '%s' cannot be applied to '%s' and '%s'.",
                     nodeLexeme(opNode), tBuf, rBuf);
        }
    }
}

static void analyzeIfStmt(SemanticAnalyzer *sa, const ParseNode *node) {
    // Condition is the first meaningful child after IF
    const ParseNode *condExpr = firstMeaningfulAfter(node, "IF", 0);
    SemType condType = analyzeExpression(sa, condExpr);
    if (!isErrorType(condType) && !isBool(condType)) {
        char buf[64];
        formatSemType(condType, buf, sizeof(buf));
        semError(sa, condExpr, "'if' condition must evaluate to 'bool', got '%s'.", buf);
    }

    int elseIdx = findChildIndex(node, "ELSE", 0);
    size_t thenLimit = (elseIdx >= 0) ? (size_t)elseIdx : node->childCount;

    for (size_t i = 0; i < thenLimit; i++) {
        if (isLabel(node->children[i], "Block")) {
            analyzeBlock(sa, node->children[i], "if_then", true);
            break;
        } else if (isLabel(node->children[i], "Statement")) {
            pushScope(sa, "if_then");
            analyzeStatement(sa, node->children[i]);
            popScope(sa);
            break;
        }
    }

    if (elseIdx >= 0) {
        const ParseNode *elseNode = firstMeaningfulInRange(node, (size_t)elseIdx + 1, node->childCount);
        if (isLabel(elseNode, "IfStmt")) {
            analyzeIfStmt(sa, elseNode);
        } else if (isLabel(elseNode, "Block")) {
            analyzeBlock(sa, elseNode, "if_else", true);
        } else if (isLabel(elseNode, "Statement")) {
            pushScope(sa, "if_else");
            analyzeStatement(sa, elseNode);
            popScope(sa);
        }
    }
}

static void analyzeLoopStmt(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *body = findChild(node, "Block", 0);
    sa->loopDepth++;
    analyzeBlock(sa, body, "loop", true);
    sa->loopDepth--;
}

static void analyzeForStmt(SemanticAnalyzer *sa, const ParseNode *node) {
    pushScope(sa, "for");

    const ParseNode *typeNode   = findChild(node, "Type", 0);
    const ParseNode *varNode    = findChild(node, "IDENTIFIER", 0);
    const ParseNode *initExpr   = firstMeaningfulAfter(node, "ASSIGNMENT", 0);
    const ParseNode *condExpr   = firstMeaningfulAfter(node, "SEMICOLON", 0);
    const ParseNode *updateNode = findChild(node, "Assignment", 0);
    const ParseNode *bodyBlock  = findChild(node, "Block", 0);

    // 1. Loop variable declaration
    SemType varType  = resolveTypeNode(sa, typeNode);
    SemType initType = analyzeExpression(sa, initExpr);
    if (!isAssignable(varType, initType)) {
        char vBuf[64], iBuf[64];
        formatSemType(varType, vBuf, sizeof(vBuf));
        formatSemType(initType, iBuf, sizeof(iBuf));
        semError(sa, initExpr,
                 "Cannot initialize loop variable '%s' of type '%s' with '%s'.",
                 nodeLexeme(varNode), vBuf, iBuf);
    }

    Symbol sym;
    memset(&sym, 0, sizeof(sym));
    strncpy(sym.name, nodeLexeme(varNode), sizeof(sym.name) - 1);
    sym.kind = SYM_VAR;
    sym.type = varType;
    sym.line = varNode ? varNode->line : node->line;
    sym.column = varNode ? varNode->column : node->column;
    defineSymbol(sa, sa->currentScope, &sym, varNode);

    // 2. Condition
    SemType condType = analyzeExpression(sa, condExpr);
    if (!isErrorType(condType) && !isBool(condType)) {
        char buf[64];
        formatSemType(condType, buf, sizeof(buf));
        semError(sa, condExpr, "'for' loop condition must evaluate to 'bool', got '%s'.", buf);
    }

    // 3. Increment
    if (updateNode != NULL) {
        analyzeAssignment(sa, updateNode);
    }

    // 4. Body (shares the 'for' scope so loop variable can't be redeclared in body)
    sa->loopDepth++;
    analyzeBlock(sa, bodyBlock, "for_body", false);
    sa->loopDepth--;

    popScope(sa);
}

static void analyzeMatchStmt(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *subjectNode = firstMeaningfulAfter(node, "MATCH", 0);
    SemType subjectType = analyzeExpression(sa, subjectNode);

    bool seenWildcard = false;
    for (size_t i = 0; i < node->childCount; i++) {
        const ParseNode *mc = node->children[i];
        if (!isLabel(mc, "MatchCase")) continue;

        int arrowIdx = findChildIndex(mc, "ARROW", 0);
        if (arrowIdx < 0) continue;

        if (seenWildcard) {
            semWarning(sa, mc, "Unreachable match case after wildcard '_'.");
        }

        const ParseNode *firstPat = firstMeaningfulInRange(mc, 0, (size_t)arrowIdx);
        bool isWildcard = isLabel(firstPat, "IDENTIFIER") && strcmp(nodeLexeme(firstPat), "_") == 0;

        if (isWildcard) {
            seenWildcard = true;
        } else if (firstPat != NULL) {
            SemType patType = makeErrorType();
            if (isLabel(firstPat, "SUBTRACTION") || isLabel(firstPat, "ADDITION")) {
                const ParseNode *numNode = firstMeaningfulInRange(mc, 1, (size_t)arrowIdx);
                patType = analyzeExpression(sa, numNode);
            } else {
                patType = analyzeExpression(sa, firstPat);
            }

            if (!isErrorType(subjectType) && !isErrorType(patType)) {
                bool compatible = typesEqualExact(subjectType, patType) ||
                                  (subjectType.isNullable && patType.base == TYPE_NULL) ||
                                  isAssignable(subjectType, patType);
                if (!compatible) {
                    char sBuf[64], pBuf[64];
                    formatSemType(subjectType, sBuf, sizeof(sBuf));
                    formatSemType(patType, pBuf, sizeof(pBuf));
                    semError(sa, firstPat,
                             "Match case pattern of type '%s' does not match subject type '%s'.",
                             pBuf, sBuf);
                }
            }
        }

        const ParseNode *body = firstMeaningfulInRange(mc, (size_t)arrowIdx + 1, mc->childCount);
        if (isLabel(body, "Block")) {
            analyzeBlock(sa, body, "match_case", true);
        } else if (body != NULL) {
            pushScope(sa, "match_case");
            analyzeStatement(sa, body);
            popScope(sa);
        }
    }
}

static void analyzeTransitionStmt(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *target = findChild(node, "IDENTIFIER", 0);
    if (target == NULL) return;

    const char *stateName = nodeLexeme(target);
    Symbol *sym = lookupSymbol(sa->globalScope, stateName);
    if (sym == NULL || sym->kind != SYM_STATE) {
        semError(sa, target, "Transition target '%s' is not a declared state.", stateName);
        return;
    }
    sym->isUsed = true;

    if (sa->inFunction) {
        semError(sa, node, "'transition' statement cannot be used inside function '%s'.", sa->currentFnName);
    }
}

static void analyzeReturnStmt(SemanticAnalyzer *sa, const ParseNode *node) {
    if (!sa->inFunction) {
        semError(sa, node, "'return' statement is only allowed inside a function declaration.");
        return;
    }

    const ParseNode *exprNode = firstMeaningfulAfter(node, "RETURN", 0);
    if (exprNode == NULL) {
        if (sa->currentFnReturnType.base != TYPE_VOID) {
            char buf[64];
            formatSemType(sa->currentFnReturnType, buf, sizeof(buf));
            semError(sa, node,
                     "Function '%s' must return a value of type '%s'.", sa->currentFnName, buf);
        }
        return;
    }

    SemType retType = analyzeExpression(sa, exprNode);
    if (!isAssignable(sa->currentFnReturnType, retType)) {
        char eBuf[64], gBuf[64];
        formatSemType(sa->currentFnReturnType, eBuf, sizeof(eBuf));
        formatSemType(retType, gBuf, sizeof(gBuf));
        semError(sa, exprNode,
                 "Return type mismatch in function '%s': expected '%s', got '%s'.",
                 sa->currentFnName, eBuf, gBuf);
    }
}

static void analyzePrintStmt(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *exprNode = firstMeaningfulAfter(node, "PRINT", 0);
    SemType t = analyzeExpression(sa, exprNode);
    if (!isErrorType(t) && t.base == TYPE_VOID) {
        semError(sa, exprNode, "Cannot pass a void expression to print().");
    }
}

static void analyzeSleepStmt(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *exprNode = firstMeaningfulAfter(node, "SLEEP", 0);
    SemType t = analyzeExpression(sa, exprNode);
    if (!isErrorType(t) && !isInt(t)) {
        char buf[64];
        formatSemType(t, buf, sizeof(buf));
        semError(sa, exprNode, "sleep() expects an 'int' duration in milliseconds, got '%s'.", buf);
    }
}

static void analyzeStateDecl(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *nameNode = findChild(node, "IDENTIFIER", 0);
    const ParseNode *body     = findChild(node, "Block", 0);

    bool prevInState = sa->inState;
    char prevStateName[128];
    strncpy(prevStateName, sa->currentStateName, sizeof(prevStateName));

    sa->inState = true;
    strncpy(sa->currentStateName, nodeLexeme(nameNode), sizeof(sa->currentStateName) - 1);
    sa->currentStateName[sizeof(sa->currentStateName) - 1] = '\0';

    char scopeLabel[64];
    snprintf(scopeLabel, sizeof(scopeLabel), "state:%s", sa->currentStateName);
    analyzeBlock(sa, body, scopeLabel, true);

    sa->inState = prevInState;
    strncpy(sa->currentStateName, prevStateName, sizeof(sa->currentStateName));
}

static void analyzeReactDecl(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *watchNode = findChild(node, "IDENTIFIER", 0);
    const ParseNode *body      = findChild(node, "Block", 0);

    const char *watchName = nodeLexeme(watchNode);
    Symbol *sym = lookupSymbol(sa->currentScope, watchName);
    if (sym == NULL) {
        semError(sa, watchNode, "'react' watches undeclared variable '%s'.", watchName);
    } else if (sym->kind != SYM_VAR) {
        semError(sa, watchNode, "'react' can only watch a variable, not '%s'.", watchName);
    } else {
        sym->isUsed = true;
    }

    bool prevInReact = sa->inReact;
    sa->inReact = true;

    char scopeLabel[64];
    snprintf(scopeLabel, sizeof(scopeLabel), "react:%s", watchName);
    analyzeBlock(sa, body, scopeLabel, true);

    sa->inReact = prevInReact;
}

static void analyzeFnDecl(SemanticAnalyzer *sa, const ParseNode *node) {
    const ParseNode *nameNode  = findChild(node, "IDENTIFIER", 0);
    const ParseNode *paramList = findChild(node, "ParamList", 0);
    const ParseNode *retNode   = findChild(node, "Type", 0);
    const ParseNode *body      = findChild(node, "Block", 0);

    const char *fnName = nodeLexeme(nameNode);
    SemType retType = resolveTypeNode(sa, retNode);

    char scopeLabel[64];
    snprintf(scopeLabel, sizeof(scopeLabel), "fn:%s", fnName);
    pushScope(sa, scopeLabel);

    // Register parameters into the function's local scope
    if (paramList != NULL) {
        for (size_t i = 0; i < paramList->childCount; i++) {
            if (isLabel(paramList->children[i], "Type")) {
                const ParseNode *pTypeNode = paramList->children[i];
                const ParseNode *pNameNode = NULL;
                for (size_t j = i + 1; j < paramList->childCount; j++) {
                    if (isLabel(paramList->children[j], "IDENTIFIER")) {
                        pNameNode = paramList->children[j];
                        i = j;
                        break;
                    }
                }
                if (pNameNode != NULL) {
                    Symbol psym;
                    memset(&psym, 0, sizeof(psym));
                    strncpy(psym.name, nodeLexeme(pNameNode), sizeof(psym.name) - 1);
                    psym.kind = SYM_PARAM;
                    psym.type = resolveTypeNode(sa, pTypeNode);
                    psym.line = pNameNode->line;
                    psym.column = pNameNode->column;
                    defineSymbol(sa, sa->currentScope, &psym, pNameNode);
                }
            }
        }
    }

    bool prevInFn = sa->inFunction;
    SemType prevRet = sa->currentFnReturnType;
    char prevFnName[128];
    strncpy(prevFnName, sa->currentFnName, sizeof(prevFnName));

    sa->inFunction = true;
    sa->currentFnReturnType = retType;
    strncpy(sa->currentFnName, fnName, sizeof(sa->currentFnName) - 1);
    sa->currentFnName[sizeof(sa->currentFnName) - 1] = '\0';

    analyzeBlock(sa, body, scopeLabel, false);

    if (!isErrorType(retType) && retType.base != TYPE_VOID && !nodeAlwaysReturns(body)) {
        semError(sa, nameNode,
                 "Function '%s' does not return a value on all code paths.", fnName);
    }

    sa->inFunction = prevInFn;
    sa->currentFnReturnType = prevRet;
    strncpy(sa->currentFnName, prevFnName, sizeof(sa->currentFnName));

    popScope(sa);
}

static void analyzeBlock(SemanticAnalyzer *sa, const ParseNode *blockNode,
                         const char *scopeName, bool createScope) {
    if (blockNode == NULL) return;
    if (createScope) pushScope(sa, scopeName);

    for (size_t i = 0; i < blockNode->childCount; i++) {
        if (isLabel(blockNode->children[i], "Statement")) {
            analyzeStatement(sa, blockNode->children[i]);
        }
    }

    if (createScope) popScope(sa);
}

static void analyzeStatement(SemanticAnalyzer *sa, const ParseNode *node) {
    if (node == NULL) return;

    // Unwrap Statement -> SimpleStatement / BlockStatement
    if (isLabel(node, "Statement")) {
        for (size_t i = 0; i < node->childCount; i++) {
            if (!isPunctuationNode(node->children[i])) {
                analyzeStatement(sa, node->children[i]);
            }
        }
        return;
    }

    if (isLabel(node, "SimpleStatement") || isLabel(node, "BlockStatement")) {
        const ParseNode *inner = firstMeaningfulInRange(node, 0, node->childCount);
        if (inner != NULL) analyzeStatement(sa, inner);
        return;
    }

    if (isLabel(node, "VarDecl"))        { analyzeVarDecl(sa, node); return; }
    if (isLabel(node, "EnsureStmt"))     { analyzeEnsureStmt(sa, node); return; }
    if (isLabel(node, "Assignment"))     { analyzeAssignment(sa, node); return; }
    if (isLabel(node, "PrintStmt"))      { analyzePrintStmt(sa, node); return; }
    if (isLabel(node, "SleepStmt"))      { analyzeSleepStmt(sa, node); return; }
    if (isLabel(node, "TransitionStmt")) { analyzeTransitionStmt(sa, node); return; }
    if (isLabel(node, "ReturnStmt"))     { analyzeReturnStmt(sa, node); return; }
    if (isLabel(node, "IfStmt"))         { analyzeIfStmt(sa, node); return; }
    if (isLabel(node, "LoopStmt"))       { analyzeLoopStmt(sa, node); return; }
    if (isLabel(node, "ForStmt"))        { analyzeForStmt(sa, node); return; }
    if (isLabel(node, "MatchStmt"))      { analyzeMatchStmt(sa, node); return; }
    if (isLabel(node, "StateDecl"))      { analyzeStateDecl(sa, node); return; }
    if (isLabel(node, "ReactDecl"))      { analyzeReactDecl(sa, node); return; }
    if (isLabel(node, "FnDecl"))         { analyzeFnDecl(sa, node); return; }
    if (isLabel(node, "QUIT"))           { return; }

    // Expression statement (e.g., standalone FunctionCall)
    analyzeExpression(sa, node);
}

// ============================================================================
// PASS 1: HOIST TOP-LEVEL STATES & FUNCTION SIGNATURES
// ============================================================================
static void hoistTopLevelDecls(SemanticAnalyzer *sa, const ParseNode *root) {
    if (root == NULL) return;

    for (size_t i = 0; i < root->childCount; i++) {
        const ParseNode *stmt = root->children[i];
        if (!isLabel(stmt, "Statement")) continue;

        const ParseNode *blockStmt = findChild(stmt, "BlockStatement", 0);
        if (blockStmt == NULL) continue;

        const ParseNode *decl = firstMeaningfulInRange(blockStmt, 0, blockStmt->childCount);
        if (decl == NULL) continue;

        if (isLabel(decl, "StateDecl")) {
            const ParseNode *nameNode = findChild(decl, "IDENTIFIER", 0);
            if (nameNode == NULL) continue;

            Symbol sym;
            memset(&sym, 0, sizeof(sym));
            strncpy(sym.name, nodeLexeme(nameNode), sizeof(sym.name) - 1);
            sym.kind = SYM_STATE;
            sym.type = makePrimitive(TYPE_VOID);
            sym.line = nameNode->line;
            sym.column = nameNode->column;
            defineSymbol(sa, sa->globalScope, &sym, nameNode);
        } else if (isLabel(decl, "FnDecl")) {
            const ParseNode *nameNode  = findChild(decl, "IDENTIFIER", 0);
            const ParseNode *paramList = findChild(decl, "ParamList", 0);
            const ParseNode *retNode   = findChild(decl, "Type", 0);
            if (nameNode == NULL) continue;

            Symbol sym;
            memset(&sym, 0, sizeof(sym));
            strncpy(sym.name, nodeLexeme(nameNode), sizeof(sym.name) - 1);
            sym.kind = SYM_FN;
            sym.type = resolveTypeNode(sa, retNode);
            sym.line = nameNode->line;
            sym.column = nameNode->column;

            if (paramList != NULL) {
                for (size_t p = 0; p < paramList->childCount; p++) {
                    if (isLabel(paramList->children[p], "Type")) {
                        if (sym.paramCount < SEM_MAX_PARAMS) {
                            sym.paramTypes[sym.paramCount++] = resolveTypeNode(sa, paramList->children[p]);
                        }
                    }
                }
            }

            defineSymbol(sa, sa->globalScope, &sym, nameNode);
        }
    }
}

// ============================================================================
// REPORT PRINTING & CLEANUP
// ============================================================================
static const char *symbolKindName(SymbolKind kind) {
    switch (kind) {
        case SYM_VAR:   return "VARIABLE";
        case SYM_PARAM: return "PARAMETER";
        case SYM_FN:    return "FUNCTION";
        case SYM_STATE: return "STATE";
        default:        return "UNKNOWN";
    }
}

static void writeSymbolTableReport(SemanticAnalyzer *sa) {
    if (sa->output == NULL) return;

    fprintf(sa->output, "\n================================================================================\n");
    fprintf(sa->output, "  SEMANTIC SYMBOL TABLE\n");
    fprintf(sa->output, "================================================================================\n");
    fprintf(sa->output, "%-22s | %-22s | %-10s | %-20s | %-8s\n",
            "SCOPE", "NAME", "KIND", "TYPE", "LINE");
    fprintf(sa->output, "-----------------------|------------------------|------------|----------------------|---------\n");

    for (size_t s = 0; s < sa->scopeCount; s++) {
        Scope *sc = sa->allScopes[s];
        for (size_t i = 0; i < sc->count; i++) {
            Symbol *sym = &sc->symbols[i];
            if (sym->isBuiltin) continue;

            char typeBuf[128];
            if (sym->kind == SYM_FN) {
                char retBuf[64];
                formatSemType(sym->type, retBuf, sizeof(retBuf));
                snprintf(typeBuf, sizeof(typeBuf), "(%d args) -> %s", sym->paramCount, retBuf);
            } else if (sym->kind == SYM_STATE) {
                strcpy(typeBuf, "state");
            } else {
                formatSemType(sym->type, typeBuf, sizeof(typeBuf));
            }

            fprintf(sa->output, "%-22s | %-22s | %-10s | %-20s | %-8d\n",
                    sc->name, sym->name, symbolKindName(sym->kind), typeBuf, sym->line);
        }
    }

    fprintf(sa->output, "\n================================================================================\n");
    fprintf(sa->output, "  SEMANTIC ANALYSIS SUMMARY\n");
    fprintf(sa->output, "================================================================================\n");
    if (sa->errorCount == 0) {
        fprintf(sa->output, "  [OK] Semantic analysis passed — 0 error(s), %d warning(s).\n",
                sa->warningCount);
    } else {
        fprintf(sa->output, "  [ERR] Semantic analysis failed — %d error(s), %d warning(s).\n",
                sa->errorCount, sa->warningCount);
    }
    fprintf(sa->output, "================================================================================\n");
}

static void cleanupAnalyzer(SemanticAnalyzer *sa) {
    for (size_t i = 0; i < sa->scopeCount; i++) {
        free(sa->allScopes[i]->symbols);
        free(sa->allScopes[i]);
    }
    free(sa->allScopes);
    if (sa->output != NULL) {
        fclose(sa->output);
        sa->output = NULL;
    }
}

// ============================================================================
// PUBLIC ENTRY POINT
// ============================================================================
bool analyzeSemantics(const char *sourceFilePath,
                      const ParseNode *root,
                      const char *outputPath) {
    if (root == NULL) {
        fprintf(stderr, "Error: Invalid input provided to semantic analyzer.\n");
        return false;
    }

    SemanticAnalyzer sa;
    memset(&sa, 0, sizeof(sa));

    if (sourceFilePath != NULL) {
        strncpy(sa.sourceFilePath, sourceFilePath, sizeof(sa.sourceFilePath) - 1);
    }

    if (outputPath != NULL) {
        sa.output = fopen(outputPath, "w");
        if (sa.output == NULL) {
            fprintf(stderr, "Error: Could not create semantic report file: %s\n", outputPath);
            return false;
        }
        fprintf(sa.output, "================================================================================\n");
        fprintf(sa.output, "  FIDELIS SEMANTIC ANALYZER — REPORT\n");
        fprintf(sa.output, "  Source: %s\n", sa.sourceFilePath[0] ? sa.sourceFilePath : "(unknown)");
        fprintf(sa.output, "================================================================================\n");
    }

    sa.globalScope = pushScope(&sa, "global");
    registerBuiltins(&sa);

    hoistTopLevelDecls(&sa, root);

    for (size_t i = 0; i < root->childCount; i++) {
        if (isLabel(root->children[i], "Statement")) {
            analyzeStatement(&sa, root->children[i]);
        }
    }

    if (sa.output != NULL) {
        writeSymbolTableReport(&sa);
    }

    bool success = (sa.errorCount == 0);
    cleanupAnalyzer(&sa);
    return success;
}