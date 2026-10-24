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
    const char *filename = (argc > 1) ? argv[1] : "samples/test_features.swift";
    const char *outputName = (argc > 2) ? argv[2] : "outputs/symbol_table.txt";
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error opening file: %s\n", filename);
        return 1;
    }

    MKDIR("outputs");

    FILE *output = fopen(outputName, "w+");
    if (!output) {
        printf("Error opening output file: %s\n", outputName);
        fclose(file);
        return 1;
    }

    fprintf(output, "     TOKEN TYPE      |        LEXEME       \n");
    fprintf(output, "---------------------|---------------------\n");

    Token t;
    do {
        t = getNextToken(file);
        // if (t.type != TOKEN_EOF) {
        const char *tokenType = getTokenTypeName(t.type);
        const char *lexeme = escapedLexeme(t.lexeme);
          
        fprintf(output, "%-20s | %s\n" , tokenType, lexeme);
        // }
    } while (t.type != TOKEN_EOF);

    fclose(file);
    fclose(output);
    return 0;
}
