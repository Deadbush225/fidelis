#include <stdio.h>
#include "lexer.h"

int main(int argc, char *argv[]) {
    const char *filename = (argc > 1) ? argv[1] : "samples/test_features.swift";

    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error opening file: %s\n", filename);
        return 1;
    }

    Token t;
    do {
        t = getNextToken(file);
        if (t.type != TOKEN_EOF) {
            printToken(t);
        }
    } while (t.type != TOKEN_EOF);

    fclose(file);
    return 0;
}
