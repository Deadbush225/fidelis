#include <stdio.h>
#include "lexer.h"

int main(int argc, char *argv[]) {
    const char *filename = (argc > 1) ? argv[1] : "samples/test_features.swift";
    const char *outputName = (argc > 2) ? argv[2] : "outputs/symbol_table.txt";
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error opening file: %s\n", filename);
        return 1;
    }
    FILE *output = fopen(outputName, "w");
    if (!output) {
        printf("Error opening output file: %s\n", outputName);
        fclose(file);
        return 1;
    }

    Token t;
    fprintf(output,"        TOKEN TYPE            |            LEXEME\n");
    do {
        t = getNextToken(file);
        if (t.type != TOKEN_EOF) {
            printToken(t, output);
        }
    } while (t.type != TOKEN_EOF);

    fclose(file);
    fclose(output);
    return 0;
}
