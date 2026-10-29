#include <stdio.h>
#include "lexer.h"
#include "helper.h"

#ifdef _WIN32
    #include <direct.h> // Windows specific header for mkdir
    #define MKDIR(dir) _mkdir(dir)
#else
    #include <sys/stat.h> // Unix based system header for mkdir
    #define MKDIR(dir) mkdir(dir, 0755)
#endif

int main(int argc, char *argv[]) {
    printf("===================== FIDELIS LEXICAL ANALYZER =====================\n");
    /* ━━━━━━━━━━━━━━━━━━━━━━━ Lexical Preprocessing ━━━━━━━━━━━━━━━━━━━━━━━━ */

    const char *inputFileName = (argc > 1) ? argv[1] : "samples/test_features.fdls";
    const char *symbolTableFilePath = "outputs/symbol_table.txt";
    const char *transitionTraceOutputPath = "outputs/transition_trace.txt";

    if (argc == 1) {
        printf("\e[1;31mNo input file specified. Please provide a .fdls file as an argument.\e[0m\n");
        return 1;
    }

    const char *filename = argv[1];
    const char *outputName = "outputs/symbol_table.txt";

    // check if the input file extension is .fdls
    const char *ext = strrchr(inputFileName, '.'); 
    if (ext == NULL || strcmp(ext, ".fdls") != 0) {
        printf("\e[1;31mInvalid file extension. Please use a .fdls file.\e[0m\n");
        return 1;
    }

    // Open the input file for reading
    FILE *inputFileHandle = fopen(inputFileName, "r");
    if (!inputFileHandle) {
      printf("Error opening file: %s\n", inputFileName);
      return 1;
    }
    
    // Create the output directory if it doesn't exist
    MKDIR("outputs");

    // Open/Create the output file for writing
    FILE *output = fopen(symbolTableFilePath, "w+");
    if (!output) {
        printf("Error opening output file: %s\n", symbolTableFilePath);
        fclose(inputFileHandle);
        return 1;
    }

    // Open/Create the transition trace output file for writing
    FILE *transitionFile = fopen(transitionTraceOutputPath, "w+");
    if (!transitionFile) {
      printf("Error opening file: %s\n", transitionTraceOutputPath   );
      return 1;
    }
    
    /* ━━━━━━━━━━━━━━━━━━━━━━━ Lexical Processing ━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    printf("Processing File: %s\n\n", inputFileName);

    // Print 3-column table header (Token Type, Lexeme, and State Transitions) to terminal and file
    // printf("     TOKEN TYPE      |        LEXEME        |          STATE TRANSITIONS          \n");
    // printf("=====================|======================|========================================\n");

    fprintf(output, "     TOKEN TYPE      |        LEXEME        |          STATE TRANSITIONS          \n");
    fprintf(output, "=====================|======================|========================================\n");

    Token t;
    do {
        t = getNextToken(inputFileHandle, transitionFile);
        const char *tokenType = getTokenTypeName(t.type);
        char *lexeme = escapedLexeme(t.lexeme);

        if (lexeme == NULL) {
            printf("Error allocating escaped lexeme\n");
            fclose(inputFileHandle);
            fclose(output);
            return 1;
        }

        // Output each token and its DFA state transition path to terminal and output file
        // printf("%-20s | %-20s | %s\n", tokenType, lexeme, t.transitions);
        fprintf(output, "%-20s | %-20s | %s\n", tokenType, lexeme, t.transitions);
        free(lexeme);
    } while (t.type != TOKEN_EOF);
    
    printf("\nLexical analysis complete.\n");
    printf("Output written to: %s\n", symbolTableFilePath);

    fclose(inputFileHandle);
    fclose(output);
    return 0;
}
