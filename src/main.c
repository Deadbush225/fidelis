#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "helper.h"
#include "syntax.h"
#include "semantic.h"
#include "interpreter.h"

#ifdef _WIN32
    #include <direct.h> // Windows specific header for mkdir
    #define MKDIR(dir) _mkdir(dir)
#else
    #include <sys/stat.h> // Unix based system header for mkdir
    #define MKDIR(dir) mkdir(dir, 0755)
#endif

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
static int runLexer(const char *inputFileName, const char *symbolTableFilePath, const char *transitionTraceOutputPath);
static int runParser(const char *inputFileName, const char *symbolTableFilePath, const char *parseOutputPath, ParseNode **outRoot, bool checkMode);
// ============================================================================
// MAIN - Entry point
// ============================================================================
// Usage: fidelis.exe <input.fdls>
//
// Phase 1: Lexical Analysis - produces symbol_table.txt
// Phase 2: Syntax Analysis (Recursive Descent Parsing) - produces parse_tree.txt
// Phase 3: Semantic Analysis - produces semantic_report.txt
// Phase 4: Interpretation - executes the program
// ============================================================================
int main(int argc, char *argv[]) {
    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
       PHASE 0: ARGUMENT VALIDATION
       ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    bool checkMode = false;
    const char *inputFileName = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--check") == 0) {
            checkMode = true;
        } else if (inputFileName == NULL) {
            inputFileName = argv[i];
        }
    }

    if (!checkMode) {
        printf(CLR_CYAN "===================== FIDELIS COMPILER ======================" CLR_RESET "\n");
        printf("  " CLR_DIM "Lexical, Syntax & Semantic Analysis + Interpreter" CLR_RESET "\n");
        printf(CLR_CYAN "=============================================================" CLR_RESET "\n");
    }

    if (inputFileName == NULL) {
        fprintf(stderr, CLR_RED "No input file specified. Please provide a .fdls file as an argument." CLR_RESET "\n");
        fprintf(stderr, "Usage: %s [--check] <input_file.fdls>\n", argv[0]);
        return 1;
    }

    // Validate file extension
    const char *ext = strrchr(inputFileName, '.');
    if (ext == NULL || strcmp(ext, ".fdls") != 0) {
        fprintf(stderr, CLR_RED "Invalid file extension. Please use a .fdls file." CLR_RESET "\n");
        return 1;
    }

    // Output paths: temp directory in --check mode, outputs/ in normal mode
    char symbolTableFilePath[512];
    char transitionTraceOutputPath[512];
    char parseOutputPath[512];
    char semanticOutputPath[512];

    const char *traceArg    = NULL;
    const char *parseArg    = NULL;
    const char *semanticArg = NULL;

    if (checkMode) {
        makeTempPath(symbolTableFilePath, sizeof(symbolTableFilePath), "symbol_table.txt");
        // Keep transitionTraceOutputPath only if your runLexer() doesn't accept NULL yet:
        makeTempPath(transitionTraceOutputPath, sizeof(transitionTraceOutputPath), "transition_trace.txt");
        traceArg    = NULL;
        parseArg    = NULL;
        semanticArg = NULL;
    } else {
        MKDIR("outputs");
        snprintf(symbolTableFilePath, sizeof(symbolTableFilePath), "outputs/symbol_table.txt");
        snprintf(transitionTraceOutputPath, sizeof(transitionTraceOutputPath), "outputs/transition_trace.txt");
        snprintf(parseOutputPath,     sizeof(parseOutputPath),     "outputs/parse_tree.txt");
        snprintf(semanticOutputPath,  sizeof(semanticOutputPath),  "outputs/semantic_report.txt");
        traceArg    = transitionTraceOutputPath;
        parseArg    = parseOutputPath;
        semanticArg = semanticOutputPath;
    }

    // Track per-phase results: 0 = PASS, 1 = FAIL, -1 = SKIPPED
    int lexResult      = -1;
    int parseResult    = -1;
    int semanticResult = -1;
    int execResult     = -1;
    ParseNode *parseTree = NULL;

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
       PHASE 1: LEXICAL ANALYSIS
       ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    if (!checkMode) printPhaseHeader("PHASE 1: LEXICAL ANALYSIS");

    lexResult = runLexer(inputFileName, symbolTableFilePath, traceArg);
    if (lexResult != 0) {
        goto summary;
    }

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
       PHASE 2: SYNTAX ANALYSIS (RECURSIVE DESCENT PARSING)
       ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    if (!checkMode) printPhaseHeader("PHASE 2: SYNTAX ANALYSIS (Recursive Descent Parser)");

    parseResult = runParser(inputFileName, symbolTableFilePath, parseArg, &parseTree, checkMode);

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
       PHASE 3: SEMANTIC ANALYSIS
       ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    if (parseResult == 0 && parseTree != NULL) {
        if (!checkMode) printPhaseHeader("PHASE 3: SEMANTIC ANALYSIS");

        bool semOk = analyzeSemantics(inputFileName, parseTree, semanticArg);
        semanticResult = semOk ? 0 : 1;

        if (!checkMode) {
            if (semOk) {
                printf("  " CLR_GREEN "[OK] Semantic analysis complete - No semantic errors." CLR_RESET "\n");
            } else {
                printf("  " CLR_RED "[ERR] Semantic analysis complete - Errors detected." CLR_RESET "\n");
            }
            printf("  Semantic report written to: " CLR_DIM "%s" CLR_RESET "\n", semanticOutputPath);
        }
    }

summary:
    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
       BUILD SUMMARY
       ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    if (!checkMode) {
        printPhaseHeader("BUILD SUMMARY");
        printf("  " CLR_YELLOW "Input file:     " CLR_RESET "%s\n", inputFileName);
        printf("  " CLR_YELLOW "Symbol table:   " CLR_RESET CLR_DIM "%s" CLR_RESET "\n", symbolTableFilePath);
        printf("  " CLR_YELLOW "Transition log: " CLR_RESET CLR_DIM "%s" CLR_RESET "\n", transitionTraceOutputPath);
        printf("  " CLR_YELLOW "Parse tree:     " CLR_RESET CLR_DIM "%s" CLR_RESET "\n", parseOutputPath);
        printf("  " CLR_YELLOW "Semantic rep.:  " CLR_RESET CLR_DIM "%s" CLR_RESET "\n\n", semanticOutputPath);

        printf("  Phase 1 (Lexer):    %s\n", phaseBadge(lexResult));
        printf("  Phase 2 (Syntax):   %s\n", phaseBadge(parseResult));
        printf("  Phase 3 (Semantic): %s\n", phaseBadge(semanticResult));

        if (lexResult == 0 && parseResult == 0 && semanticResult == 0) {
            printf("\n  " CLR_GREEN "[OK] BUILD SUCCESSFUL - Ready for execution." CLR_RESET "\n");
        } else {
            printf("\n  " CLR_RED "[ERR] BUILD FAILED - Execution skipped." CLR_RESET "\n");
        }
        printf(CLR_CYAN "===============================================================" CLR_RESET "\n");
    }

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
       PHASE 4: EXECUTION (INTERPRETER)
       ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    if (!checkMode && lexResult == 0 && parseResult == 0 && semanticResult == 0 && parseTree != NULL) {
        printPhaseHeader("PHASE 4: EXECUTION");

        bool execOk = executeProgram(inputFileName, parseTree);
        execResult = execOk ? 0 : 1;

        printf("\n" CLR_CYAN "===============================================================" CLR_RESET "\n");
        if (execOk) {
            printf("  " CLR_GREEN "[OK] Program exited cleanly." CLR_RESET "\n");
        } else {
            printf("  " CLR_RED "[ERR] Program terminated with a runtime error." CLR_RESET "\n");
        }
        printf(CLR_CYAN "===============================================================" CLR_RESET "\n");
    }

    freeParseTree(parseTree);

    if (checkMode) {
        remove(symbolTableFilePath);
        remove(transitionTraceOutputPath);
        remove(parseOutputPath);
        remove(semanticOutputPath);
        if (traceArg != NULL) remove(traceArg);
    }

    bool buildOk = (lexResult == 0 && parseResult == 0 && semanticResult == 0);
    return (buildOk && (checkMode || execResult == 0)) ? 0 : 1;
}

// ============================================================================
// runLexer - Phase 1: Tokenize the input .fdls file
// ============================================================================
static int runLexer(const char *inputFileName, const char *symbolTableFilePath, const char *transitionTraceOutputPath) {
    printf("Processing File: %s\n\n", inputFileName);

    // Open the input file for reading
    FILE *inputFileHandle = fopen(inputFileName, "r");
    if (!inputFileHandle) {
        printf("Error opening file: %s\n", inputFileName);
        return 1;
    }

    // Open/Create the symbol table output file
    FILE *output = fopen(symbolTableFilePath, "w+");
    if (!output) {
        printf("Error opening output file: %s\n", symbolTableFilePath);
        fclose(inputFileHandle);
        return 1;
    }

    // Open/Create the transition trace output file
    FILE *transitionFile = NULL;
    if (transitionTraceOutputPath != NULL) {
        transitionFile = fopen(transitionTraceOutputPath, "w");
        if (transitionFile == NULL) {
            fprintf(stderr, "Error: Could not create transition trace file: %s\n", transitionTraceOutputPath);
            fclose(inputFileHandle);
            fclose(output);
            return 1;
        }
    }

    // Write symbol table header
    fprintf(output, "%-6s | %-6s | %-32s | %-50s | %-80s\n",
            "LINE", "COL",
            "           TOKEN TYPE",
            "                    LEXEME",
            "                            STATE TRANSITIONS");
    fprintf(output, "=======|========|"
                    "==================================|"
                    "====================================================|"
                    "=================================================================================\n");

    resetLexerPosition();

    // Tokenize the input file
    Token t;
    int tokenCount = 0;
    do {
        t = getNextToken(inputFileHandle, transitionFile);
        const char *tokenType = getTokenTypeName(t.type);
        char *lexeme = escapedLexeme(t.lexeme);

        if (lexeme == NULL) {
            printf("Error allocating escaped lexeme\n");
            fclose(inputFileHandle);
            fclose(output);
            if (transitionFile != NULL) {
                fclose(transitionFile);
            }
            return 1;
        }

        printSymbolTableRow(output, t.line, t.column, tokenType, lexeme, t.transitions);
        tokenCount++;
        free(lexeme);
    } while (t.type != TOKEN_EOF);

    printf("  Lexical analysis complete - %d tokens generated.\n", tokenCount);
    printf("  Output written to: %s\n", symbolTableFilePath);

    fclose(inputFileHandle);
    fclose(output);
    if (transitionFile != NULL) {
        fclose(transitionFile);
    }
    return 0;
}

// ============================================================================
// runParser - Phase 2: Recursive Descent Parsing of the symbol table
// ============================================================================
static int runParser(const char *inputFileName, const char *symbolTableFilePath,
                     const char *parseOutputPath, ParseNode **outRoot, bool checkMode) {
    if (!checkMode) {
        printf("Parsing symbol table: %s\n\n", symbolTableFilePath);
    }

    bool success = parseSourceFile(inputFileName, symbolTableFilePath,
                                   parseOutputPath, outRoot, checkMode);

    if (!checkMode) {
        if (success) {
            printf("  " CLR_GREEN "[OK] Syntax analysis complete - No syntax errors." CLR_RESET "\n");
        } else {
            printf("  " CLR_RED "[ERR] Syntax analysis complete - Errors detected." CLR_RESET "\n");
        }
        printf("  Parse tree written to: " CLR_DIM "%s" CLR_RESET "\n", parseOutputPath);
    }
    return success ? 0 : 1;
}