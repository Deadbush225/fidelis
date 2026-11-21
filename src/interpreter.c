#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "interpreter.h"
#include "semantic.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
static void execStatement(Interpreter *ip, const ParseNode *node);
static void execBlock(Interpreter *ip, const ParseNode *blockNode, bool createScope);
static Value evalExpression(Interpreter *ip, const ParseNode *node);
static Value makeArray(size_t initialCap);
static void arrayPush(ArrayObject *arr, Value val);

// ============================================================================
// PORTABLE HELPERS (STRINGS & SLEEP)
// ============================================================================
static char *dupString(const char *s) {
    if (s == NULL) s = "";
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

static void sleepMilliseconds(long long ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000L);
    nanosleep(&ts, NULL);
#endif
}

// ============================================================================
// VALUE LIFECYCLE & CONSTRUCTORS
// ============================================================================
static Value makeNull(void) {
    Value v;
    v.kind = VAL_NULL;
    v.as.intVal = 0;
    return v;
}

static Value makeInt(long long n) {
    Value v;
    v.kind = VAL_INT;
    v.as.intVal = n;
    return v;
}

static Value makeFloat(double d) {
    Value v;
    v.kind = VAL_FLOAT;
    v.as.floatVal = d;
    return v;
}

static Value makeBool(bool b) {
    Value v;
    v.kind = VAL_BOOL;
    v.as.boolVal = b;
    return v;
}

static Value makeChar(char c) {
    Value v;
    v.kind = VAL_CHAR;
    v.as.charVal = c;
    return v;
}

static Value makeStringOwned(char *ownedStr) {
    Value v;
    v.kind = VAL_STRING;
    v.as.strVal = ownedStr ? ownedStr : dupString("");
    return v;
}

static Value makeStringCopy(const char *s) {
    return makeStringOwned(dupString(s));
}

static Value defaultValueForType(const ParseNode *typeNode) {
    if (typeNode == NULL) return makeNull();
    if (findChild(typeNode, "QUESTION", 0) != NULL) return makeNull();

    const ParseNode *innerType = findChild(typeNode, "Type", 0);
    if (innerType != NULL) {
        const ParseNode *sizeNode = findChild(typeNode, "INT_LITERAL", 0);
        long long sz = (sizeNode != NULL) ? strtoll(nodeLexeme(sizeNode), NULL, 10) : 0;
        Value arr = makeArray((size_t)(sz > 0 ? sz : 0));
        for (long long i = 0; i < sz; i++) {
            arrayPush(arr.as.arrVal, defaultValueForType(innerType));
        }
        return arr;
    }

    if (findChild(typeNode, "INT", 0) != NULL)    return makeInt(0);
    if (findChild(typeNode, "FLOAT", 0) != NULL)  return makeFloat(0.0);
    if (findChild(typeNode, "BOOL", 0) != NULL)   return makeBool(false);
    if (findChild(typeNode, "CHAR", 0) != NULL)   return makeChar('\0');
    if (findChild(typeNode, "STRING", 0) != NULL) return makeStringCopy("");
    return makeNull();
}

static Value makeArray(size_t initialCap) {
    ArrayObject *arr = (ArrayObject *)calloc(1, sizeof(ArrayObject));
    if (arr == NULL) return makeNull();
    arr->refCount = 1;
    if (initialCap > 0) {
        arr->items = (Value *)calloc(initialCap, sizeof(Value));
        if (arr->items != NULL) arr->capacity = initialCap;
    }
    Value v;
    v.kind = VAL_ARRAY;
    v.as.arrVal = arr;
    return v;
}

static void arrayPush(ArrayObject *arr, Value val) {
    if (arr == NULL) return;
    if (arr->count == arr->capacity) {
        size_t newCap = (arr->capacity == 0) ? 4 : arr->capacity * 2;
        Value *grown = (Value *)realloc(arr->items, newCap * sizeof(Value));
        if (grown == NULL) return;
        arr->items = grown;
        arr->capacity = newCap;
    }
    arr->items[arr->count++] = val;
}

static Value cloneValue(Value v) {
    if (v.kind == VAL_STRING) {
        return makeStringCopy(v.as.strVal);
    }
    if (v.kind == VAL_ARRAY && v.as.arrVal != NULL) {
        v.as.arrVal->refCount++;
    }
    return v;
}

static void freeValue(Value v) {
    if (v.kind == VAL_STRING) {
        free(v.as.strVal);
    } else if (v.kind == VAL_ARRAY && v.as.arrVal != NULL) {
        v.as.arrVal->refCount--;
        if (v.as.arrVal->refCount == 0) {
            for (size_t i = 0; i < v.as.arrVal->count; i++) {
                freeValue(v.as.arrVal->items[i]);
            }
            free(v.as.arrVal->items);
            free(v.as.arrVal);
        }
    }
}

static bool isNumericVal(Value v) {
    return v.kind == VAL_INT || v.kind == VAL_FLOAT;
}

static double asDouble(Value v) {
    return (v.kind == VAL_INT) ? (double)v.as.intVal : v.as.floatVal;
}

static char *valueToString(Value v) {
    char buf[128];
    switch (v.kind) {
        case VAL_INT:
            snprintf(buf, sizeof(buf), "%lld", v.as.intVal);
            return dupString(buf);
        case VAL_FLOAT:
            snprintf(buf, sizeof(buf), "%g", v.as.floatVal);
            return dupString(buf);
        case VAL_BOOL:
            return dupString(v.as.boolVal ? "true" : "false");
        case VAL_CHAR:
            buf[0] = v.as.charVal;
            buf[1] = '\0';
            return dupString(buf);
        case VAL_STRING:
            return dupString(v.as.strVal ? v.as.strVal : "");
        case VAL_NULL:
            return dupString("null");
        case VAL_ARRAY: {
            ArrayObject *arr = v.as.arrVal;
            if (arr == NULL || arr->count == 0) return dupString("[]");
            size_t cap = 64;
            size_t len = 0;
            char *out = (char *)malloc(cap);
            if (out == NULL) return dupString("[]");
            out[len++] = '[';
            for (size_t i = 0; i < arr->count; i++) {
                char *elem = valueToString(arr->items[i]);
                size_t elen = strlen(elem);
                size_t needed = len + elen + 4;
                if (needed > cap) {
                    while (cap < needed) cap *= 2;
                    char *grown = (char *)realloc(out, cap);
                    if (grown == NULL) {
                        free(elem);
                        free(out);
                        return dupString("[]");
                    }
                    out = grown;
                }
                if (i > 0) {
                    out[len++] = ',';
                    out[len++] = ' ';
                }
                memcpy(out + len, elem, elen);
                len += elen;
                free(elem);
            }
            out[len++] = ']';
            out[len] = '\0';
            return out;
        }
    }
    return dupString("");
}

static bool valuesEqual(Value a, Value b) {
    if (a.kind == VAL_NULL && b.kind == VAL_NULL) return true;
    if (a.kind == VAL_NULL || b.kind == VAL_NULL) return false;
    if (isNumericVal(a) && isNumericVal(b)) {
        if (a.kind == VAL_INT && b.kind == VAL_INT) {
            return a.as.intVal == b.as.intVal;
        }
        return asDouble(a) == asDouble(b);
    }
    if (a.kind != b.kind) return false;
    switch (a.kind) {
        case VAL_BOOL:   return a.as.boolVal == b.as.boolVal;
        case VAL_CHAR:   return a.as.charVal == b.as.charVal;
        case VAL_STRING: return strcmp(a.as.strVal, b.as.strVal) == 0;
        case VAL_ARRAY:  return a.as.arrVal == b.as.arrVal;
        default:         return false;
    }
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

static const ParseNode *firstMeaningfulAfter(const ParseNode *node, const char *markerLabel, size_t occurrence) {
    int idx = findChildIndex(node, markerLabel, occurrence);
    if (idx < 0) return NULL;
    return firstMeaningfulInRange(node, (size_t)idx + 1, node->childCount);
}

// ============================================================================
// LITERAL UNESCAPING (REVERSES symbol_table.txt ESCAPING + FIDELIS ESCAPES)
// ============================================================================
// helper.c escapes '\' -> '\\' when writing symbol_table.txt. Undo that layer
// first so Fidelis escape sequences (\n, \t, \", \', \\) decode cleanly.
static char *unescapeSymbolTableText(const char *raw) {
    if (raw == NULL) return dupString("");
    size_t len = strlen(raw);
    char *out = (char *)malloc(len + 1);
    if (out == NULL) return dupString("");

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (raw[i] == '\\' && i + 1 < len) {
            char next = raw[i + 1];
            if (next == '\\') { out[j++] = '\\'; i++; continue; }
            if (next == 'n')  { out[j++] = '\n'; i++; continue; }
            if (next == 't')  { out[j++] = '\t'; i++; continue; }
            if (next == 'r')  { out[j++] = '\r'; i++; continue; }
        }
        out[j++] = raw[i];
    }
    out[j] = '\0';
    return out;
}

static char *decodeStringLiteral(const char *rawLexeme) {
    char *raw = unescapeSymbolTableText(rawLexeme);
    size_t len = strlen(raw);

    size_t start = (len >= 2 && raw[0] == '"') ? 1 : 0;
    size_t end   = (len >= 2 && raw[len - 1] == '"') ? len - 1 : len;

    char *out = (char *)malloc((end - start) + 1);
    if (out == NULL) {
        free(raw);
        return dupString("");
    }

    size_t j = 0;
    for (size_t i = start; i < end; i++) {
        if (raw[i] == '\\' && i + 1 < end) {
            i++;
            switch (raw[i]) {
                case 'n':  out[j++] = '\n'; break;
                case 't':  out[j++] = '\t'; break;
                case 'r':  out[j++] = '\r'; break;
                case '\\': out[j++] = '\\'; break;
                case '"':  out[j++] = '"';  break;
                case '\'': out[j++] = '\''; break;
                case '0':  out[j++] = '\0'; break;
                default:   out[j++] = raw[i]; break;
            }
        } else {
            out[j++] = raw[i];
        }
    }
    out[j] = '\0';
    free(raw);
    return out;
}

static char decodeCharLiteral(const char *rawLexeme) {
    char *raw = unescapeSymbolTableText(rawLexeme);
    size_t len = strlen(raw);
    char result = '\0';

    size_t start = (len >= 2 && raw[0] == '\'') ? 1 : 0;
    size_t end   = (len >= 2 && raw[len - 1] == '\'') ? len - 1 : len;

    if (start < end) {
        if (raw[start] == '\\' && start + 1 < end) {
            switch (raw[start + 1]) {
                case 'n':  result = '\n'; break;
                case 't':  result = '\t'; break;
                case 'r':  result = '\r'; break;
                case '\\': result = '\\'; break;
                case '\'': result = '\''; break;
                case '"':  result = '"';  break;
                case '0':  result = '\0'; break;
                default:   result = raw[start + 1]; break;
            }
        } else {
            result = raw[start];
        }
    }

    free(raw);
    return result;
}

// ============================================================================
// RUNTIME ERROR REPORTING
// ============================================================================
static void runtimeError(Interpreter *ip, const ParseNode *node, const char *fmt, ...) {
    if (ip->hadRuntimeError) return;
    ip->hadRuntimeError = true;

    char msg[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    const char *file = ip->sourceFilePath[0] ? ip->sourceFilePath : "(source)";
    int line = (node != NULL) ? node->line : 0;
    int col  = (node != NULL) ? node->column : 0;
    const char *lex = (node != NULL) ? nodeLexeme(node) : "";

    fprintf(stderr, "\n|==============================================================|\n");
    fprintf(stderr, "|  \e[1;31mRUNTIME ERROR\e[0m                                               |\n");
    fprintf(stderr, "|==============================================================|\n");
    if (line > 0) {
        fprintf(stderr, "|  \e[1;33mLocation:\e[0m  %s:%d:%d (Line %d, Column %d)\n",
                file, line, col, line, col);
    } else {
        fprintf(stderr, "|  \e[1;33mLocation:\e[0m  %s\n", file);
    }
    fprintf(stderr, "|  \e[1;33mProblem:\e[0m   %s\n", msg);
    if (line > 0 && ip->sourceFilePath[0] != '\0') {
        fprintf(stderr, "|\n");
        printSourceSnippet(stderr, ip->sourceFilePath, line, col, lex, true);
    }
    fprintf(stderr, "|==============================================================|\n\n");
}

// ============================================================================
// LEXICAL ENVIRONMENTS (PUSH / POP WITH IMMEDIATE CLEANUP)
// ============================================================================
static Env *createEnv(Env *parent) {
    Env *env = (Env *)calloc(1, sizeof(Env));
    if (env == NULL) return NULL;
    env->parent = parent;
    return env;
}

static void destroyEnv(Env *env) {
    if (env == NULL) return;
    for (size_t i = 0; i < env->count; i++) {
        freeValue(env->bindings[i].value);
    }
    free(env->bindings);
    free(env);
}

static void envDefine(Env *env, const char *name, Value val) {
    if (env == NULL || name == NULL) {
        freeValue(val);
        return;
    }
    for (size_t i = 0; i < env->count; i++) {
        if (strcmp(env->bindings[i].name, name) == 0) {
            freeValue(env->bindings[i].value);
            env->bindings[i].value = val;
            return;
        }
    }
    if (env->count == env->capacity) {
        size_t newCap = (env->capacity == 0) ? 8 : env->capacity * 2;
        VarBinding *grown = (VarBinding *)realloc(env->bindings, newCap * sizeof(VarBinding));
        if (grown == NULL) {
            freeValue(val);
            return;
        }
        env->bindings = grown;
        env->capacity = newCap;
    }
    strncpy(env->bindings[env->count].name, name, sizeof(env->bindings[env->count].name) - 1);
    env->bindings[env->count].name[sizeof(env->bindings[env->count].name) - 1] = '\0';
    env->bindings[env->count].value = val;
    env->count++;
}

static VarBinding *envLookup(Env *env, const char *name, Env **outOwner) {
    for (Env *e = env; e != NULL; e = e->parent) {
        for (size_t i = 0; i < e->count; i++) {
            if (strcmp(e->bindings[i].name, name) == 0) {
                if (outOwner != NULL) *outOwner = e;
                return &e->bindings[i];
            }
        }
    }
    if (outOwner != NULL) *outOwner = NULL;
    return NULL;
}

// ============================================================================
// REGISTRIES: FUNCTIONS, STATES, AND REACTIVE WATCHERS
// ============================================================================
static void registerFunction(Interpreter *ip, const char *name, const ParseNode *fnNode) {
    if (ip->fnCount == ip->fnCapacity) {
        size_t newCap = (ip->fnCapacity == 0) ? 8 : ip->fnCapacity * 2;
        FnEntry *grown = (FnEntry *)realloc(ip->functions, newCap * sizeof(FnEntry));
        if (grown == NULL) return;
        ip->functions = grown;
        ip->fnCapacity = newCap;
    }
    strncpy(ip->functions[ip->fnCount].name, name, sizeof(ip->functions[ip->fnCount].name) - 1);
    ip->functions[ip->fnCount].name[sizeof(ip->functions[ip->fnCount].name) - 1] = '\0';
    ip->functions[ip->fnCount].fnNode = fnNode;
    ip->fnCount++;
}

static const ParseNode *lookupFunction(Interpreter *ip, const char *name) {
    for (size_t i = 0; i < ip->fnCount; i++) {
        if (strcmp(ip->functions[i].name, name) == 0) return ip->functions[i].fnNode;
    }
    return NULL;
}

static void registerState(Interpreter *ip, const char *name, const ParseNode *blockNode) {
    if (ip->stateCount == ip->stateCapacity) {
        size_t newCap = (ip->stateCapacity == 0) ? 8 : ip->stateCapacity * 2;
        StateEntry *grown = (StateEntry *)realloc(ip->states, newCap * sizeof(StateEntry));
        if (grown == NULL) return;
        ip->states = grown;
        ip->stateCapacity = newCap;
    }
    strncpy(ip->states[ip->stateCount].name, name, sizeof(ip->states[ip->stateCount].name) - 1);
    ip->states[ip->stateCount].name[sizeof(ip->states[ip->stateCount].name) - 1] = '\0';
    ip->states[ip->stateCount].blockNode = blockNode;
    ip->stateCount++;
}

static const ParseNode *lookupState(Interpreter *ip, const char *name) {
    for (size_t i = 0; i < ip->stateCount; i++) {
        if (strcmp(ip->states[i].name, name) == 0) return ip->states[i].blockNode;
    }
    return NULL;
}

static void registerReact(Interpreter *ip, const char *watchedVar, const ParseNode *blockNode) {
    if (ip->reactCount == ip->reactCapacity) {
        size_t newCap = (ip->reactCapacity == 0) ? 8 : ip->reactCapacity * 2;
        ReactEntry *grown = (ReactEntry *)realloc(ip->reacts, newCap * sizeof(ReactEntry));
        if (grown == NULL) return;
        ip->reacts = grown;
        ip->reactCapacity = newCap;
    }
    strncpy(ip->reacts[ip->reactCount].watchedVar, watchedVar,
            sizeof(ip->reacts[ip->reactCount].watchedVar) - 1);
    ip->reacts[ip->reactCount].watchedVar[sizeof(ip->reacts[ip->reactCount].watchedVar) - 1] = '\0';
    ip->reacts[ip->reactCount].blockNode = blockNode;
    ip->reactCount++;
}

static void triggerReactWatchers(Interpreter *ip, const char *varName, const ParseNode *triggerNode) {
    if (ip->hadRuntimeError || ip->flow != FLOW_NONE) return;

    bool hasWatcher = false;
    for (size_t i = 0; i < ip->reactCount; i++) {
        if (strcmp(ip->reacts[i].watchedVar, varName) == 0) {
            hasWatcher = true;
            break;
        }
    }
    if (!hasWatcher) return;

    if (ip->reactDepth >= INTERP_MAX_REACT_DEPTH) {
        runtimeError(ip, triggerNode,
                     "Maximum react watcher depth (%d) exceeded while updating '%s'.",
                     INTERP_MAX_REACT_DEPTH, varName);
        return;
    }

    ip->reactDepth++;
    Env *savedEnv = ip->currentEnv;
    ip->currentEnv = ip->globalEnv;

    for (size_t i = 0; i < ip->reactCount; i++) {
        if (strcmp(ip->reacts[i].watchedVar, varName) == 0) {
            execBlock(ip, ip->reacts[i].blockNode, true);
            if (ip->hadRuntimeError || ip->flow != FLOW_NONE) break;
        }
    }

    ip->currentEnv = savedEnv;
    ip->reactDepth--;
}

// ============================================================================
// BUILT-IN & USER FUNCTION CALLS
// ============================================================================
static Value builtinReadLine(void) {
    char buf[1024];
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        return makeStringCopy("");
    }
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }
    return makeStringCopy(buf);
}

static Value builtinReadInt(void) {
    char buf[1024];
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        return makeNull();
    }

    char *p = buf;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '\0') return makeNull();

    errno = 0;
    char *endPtr = NULL;
    long long val = strtoll(p, &endPtr, 10);
    if (errno == ERANGE || endPtr == p) return makeNull();

    while (isspace((unsigned char)*endPtr)) endPtr++;
    if (*endPtr != '\0') return makeNull();

    return makeInt(val);
}

static Value builtinReplace(const char *src, const char *target, const char *replacement) {
    if (src == NULL) src = "";
    if (target == NULL || target[0] == '\0') return makeStringCopy(src);
    if (replacement == NULL) replacement = "";

    size_t srcLen = strlen(src);
    size_t tarLen = strlen(target);
    size_t repLen = strlen(replacement);

    size_t count = 0;
    const char *scan = src;
    while ((scan = strstr(scan, target)) != NULL) {
        count++;
        scan += tarLen;
    }

    size_t outLen = srcLen + count * (repLen - tarLen);
    char *out = (char *)malloc(outLen + 1);
    if (out == NULL) return makeStringCopy(src);

    char *dst = out;
    const char *cur = src;
    const char *hit = NULL;
    while ((hit = strstr(cur, target)) != NULL) {
        size_t chunk = (size_t)(hit - cur);
        memcpy(dst, cur, chunk);
        dst += chunk;
        memcpy(dst, replacement, repLen);
        dst += repLen;
        cur = hit + tarLen;
    }
    strcpy(dst, cur);
    return makeStringOwned(out);
}

static Value invokeFunctionCall(Interpreter *ip, const ParseNode *callNode,
                                const Value *pipedArg) {
    if (ip->hadRuntimeError) return makeNull();

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

    // Evaluate all arguments left-to-right
    Value args[INTERP_MAX_ARGS];
    int argCount = 0;

    if (pipedArg != NULL) {
        args[argCount++] = cloneValue(*pipedArg);
    }

    const ParseNode *argList = findChild(callNode, "ArgList", 0);
    if (argList != NULL) {
        for (size_t i = 0; i < argList->childCount; i++) {
            const ParseNode *child = argList->children[i];
            if (isPunctuationNode(child)) continue;
            if (argCount < INTERP_MAX_ARGS) {
                args[argCount++] = evalExpression(ip, child);
            }
            if (ip->hadRuntimeError) {
                for (int k = 0; k < argCount; k++) freeValue(args[k]);
                return makeNull();
            }
        }
    }

    // Built-in dispatch
    if (strcmp(fnName, "readLine") == 0) {
        for (int k = 0; k < argCount; k++) freeValue(args[k]);
        return builtinReadLine();
    }
    if (strcmp(fnName, "readInt") == 0) {
        for (int k = 0; k < argCount; k++) freeValue(args[k]);
        return builtinReadInt();
    }
    if (strcmp(fnName, "length") == 0) {
        Value result = makeInt(0);
        if (argCount >= 1) {
            if (args[0].kind == VAL_ARRAY && args[0].as.arrVal != NULL) {
                result = makeInt((long long)args[0].as.arrVal->count);
            } else if (args[0].kind == VAL_STRING && args[0].as.strVal != NULL) {
                result = makeInt((long long)strlen(args[0].as.strVal));
            } else {
                runtimeError(ip, callNode, "length() cannot be called on null or non-collection value.");
            }
        }
        for (int k = 0; k < argCount; k++) freeValue(args[k]);
        return result;
    }
    if (strcmp(fnName, "replace") == 0) {
        Value result = makeStringCopy("");
        if (argCount >= 3 && args[0].kind == VAL_STRING &&
            args[1].kind == VAL_STRING && args[2].kind == VAL_STRING) {
            result = builtinReplace(args[0].as.strVal, args[1].as.strVal, args[2].as.strVal);
        } else {
            runtimeError(ip, callNode, "replace() requires 3 non-null string arguments.");
        }
        for (int k = 0; k < argCount; k++) freeValue(args[k]);
        return result;
    }

    // User-defined function lookup
    const ParseNode *fnNode = lookupFunction(ip, fnName);
    if (fnNode == NULL) {
        for (int k = 0; k < argCount; k++) freeValue(args[k]);
        runtimeError(ip, calleeNode, "Undefined function '%s'.", fnName);
        return makeNull();
    }

    if (ip->callDepth >= INTERP_MAX_CALL_DEPTH) {
        for (int k = 0; k < argCount; k++) freeValue(args[k]);
        runtimeError(ip, calleeNode, "Stack overflow: maximum recursion depth (%d) exceeded in '%s'.",
                     INTERP_MAX_CALL_DEPTH, fnName);
        return makeNull();
    }

    // Lexical function scope: parent is globalEnv (NOT caller's local frame)
    Env *fnEnv = createEnv(ip->globalEnv);
    const ParseNode *paramList = findChild(fnNode, "ParamList", 0);
    int paramIdx = 0;
    if (paramList != NULL) {
        for (size_t i = 0; i < paramList->childCount; i++) {
            if (isLabel(paramList->children[i], "IDENTIFIER")) {
                const char *pName = nodeLexeme(paramList->children[i]);
                Value pVal = (paramIdx < argCount) ? cloneValue(args[paramIdx]) : makeNull();
                envDefine(fnEnv, pName, pVal);
                paramIdx++;
            }
        }
    }
    for (int k = 0; k < argCount; k++) freeValue(args[k]);

    Env *savedEnv = ip->currentEnv;
    ip->currentEnv = fnEnv;
    ip->callDepth++;

    const ParseNode *body = findChild(fnNode, "Block", 0);
    execBlock(ip, body, false);

    Value result = makeNull();
    if (ip->flow == FLOW_RETURN) {
        result = ip->returnValue;
        ip->returnValue = makeNull();
        ip->flow = FLOW_NONE;
    }

    ip->callDepth--;
    ip->currentEnv = savedEnv;
    destroyEnv(fnEnv);
    return result;
}

// ============================================================================
// EXPRESSION EVALUATION
// ============================================================================
static long long intPow(long long base, long long exp) {
    long long res = 1;
    while (exp > 0) {
        if (exp & 1) res *= base;
        base *= base;
        exp >>= 1;
    }
    return res;
}

static Value evalBinaryOp(Interpreter *ip, const ParseNode *opNode, Value lhs, Value rhs) {
    const char *op = opNode->label;

    if (strcmp(op, "EQUALITY") == 0)   return makeBool(valuesEqual(lhs, rhs));
    if (strcmp(op, "INEQUALITY") == 0) return makeBool(!valuesEqual(lhs, rhs));

    if (strcmp(op, "LESS_THAN") == 0 || strcmp(op, "GREATER_THAN") == 0 ||
        strcmp(op, "LESS_EQUAL") == 0 || strcmp(op, "GREATER_EQUAL") == 0) {
        double l = (lhs.kind == VAL_CHAR) ? (double)lhs.as.charVal : asDouble(lhs);
        double r = (rhs.kind == VAL_CHAR) ? (double)rhs.as.charVal : asDouble(rhs);
        if (strcmp(op, "LESS_THAN") == 0)     return makeBool(l < r);
        if (strcmp(op, "GREATER_THAN") == 0)  return makeBool(l > r);
        if (strcmp(op, "LESS_EQUAL") == 0)    return makeBool(l <= r);
        if (strcmp(op, "GREATER_EQUAL") == 0) return makeBool(l >= r);
    }

    if (strcmp(op, "ADDITION") == 0) {
        if (lhs.kind == VAL_STRING || rhs.kind == VAL_STRING) {
            char *ls = valueToString(lhs);
            char *rs = valueToString(rhs);
            size_t llen = strlen(ls);
            size_t rlen = strlen(rs);
            char *joined = (char *)malloc(llen + rlen + 1);
            if (joined == NULL) {
                free(ls);
                free(rs);
                return makeStringCopy("");
            }
            memcpy(joined, ls, llen);
            memcpy(joined + llen, rs, rlen + 1);
            free(ls);
            free(rs);
            return makeStringOwned(joined);
        }
        if (lhs.kind == VAL_INT && rhs.kind == VAL_INT) {
            return makeInt(lhs.as.intVal + rhs.as.intVal);
        }
        return makeFloat(asDouble(lhs) + asDouble(rhs));
    }

    if (strcmp(op, "SUBTRACTION") == 0) {
        if (lhs.kind == VAL_INT && rhs.kind == VAL_INT) {
            return makeInt(lhs.as.intVal - rhs.as.intVal);
        }
        return makeFloat(asDouble(lhs) - asDouble(rhs));
    }

    if (strcmp(op, "MULTIPLICATION") == 0) {
        if (lhs.kind == VAL_INT && rhs.kind == VAL_INT) {
            return makeInt(lhs.as.intVal * rhs.as.intVal);
        }
        return makeFloat(asDouble(lhs) * asDouble(rhs));
    }

    if (strcmp(op, "DIVISION") == 0) {
        if ((rhs.kind == VAL_INT && rhs.as.intVal == 0) ||
            (rhs.kind == VAL_FLOAT && rhs.as.floatVal == 0.0)) {
            runtimeError(ip, opNode, "Division by zero.");
            return makeNull();
        }
        if (lhs.kind == VAL_INT && rhs.kind == VAL_INT) {
            return makeInt(lhs.as.intVal / rhs.as.intVal);
        }
        return makeFloat(asDouble(lhs) / asDouble(rhs));
    }

    if (strcmp(op, "INTEGER_DIVISION") == 0) {
        if (rhs.as.intVal == 0) {
            runtimeError(ip, opNode, "Integer division by zero.");
            return makeNull();
        }
        return makeInt(lhs.as.intVal / rhs.as.intVal);
    }

    if (strcmp(op, "MODULO") == 0) {
        if (rhs.as.intVal == 0) {
            runtimeError(ip, opNode, "Modulo by zero.");
            return makeNull();
        }
        return makeInt(lhs.as.intVal % rhs.as.intVal);
    }

    if (strcmp(op, "EXPONENT") == 0) {
        if (lhs.kind == VAL_INT && rhs.kind == VAL_INT && rhs.as.intVal >= 0) {
            return makeInt(intPow(lhs.as.intVal, rhs.as.intVal));
        }
        return makeFloat(pow(asDouble(lhs), asDouble(rhs)));
    }

    runtimeError(ip, opNode, "Unsupported operator '%s'.", op);
    return makeNull();
}

static Value evalBinaryChain(Interpreter *ip, const ParseNode *node) {
    const ParseNode *items[128];
    size_t count = 0;
    for (size_t i = 0; i < node->childCount && count < 128; i++) {
        if (!isPunctuationNode(node->children[i])) {
            items[count++] = node->children[i];
        }
    }
    if (count == 0) return makeNull();

    Value acc = evalExpression(ip, items[0]);
    for (size_t i = 1; i + 1 < count && !ip->hadRuntimeError; i += 2) {
        const ParseNode *opNode = items[i];
        const ParseNode *rhsNode = items[i + 1];

        // Short-circuit logical operators
        if (isLabel(opNode, "LOGICAL_AND")) {
            if (!acc.as.boolVal) continue;
            freeValue(acc);
            acc = evalExpression(ip, rhsNode);
            continue;
        }
        if (isLabel(opNode, "LOGICAL_OR")) {
            if (acc.as.boolVal) continue;
            freeValue(acc);
            acc = evalExpression(ip, rhsNode);
            continue;
        }

        Value rhs = evalExpression(ip, rhsNode);
        if (ip->hadRuntimeError) {
            freeValue(acc);
            freeValue(rhs);
            return makeNull();
        }
        Value res = evalBinaryOp(ip, opNode, acc, rhs);
        freeValue(acc);
        freeValue(rhs);
        acc = res;
    }

    return acc;
}

static Value evalExpression(Interpreter *ip, const ParseNode *node) {
    if (node == NULL || ip->hadRuntimeError) return makeNull();

    // ---- Literals ----
    if (isLabel(node, "INT_LITERAL"))    return makeInt(strtoll(nodeLexeme(node), NULL, 10));
    if (isLabel(node, "FLOAT_LITERAL"))  return makeFloat(strtod(nodeLexeme(node), NULL));
    if (isLabel(node, "STRING_LITERAL")) return makeStringOwned(decodeStringLiteral(nodeLexeme(node)));
    if (isLabel(node, "CHAR_LITERAL"))   return makeChar(decodeCharLiteral(nodeLexeme(node)));
    if (isLabel(node, "TRUE"))           return makeBool(true);
    if (isLabel(node, "FALSE"))          return makeBool(false);
    if (isLabel(node, "NULL"))           return makeNull();

    // ---- Identifier ----
    if (isLabel(node, "IDENTIFIER")) {
        const char *name = nodeLexeme(node);
        VarBinding *b = envLookup(ip->currentEnv, name, NULL);
        if (b == NULL) {
            runtimeError(ip, node, "Undefined variable '%s'.", name);
            return makeNull();
        }
        return cloneValue(b->value);
    }

    // ---- Binary & exponent chains ----
    if (isLabel(node, "LogicalOrExpr") || isLabel(node, "LogicalAndExpr") ||
        isLabel(node, "EqualityExpr")  || isLabel(node, "RelationalExpr") ||
        isLabel(node, "AdditiveExpr")  || isLabel(node, "MultiplicativeExpr") ||
        isLabel(node, "ExponentExpr")) {
        return evalBinaryChain(ip, node);
    }

    // ---- UnaryExpr ----
    if (isLabel(node, "UnaryExpr")) {
        const ParseNode *opNode = (node->childCount > 0) ? node->children[0] : NULL;
        const ParseNode *operand = firstMeaningfulInRange(node, 1, node->childCount);
        Value val = evalExpression(ip, operand);
        if (ip->hadRuntimeError) {
            freeValue(val);
            return makeNull();
        }
        if (isLabel(opNode, "LOGICAL_NOT")) {
            bool res = !val.as.boolVal;
            freeValue(val);
            return makeBool(res);
        }
        if (isLabel(opNode, "SUBTRACTION")) {
            if (val.kind == VAL_INT) {
                long long res = -val.as.intVal;
                freeValue(val);
                return makeInt(res);
            }
            double res = -asDouble(val);
            freeValue(val);
            return makeFloat(res);
        }
        freeValue(val);
        return makeNull();
    }

    // ---- TernaryExpr ----
    if (isLabel(node, "TernaryExpr")) {
        int qIdx = findChildIndex(node, "QUESTION", 0);
        int cIdx = findChildIndex(node, "COLON", 0);
        if (qIdx < 0 || cIdx < 0) return makeNull();

        const ParseNode *condNode  = firstMeaningfulInRange(node, 0, (size_t)qIdx);
        const ParseNode *trueNode  = firstMeaningfulInRange(node, (size_t)qIdx + 1, (size_t)cIdx);
        const ParseNode *falseNode = firstMeaningfulInRange(node, (size_t)cIdx + 1, node->childCount);

        Value cond = evalExpression(ip, condNode);
        if (ip->hadRuntimeError) {
            freeValue(cond);
            return makeNull();
        }
        bool takeTrue = (cond.kind == VAL_BOOL && cond.as.boolVal);
        freeValue(cond);
        return evalExpression(ip, takeTrue ? trueNode : falseNode);
    }

    // ---- PipelineExpr ----
    if (isLabel(node, "PipelineExpr")) {
        int firstPipe = findChildIndex(node, "PIPELINE", 0);
        if (firstPipe < 0) return makeNull();

        const ParseNode *lhsNode = firstMeaningfulInRange(node, 0, (size_t)firstPipe);
        Value current = evalExpression(ip, lhsNode);

        for (size_t i = (size_t)firstPipe; i < node->childCount && !ip->hadRuntimeError; i++) {
            if (isLabel(node->children[i], "PIPELINE")) {
                const ParseNode *stage = firstMeaningfulInRange(node, i + 1, node->childCount);
                if (stage != NULL && isLabel(stage, "FunctionCall")) {
                    Value next = invokeFunctionCall(ip, stage, &current);
                    freeValue(current);
                    current = next;
                }
            }
        }
        return current;
    }

    // ---- FunctionCall ----
    if (isLabel(node, "FunctionCall")) {
        return invokeFunctionCall(ip, node, NULL);
    }

    // ---- ArrayLiteral ----
    if (isLabel(node, "ArrayLiteral")) {
        const ParseNode *argList = findChild(node, "ArgList", 0);
        Value arr = makeArray(argList ? argList->childCount : 0);
        if (argList != NULL) {
            for (size_t i = 0; i < argList->childCount; i++) {
                if (isPunctuationNode(argList->children[i])) continue;
                Value elem = evalExpression(ip, argList->children[i]);
                if (ip->hadRuntimeError) {
                    freeValue(elem);
                    freeValue(arr);
                    return makeNull();
                }
                arrayPush(arr.as.arrVal, elem);
            }
        }
        return arr;
    }

    // ---- ArrayAccess ----
    if (isLabel(node, "ArrayAccess")) {
        const ParseNode *arrIdent = findChild(node, "IDENTIFIER", 0);
        const ParseNode *idxExpr  = firstMeaningfulAfter(node, "LBRACKET", 0);

        Value arrVal = evalExpression(ip, arrIdent);
        Value idxVal = evalExpression(ip, idxExpr);
        if (ip->hadRuntimeError) {
            freeValue(arrVal);
            freeValue(idxVal);
            return makeNull();
        }

        if (arrVal.kind != VAL_ARRAY || arrVal.as.arrVal == NULL) {
            runtimeError(ip, arrIdent, "Cannot index null or non-array value.");
            freeValue(arrVal);
            freeValue(idxVal);
            return makeNull();
        }

        long long idx = idxVal.as.intVal;
        ArrayObject *arr = arrVal.as.arrVal;
        if (idx < 0 || (size_t)idx >= arr->count) {
            runtimeError(ip, idxExpr, "Array index %lld out of bounds (length %zu).",
                         idx, arr->count);
            freeValue(arrVal);
            freeValue(idxVal);
            return makeNull();
        }

        Value elem = cloneValue(arr->items[idx]);
        freeValue(arrVal);
        freeValue(idxVal);
        return elem;
    }

    // ---- PropertyAccess ----
    if (isLabel(node, "PropertyAccess")) {
        const ParseNode *objNode  = findChild(node, "IDENTIFIER", 0);
        const ParseNode *propNode = findChild(node, "IDENTIFIER", 1);

        Value objVal = evalExpression(ip, objNode);
        if (ip->hadRuntimeError) {
            freeValue(objVal);
            return makeNull();
        }

        const char *prop = nodeLexeme(propNode);
        Value res = makeNull();
        if (strcmp(prop, "length") == 0) {
            if (objVal.kind == VAL_ARRAY && objVal.as.arrVal != NULL) {
                res = makeInt((long long)objVal.as.arrVal->count);
            } else if (objVal.kind == VAL_STRING && objVal.as.strVal != NULL) {
                res = makeInt((long long)strlen(objVal.as.strVal));
            } else {
                runtimeError(ip, propNode, "Cannot access '.length' on null value.");
            }
        } else {
            runtimeError(ip, propNode, "Unknown property '.%s'.", prop);
        }

        freeValue(objVal);
        return res;
    }

    return makeNull();
}

// ============================================================================
// STATEMENT EXECUTION
// ============================================================================
static void execVarDecl(Interpreter *ip, const ParseNode *node) {
    const ParseNode *typeNode = findChild(node, "Type", 0);
    const ParseNode *nameNode = findChild(node, "IDENTIFIER", 0);
    const ParseNode *initExpr = firstMeaningfulAfter(node, "ASSIGNMENT", 0);

    const ParseNode *innerType = findChild(typeNode, "Type", 0);
    const ParseNode *sizeNode  = findChild(typeNode, "INT_LITERAL", 0);
    long long fixedSize = (sizeNode != NULL) ? strtoll(nodeLexeme(sizeNode), NULL, 10) : -1;

    // Case 1: Empty fixed-size array -> [int, 10] scores;
    if (initExpr == NULL) {
        Value val = defaultValueForType(typeNode);
        envDefine(ip->currentEnv, nodeLexeme(nameNode), val);
        return;
    }

    // Case 2: Initialized variable or array
    Value val = evalExpression(ip, initExpr);
    if (ip->hadRuntimeError) {
        freeValue(val);
        return;
    }

    // Pad partial fixed-size initializers (e.g., [int, 10] scores = [90, 85, 88];)
    if (fixedSize > 0 && val.kind == VAL_ARRAY && val.as.arrVal != NULL) {
        if (val.as.arrVal->count > (size_t)fixedSize) {
            runtimeError(ip, initExpr,
                         "Array initializer length (%zu) exceeds fixed size (%lld) of '%s'.",
                         val.as.arrVal->count, fixedSize, nodeLexeme(nameNode));
            freeValue(val);
            return;
        }
        while (val.as.arrVal->count < (size_t)fixedSize) {
            arrayPush(val.as.arrVal, defaultValueForType(innerType));
        }
    }

    envDefine(ip->currentEnv, nodeLexeme(nameNode), val);
}

static void execEnsureStmt(Interpreter *ip, const ParseNode *node) {
    const ParseNode *nameNode  = findChild(node, "IDENTIFIER", 0);
    const ParseNode *exprNode  = firstMeaningfulAfter(node, "ENSURE", 0);
    const ParseNode *elseBlock = findChild(node, "Block", 0);

    Value val = evalExpression(ip, exprNode);
    if (ip->hadRuntimeError) {
        freeValue(val);
        return;
    }

    // Value is non-null: define if 'Type' was provided, otherwise update existing variable
    const ParseNode *typeNode = findChild(node, "Type", 0);
    const char *name = nodeLexeme(nameNode);

    if (typeNode != NULL) {
        envDefine(ip->currentEnv, name, val);
    } else {
        Env *ownerEnv = NULL;
        VarBinding *binding = envLookup(ip->currentEnv, name, &ownerEnv);
        if (binding == NULL) {
            runtimeError(ip, nameNode, "Assignment to undefined variable '%s'.", name);
            freeValue(val);
            return;
        }

        freeValue(binding->value);
        binding->value = val;

        if (ownerEnv == ip->globalEnv) {
            triggerReactWatchers(ip, name, node);
        }
    }

    if (val.kind == VAL_NULL) {
        freeValue(val);
        execBlock(ip, elseBlock, true);
        return;
    }

    envDefine(ip->currentEnv, nodeLexeme(nameNode), val);
}

static Value applyCompoundOp(Interpreter *ip, const ParseNode *opNode, Value cur, Value rhs) {
    const char *op = opNode->label;
    ParseNode fakeOp = *opNode;

    if (strcmp(op, "ADD_ASSIGN") == 0)      strcpy(fakeOp.label, "ADDITION");
    else if (strcmp(op, "SUBTRACT_ASSIGN") == 0) strcpy(fakeOp.label, "SUBTRACTION");
    else if (strcmp(op, "MULTIPLY_ASSIGN") == 0) strcpy(fakeOp.label, "MULTIPLICATION");
    else if (strcmp(op, "DIVIDE_ASSIGN") == 0)   strcpy(fakeOp.label, "DIVISION");
    else if (strcmp(op, "MODULO_ASSIGN") == 0)   strcpy(fakeOp.label, "MODULO");

    return evalBinaryOp(ip, &fakeOp, cur, rhs);
}

static void execAssignment(Interpreter *ip, const ParseNode *node) {
    const ParseNode *targetNode = findChild(node, "IDENTIFIER", 0);
    if (targetNode == NULL) return;

    const char *name = nodeLexeme(targetNode);
    Env *ownerEnv = NULL;
    VarBinding *binding = envLookup(ip->currentEnv, name, &ownerEnv);
    if (binding == NULL) {
        runtimeError(ip, targetNode, "Assignment to undefined variable '%s'.", name);
        return;
    }

    int lbIdx = findChildIndex(node, "LBRACKET", 0);
    int rbIdx = findChildIndex(node, "RBRACKET", 0);
    size_t opSearchStart = (lbIdx >= 0 && rbIdx > lbIdx) ? (size_t)rbIdx + 1 : 1;

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

    // Indexed array assignment: arr[index] op rhs
    if (lbIdx >= 0 && rbIdx > lbIdx) {
        const ParseNode *idxExpr = firstMeaningfulInRange(node, (size_t)lbIdx + 1, (size_t)rbIdx);
        Value idxVal = evalExpression(ip, idxExpr);
        Value rhsVal = evalExpression(ip, rhsExpr);
        if (ip->hadRuntimeError) {
            freeValue(idxVal);
            freeValue(rhsVal);
            return;
        }

        if (binding->value.kind != VAL_ARRAY || binding->value.as.arrVal == NULL) {
            runtimeError(ip, targetNode, "Cannot index null or non-array variable '%s'.", name);
            freeValue(idxVal);
            freeValue(rhsVal);
            return;
        }

        ArrayObject *arr = binding->value.as.arrVal;
        long long idx = idxVal.as.intVal;
        freeValue(idxVal);

        if (idx < 0 || (size_t)idx >= arr->count) {
            runtimeError(ip, idxExpr, "Array index %lld out of bounds (length %zu).",
                         idx, arr->count);
            freeValue(rhsVal);
            return;
        }

        Value newVal = isLabel(opNode, "ASSIGNMENT")
                           ? rhsVal
                           : applyCompoundOp(ip, opNode, arr->items[idx], rhsVal);
        if (!isLabel(opNode, "ASSIGNMENT")) freeValue(rhsVal);

        if (ip->hadRuntimeError) {
            freeValue(newVal);
            return;
        }

        freeValue(arr->items[idx]);
        arr->items[idx] = newVal;

        if (ownerEnv == ip->globalEnv) {
            triggerReactWatchers(ip, name, node);
        }
        return;
    }

    // Scalar / whole-variable assignment
    Value rhsVal = evalExpression(ip, rhsExpr);
    if (ip->hadRuntimeError) {
        freeValue(rhsVal);
        return;
    }

    Value newVal = isLabel(opNode, "ASSIGNMENT")
                       ? rhsVal
                       : applyCompoundOp(ip, opNode, binding->value, rhsVal);
    if (!isLabel(opNode, "ASSIGNMENT")) freeValue(rhsVal);

    if (ip->hadRuntimeError) {
        freeValue(newVal);
        return;
    }

    freeValue(binding->value);
    binding->value = newVal;

    if (ownerEnv == ip->globalEnv) {
        triggerReactWatchers(ip, name, node);
    }
}

static void execPrintStmt(Interpreter *ip, const ParseNode *node) {
    const ParseNode *expr = firstMeaningfulAfter(node, "PRINT", 0);
    Value val = evalExpression(ip, expr);
    if (ip->hadRuntimeError) {
        freeValue(val);
        return;
    }

    const ParseNode *endExpr = firstMeaningfulAfter(node, "COMMA", 0);
    if (endExpr == NULL) {
        char *text = valueToString(val);
        printf("%s\n", text);
        fflush(stdout);
        free(text);
        freeValue(val);
        return;
    }

    Value endVal = evalExpression(ip, endExpr);
    if (ip->hadRuntimeError) {
        freeValue(val);
        freeValue(endVal);
        return;
    }

    char *text = valueToString(val);
    char *endText = valueToString(endVal);
    printf("%s%s", text, endText);
    fflush(stdout);

    free(text);
    free(endText);
    freeValue(val);
    freeValue(endVal);
}

static void execSleepStmt(Interpreter *ip, const ParseNode *node) {
    const ParseNode *expr = firstMeaningfulAfter(node, "SLEEP", 0);
    Value val = evalExpression(ip, expr);
    if (ip->hadRuntimeError) {
        freeValue(val);
        return;
    }
    if (val.kind == VAL_INT) {
        sleepMilliseconds(val.as.intVal);
    }
    freeValue(val);
}

static void execTransitionStmt(Interpreter *ip, const ParseNode *node) {
    const ParseNode *target = findChild(node, "IDENTIFIER", 0);
    if (target == NULL) return;

    const char *stateName = nodeLexeme(target);
    if (lookupState(ip, stateName) == NULL) {
        runtimeError(ip, target, "Cannot transition to unknown state '%s'.", stateName);
        return;
    }
    strncpy(ip->nextState, stateName, sizeof(ip->nextState) - 1);
    ip->nextState[sizeof(ip->nextState) - 1] = '\0';
    ip->flow = FLOW_TRANSITION;
}

static void execReturnStmt(Interpreter *ip, const ParseNode *node) {
    const ParseNode *expr = firstMeaningfulAfter(node, "RETURN", 0);
    Value val = (expr != NULL) ? evalExpression(ip, expr) : makeNull();
    if (ip->hadRuntimeError) {
        freeValue(val);
        return;
    }
    freeValue(ip->returnValue);
    ip->returnValue = val;
    ip->flow = FLOW_RETURN;
}

static void execIfStmt(Interpreter *ip, const ParseNode *node) {
    const ParseNode *condExpr = firstMeaningfulAfter(node, "IF", 0);
    Value cond = evalExpression(ip, condExpr);
    if (ip->hadRuntimeError) {
        freeValue(cond);
        return;
    }
    bool truthy = (cond.kind == VAL_BOOL && cond.as.boolVal);
    freeValue(cond);

    int elseIdx = findChildIndex(node, "ELSE", 0);
    size_t thenLimit = (elseIdx >= 0) ? (size_t)elseIdx : node->childCount;

    if (truthy) {
        for (size_t i = 0; i < thenLimit; i++) {
            if (isLabel(node->children[i], "Block")) {
                execBlock(ip, node->children[i], true);
                return;
            }
            if (isLabel(node->children[i], "Statement")) {
                Env *scope = createEnv(ip->currentEnv);
                Env *prev = ip->currentEnv;
                ip->currentEnv = scope;
                execStatement(ip, node->children[i]);
                ip->currentEnv = prev;
                destroyEnv(scope);
                return;
            }
        }
    } else if (elseIdx >= 0) {
        const ParseNode *elseNode = firstMeaningfulInRange(node, (size_t)elseIdx + 1, node->childCount);
        if (isLabel(elseNode, "IfStmt")) {
            execIfStmt(ip, elseNode);
        } else if (isLabel(elseNode, "Block")) {
            execBlock(ip, elseNode, true);
        } else if (isLabel(elseNode, "Statement")) {
            Env *scope = createEnv(ip->currentEnv);
            Env *prev = ip->currentEnv;
            ip->currentEnv = scope;
            execStatement(ip, elseNode);
            ip->currentEnv = prev;
            destroyEnv(scope);
        }
    }
}

static void execLoopStmt(Interpreter *ip, const ParseNode *node) {
    const ParseNode *body = findChild(node, "Block", 0);
    while (!ip->hadRuntimeError && ip->flow == FLOW_NONE) {
        execBlock(ip, body, true);
    }
}

static void execForStmt(Interpreter *ip, const ParseNode *node) {
    Env *forEnv = createEnv(ip->currentEnv);
    Env *prevEnv = ip->currentEnv;
    ip->currentEnv = forEnv;

    const ParseNode *varNode    = findChild(node, "IDENTIFIER", 0);
    const ParseNode *initExpr   = firstMeaningfulAfter(node, "ASSIGNMENT", 0);
    const ParseNode *condExpr   = firstMeaningfulAfter(node, "SEMICOLON", 0);
    const ParseNode *updateNode = findChild(node, "Assignment", 0);
    const ParseNode *bodyBlock  = findChild(node, "Block", 0);

    Value initVal = evalExpression(ip, initExpr);
    if (!ip->hadRuntimeError) {
        envDefine(forEnv, nodeLexeme(varNode), initVal);
    } else {
        freeValue(initVal);
    }

    while (!ip->hadRuntimeError && ip->flow == FLOW_NONE) {
        Value cond = evalExpression(ip, condExpr);
        bool keepGoing = (!ip->hadRuntimeError && cond.kind == VAL_BOOL && cond.as.boolVal);
        freeValue(cond);
        if (!keepGoing) break;

        // Each iteration gets its own inner block frame under forEnv
        execBlock(ip, bodyBlock, true);
        if (ip->hadRuntimeError || ip->flow != FLOW_NONE) break;

        if (updateNode != NULL) {
            execAssignment(ip, updateNode);
        }
    }

    ip->currentEnv = prevEnv;
    destroyEnv(forEnv);
}

static void execMatchStmt(Interpreter *ip, const ParseNode *node) {
    const ParseNode *subjectNode = firstMeaningfulAfter(node, "MATCH", 0);
    Value subject = evalExpression(ip, subjectNode);
    if (ip->hadRuntimeError) {
        freeValue(subject);
        return;
    }

    for (size_t i = 0; i < node->childCount; i++) {
        const ParseNode *mc = node->children[i];
        if (!isLabel(mc, "MatchCase")) continue;

        int arrowIdx = findChildIndex(mc, "ARROW", 0);
        if (arrowIdx < 0) continue;

        const ParseNode *firstPat = firstMeaningfulInRange(mc, 0, (size_t)arrowIdx);
        bool matched = false;

        if (isLabel(firstPat, "IDENTIFIER") && strcmp(nodeLexeme(firstPat), "_") == 0) {
            matched = true;
        } else if (firstPat != NULL) {
            Value patVal = makeNull();
            if (isLabel(firstPat, "SUBTRACTION") || isLabel(firstPat, "ADDITION")) {
                const ParseNode *numNode = firstMeaningfulInRange(mc, 1, (size_t)arrowIdx);
                patVal = evalExpression(ip, numNode);
                if (isLabel(firstPat, "SUBTRACTION")) {
                    if (patVal.kind == VAL_INT) patVal.as.intVal = -patVal.as.intVal;
                    else if (patVal.kind == VAL_FLOAT) patVal.as.floatVal = -patVal.as.floatVal;
                }
            } else {
                patVal = evalExpression(ip, firstPat);
            }

            if (ip->hadRuntimeError) {
                freeValue(patVal);
                freeValue(subject);
                return;
            }
            matched = valuesEqual(subject, patVal);
            freeValue(patVal);
        }

        if (matched) {
            const ParseNode *body = firstMeaningfulInRange(mc, (size_t)arrowIdx + 1, mc->childCount);
            if (isLabel(body, "Block")) {
                execBlock(ip, body, true);
            } else if (body != NULL) {
                Env *caseEnv = createEnv(ip->currentEnv);
                Env *prev = ip->currentEnv;
                ip->currentEnv = caseEnv;
                execStatement(ip, body);
                ip->currentEnv = prev;
                destroyEnv(caseEnv);
            }
            break;
        }
    }

    freeValue(subject);
}

static void execBlock(Interpreter *ip, const ParseNode *blockNode, bool createScope) {
    if (blockNode == NULL || ip->hadRuntimeError || ip->flow != FLOW_NONE) return;

    Env *prevEnv = ip->currentEnv;
    Env *localEnv = NULL;
    if (createScope) {
        localEnv = createEnv(prevEnv);
        ip->currentEnv = localEnv;
    }

    for (size_t i = 0; i < blockNode->childCount; i++) {
        if (ip->hadRuntimeError || ip->flow != FLOW_NONE) break;
        if (isLabel(blockNode->children[i], "Statement")) {
            execStatement(ip, blockNode->children[i]);
        }
    }

    if (createScope) {
        ip->currentEnv = prevEnv;
        destroyEnv(localEnv);
    }
}

static void execStatement(Interpreter *ip, const ParseNode *node) {
    if (node == NULL || ip->hadRuntimeError || ip->flow != FLOW_NONE) return;

    if (isLabel(node, "Statement")) {
        for (size_t i = 0; i < node->childCount; i++) {
            if (!isPunctuationNode(node->children[i])) {
                execStatement(ip, node->children[i]);
                if (ip->hadRuntimeError || ip->flow != FLOW_NONE) break;
            }
        }
        return;
    }

    if (isLabel(node, "SimpleStatement") || isLabel(node, "BlockStatement")) {
        const ParseNode *inner = firstMeaningfulInRange(node, 0, node->childCount);
        if (inner != NULL) execStatement(ip, inner);
        return;
    }

    // Declarations already registered in Pass 1
    if (isLabel(node, "FnDecl") || isLabel(node, "StateDecl") || isLabel(node, "ReactDecl")) {
        return;
    }

    if (isLabel(node, "VarDecl"))        { execVarDecl(ip, node); return; }
    if (isLabel(node, "EnsureStmt"))     { execEnsureStmt(ip, node); return; }
    if (isLabel(node, "Assignment"))     { execAssignment(ip, node); return; }
    if (isLabel(node, "PrintStmt"))      { execPrintStmt(ip, node); return; }
    if (isLabel(node, "SleepStmt"))      { execSleepStmt(ip, node); return; }
    if (isLabel(node, "TransitionStmt")) { execTransitionStmt(ip, node); return; }
    if (isLabel(node, "ReturnStmt"))     { execReturnStmt(ip, node); return; }
    if (isLabel(node, "IfStmt"))         { execIfStmt(ip, node); return; }
    if (isLabel(node, "LoopStmt"))       { execLoopStmt(ip, node); return; }
    if (isLabel(node, "ForStmt"))        { execForStmt(ip, node); return; }
    if (isLabel(node, "MatchStmt"))      { execMatchStmt(ip, node); return; }
    if (isLabel(node, "QUIT"))           { ip->flow = FLOW_QUIT; return; }

    // Standalone expression statement (e.g., function call)
    Value discarded = evalExpression(ip, node);
    freeValue(discarded);
}

// ============================================================================
// PASS 1: REGISTER TOP-LEVEL FUNCTIONS, STATES & REACTIVE WATCHERS
// ============================================================================
static void registerDeclarations(Interpreter *ip, const ParseNode *root) {
    for (size_t i = 0; i < root->childCount; i++) {
        const ParseNode *stmt = root->children[i];
        if (!isLabel(stmt, "Statement")) continue;

        const ParseNode *blockStmt = findChild(stmt, "BlockStatement", 0);
        if (blockStmt == NULL) continue;

        const ParseNode *decl = firstMeaningfulInRange(blockStmt, 0, blockStmt->childCount);
        if (decl == NULL) continue;

        if (isLabel(decl, "FnDecl")) {
            const ParseNode *nameNode = findChild(decl, "IDENTIFIER", 0);
            if (nameNode != NULL) registerFunction(ip, nodeLexeme(nameNode), decl);
        } else if (isLabel(decl, "StateDecl")) {
            const ParseNode *nameNode = findChild(decl, "IDENTIFIER", 0);
            const ParseNode *bodyNode = findChild(decl, "Block", 0);
            if (nameNode != NULL && bodyNode != NULL) {
                registerState(ip, nodeLexeme(nameNode), bodyNode);
            }
        } else if (isLabel(decl, "ReactDecl")) {
            const ParseNode *watchNode = findChild(decl, "IDENTIFIER", 0);
            const ParseNode *bodyNode  = findChild(decl, "Block", 0);
            if (watchNode != NULL && bodyNode != NULL) {
                registerReact(ip, nodeLexeme(watchNode), bodyNode);
            }
        }
    }
}

// ============================================================================
// CLEANUP & PUBLIC ENTRY POINT
// ============================================================================
static void cleanupInterpreter(Interpreter *ip) {
    freeValue(ip->returnValue);
    destroyEnv(ip->globalEnv);
    free(ip->functions);
    free(ip->states);
    free(ip->reacts);
}

bool executeProgram(const char *sourceFilePath, const ParseNode *root) {
    if (root == NULL) {
        fprintf(stderr, "Error: Cannot interpret an empty parse tree.\n");
        return false;
    }

    Interpreter ip;
    memset(&ip, 0, sizeof(ip));
    ip.returnValue = makeNull();
    if (sourceFilePath != NULL) {
        strncpy(ip.sourceFilePath, sourceFilePath, sizeof(ip.sourceFilePath) - 1);
    }

    ip.globalEnv = createEnv(NULL);
    ip.currentEnv = ip.globalEnv;

    // Pass 1: Hoist fn, state, and react declarations
    registerDeclarations(&ip, root);

    // Pass 2: Execute top-level statements (global initializers, scripts, etc.)
    for (size_t i = 0; i < root->childCount; i++) {
        if (ip.hadRuntimeError || ip.flow != FLOW_NONE) break;
        if (isLabel(root->children[i], "Statement")) {
            execStatement(&ip, root->children[i]);
        }
    }

    // Pass 3: State-machine trampoline loop
    // Starts at an explicit top-level `transition`, or auto-enters `Boot`
    // (or the first declared state) if states exist and top-level didn't `quit`.
    char activeState[128] = {0};
    if (!ip.hadRuntimeError) {
        if (ip.flow == FLOW_TRANSITION) {
            strncpy(activeState, ip.nextState, sizeof(activeState) - 1);
            ip.nextState[0] = '\0';
            ip.flow = FLOW_NONE;
        } else if (ip.flow == FLOW_NONE && ip.stateCount > 0) {
            const char *entry = (lookupState(&ip, "Boot") != NULL)
                                    ? "Boot"
                                    : ip.states[0].name;
            strncpy(activeState, entry, sizeof(activeState) - 1);
        }
    }

    while (activeState[0] != '\0' && !ip.hadRuntimeError && ip.flow != FLOW_QUIT) {
        const ParseNode *stateBlock = lookupState(&ip, activeState);
        if (stateBlock == NULL) {
            runtimeError(&ip, NULL, "State '%s' not found.", activeState);
            break;
        }

        activeState[0] = '\0';
        ip.flow = FLOW_NONE;
        execBlock(&ip, stateBlock, true);

        if (ip.flow == FLOW_TRANSITION) {
            strncpy(activeState, ip.nextState, sizeof(activeState) - 1);
            activeState[sizeof(activeState) - 1] = '\0';
            ip.nextState[0] = '\0';
            ip.flow = FLOW_NONE;
        }
    }

    bool ok = !ip.hadRuntimeError;
    cleanupInterpreter(&ip);
    return ok;
}