#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "syntax.h"
#include <stdbool.h>
#include <stddef.h>

#define INTERP_MAX_CALL_DEPTH  256
#define INTERP_MAX_REACT_DEPTH 32
#define INTERP_MAX_ARGS        32

// ============================================================================
// RUNTIME VALUE REPRESENTATION
// ============================================================================
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_CHAR,
    VAL_ARRAY,
    VAL_NULL
} ValueKind;

typedef struct Value Value;

typedef struct {
    Value *items;
    size_t count;
    size_t capacity;
    size_t refCount;
} ArrayObject;

struct Value {
    ValueKind kind;
    union {
        long long intVal;
        double floatVal;
        char *strVal;
        bool boolVal;
        char charVal;
        ArrayObject *arrVal;
    } as;
};

// ============================================================================
// RUNTIME ENVIRONMENTS, REGISTRIES & CONTROL FLOW
// ============================================================================
typedef struct {
    char name[128];
    Value value;
} VarBinding;

typedef struct Env {
    struct Env *parent;
    VarBinding *bindings;
    size_t count;
    size_t capacity;
} Env;

typedef struct {
    char name[128];
    const ParseNode *fnNode;
} FnEntry;

typedef struct {
    char name[128];
    const ParseNode *blockNode;
} StateEntry;

typedef struct {
    char watchedVar[128];
    const ParseNode *blockNode;
} ReactEntry;

typedef enum {
    FLOW_NONE,
    FLOW_RETURN,
    FLOW_TRANSITION,
    FLOW_QUIT
} FlowSignal;

typedef struct {
    char sourceFilePath[256];

    Env *globalEnv;
    Env *currentEnv;

    FnEntry *functions;
    size_t fnCount;
    size_t fnCapacity;

    StateEntry *states;
    size_t stateCount;
    size_t stateCapacity;

    ReactEntry *reacts;
    size_t reactCount;
    size_t reactCapacity;

    FlowSignal flow;
    Value returnValue;
    char nextState[128];

    int callDepth;
    int reactDepth;
    bool hadRuntimeError;
} Interpreter;

// ============================================================================
// PUBLIC API
// ============================================================================

/*
executeProgram - Interprets a validated Fidelis parse tree starting from root.

Returns:
    bool - true if execution completed cleanly without runtime errors.
*/
bool executeProgram(const char *sourceFilePath, const ParseNode *root);

#endif // INTERPRETER_H