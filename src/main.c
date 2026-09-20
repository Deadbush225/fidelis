#include <stdio.h>
#include "lexer.h"

int main() {
    FILE *file = fopen("samples/source_file.swift", "r");
    if (!file) {
        printf("Error opening file.\n");
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