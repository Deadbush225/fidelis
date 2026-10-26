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
    printf("━━━━━━━━━━━━━━━━━━━━━ FIDELIS LEXICAL ANALYZER ━━━━━━━━━━━━━━━━━━━━━\n");
    /* ━━━━━━━━━━━━━━━━━━━━━━━ Lexical Preprocessing ━━━━━━━━━━━━━━━━━━━━━━━━ */

    const char *filename = (argc > 1) ? argv[1] : "samples/test_features.swift";
    const char *outputName = "outputs/symbol_table.txt";

    if (argc == 1) {
        printf("\e[1;31mNo input file specified. Using default: %s (REMOVE THIS IN PROD!!!) \e[0m\n", filename);
    }

    // check if the input file extension is .fdls
    const char *ext = strrchr(filename, '.'); 
    if (ext == NULL || strcmp(ext, ".fdls") != 0) {
        printf("\e[1;31mInvalid file extension. Please use a .fdls file. (ENFORCE THIS IN PROD!!!)\e[0m\n");
        // return 1;
    }

    // Open the input file for reading
    FILE *file = fopen(filename, "r");
    if (!file) {
      printf("Error opening file: %s\n", filename);
      return 1;
    }
    
    // Create the output directory if it doesn't exist
    MKDIR("outputs");

    // Open/Create the output file for writing
    FILE *output = fopen(outputName, "w+");
    if (!output) {
        printf("Error opening output file: %s\n", outputName);
        fclose(file);
        return 1;
    }
    
    /* ━━━━━━━━━━━━━━━━━━━━━━━ Lexical Processing ━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
    printf("Processing File: %s\n", filename);

    fprintf(output, "     TOKEN TYPE      |        LEXEME       \n");
    fprintf(output, "━━━━━━━━━━━━━━━━━━━━━|━━━━━━━━━━━━━━━━━━━━━\n");

    Token t;
    do {
        t = getNextToken(file);
        const char *tokenType = getTokenTypeName(t.type);
        char *lexeme = escapedLexeme(t.lexeme);

        if (lexeme == NULL) {
            printf("Error allocating escaped lexeme\n");
            fclose(file);
            fclose(output);
            return 1;
        }

        fprintf(output, "%-20s | %s\n" , tokenType, lexeme);
        free(lexeme);
    } while (t.type != TOKEN_EOF);
    
    printf("Lexical analysis complete.\n");
    printf("Output written to: %s\n", outputName);

    fclose(file);
    fclose(output);
    return 0;
}
