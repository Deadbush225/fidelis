#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "helper.h"
#include "syntax.h"

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
static int runParser(const char *symbolTableFilePath, const char *parseOutputPath);

// ============================================================================
// MAIN — Entry point
// ============================================================================
// Usage: fidelis.exe <input.fdls>
//
// Phase 1: Lexical Analysis — produces symbol_table.txt
// Phase 2: Syntax Analysis (Recursive Descent Parsing) — produces parse_tree.txt
// ============================================================================
int main(int argc, char *argv[]) {
    printf("===================== FIDELIS COMPILER =====================\n");
    printf("  Lexical Analysis + Recursive Descent Parsing\n");
    printf("============================================================\n\n");

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     *  PHASE 0: ARGUMENT VALIDATION
     * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

    if (argc == 1) {
        printf("\e[1;31mNo input file specified. Please provide a .fdls file as an argument.\e[0m\n");
        printf("Usage: fidelis.exe <input_file.fdls>\n");
        return 1;
    }

    const char *inputFileName = argv[1];

    // Validate file extension
    const char *ext = strrchr(inputFileName, '.');
    if (ext == NULL || strcmp(ext, ".fdls") != 0) {
        printf("\e[1;31mInvalid file extension. Please use a .fdls file.\e[0m\n");
        return 1;
    }

    // Create the output directory if it doesn't exist
    MKDIR("outputs");

    const char *symbolTableFilePath   = "outputs/symbol_table.txt";
    const char *transitionTracePath   = "outputs/transition_trace.txt";
    const char *parseOutputPath       = "outputs/parse_tree.txt";

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     *  PHASE 1: LEXICAL ANALYSIS
     * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    printf("===============================================================\n");
    printf("  PHASE 1: LEXICAL ANALYSIS\n");
    printf("===============================================================\n\n");

    int lexResult = runLexer(inputFileName, symbolTableFilePath, transitionTracePath);
    if (lexResult != 0) {
        return lexResult;
    }

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     *  PHASE 2: SYNTAX ANALYSIS (RECURSIVE DESCENT PARSING)
     * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    printf("\n===============================================================\n");
    printf("  PHASE 2: SYNTAX ANALYSIS (Recursive Descent Parser)\n");
    printf("===============================================================\n\n");

    int parseResult = runParser(symbolTableFilePath, parseOutputPath);

    /* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
     *  SUMMARY
     * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    printf("\n===============================================================\n");
    printf("  BUILD SUMMARY\n");
    printf("===============================================================\n");
    printf("  Input file:     %s\n", inputFileName);
    printf("  Symbol table:   %s\n", symbolTableFilePath);
    printf("  Parse tree:     %s\n", parseOutputPath);
    printf("  Transition log: %s\n", transitionTracePath);

    // if (lexResult == 0 && parseResult == 0) {
    //     printf("\n  \e[1;32m✓ BUILD SUCCESSFUL — No lexical or syntax errors.\e[0m\n");
    // } else {
    //     printf("\n  \e[1;31m✗ BUILD COMPLETED WITH ERRORS.\e[0m\n");
    // }
    // printf("===============================================================\n");

    // return (lexResult != 0 || parseResult != 0) ? 1 : 0;
}

// ============================================================================
// runLexer — Phase 1: Tokenize the input .fdls file
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
    FILE *transitionFile = fopen(transitionTraceOutputPath, "w+");
    if (!transitionFile) {
        printf("Error opening file: %s\n", transitionTraceOutputPath);
        fclose(inputFileHandle);
        fclose(output);
        return 1;
    }

    // Write symbol table header
    fprintf(output, "%-32s | %-50s | %-70s\n",
            "           TOKEN TYPE", "                    LEXEME",
            "                       STATE TRANSITIONS");
    fprintf(output, "=================================|"
                    "====================================================|"
                    "=======================================================================\n");

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
            fclose(transitionFile);
            return 1;
        }

        printSymbolTableRow(output, tokenType, lexeme, t.transitions);
        tokenCount++;
        free(lexeme);
    } while (t.type != TOKEN_EOF);

    printf("  Lexical analysis complete — %d tokens generated.\n", tokenCount);
    printf("  Output written to: %s\n", symbolTableFilePath);

    fclose(inputFileHandle);
    fclose(output);
    fclose(transitionFile);
    return 0;
}

// ============================================================================
// runParser — Phase 2: Recursive Descent Parsing of the symbol table
// ============================================================================
static int runParser(const char *symbolTableFilePath, const char *parseOutputPath) {
    printf("Parsing symbol table: %s\n\n", symbolTableFilePath);

    bool success = parseSourceFile(symbolTableFilePath, parseOutputPath);

    if (success) {
        printf("  \e[1;32m✓ Syntax analysis complete — No syntax errors.\e[0m\n");
    } else {
        printf("  \e[1;31m✗ Syntax analysis complete — Errors detected.\e[0m\n");
    }
    printf("  Parse trace written to: %s\n", parseOutputPath);

    return success ? 0 : 1;
}
