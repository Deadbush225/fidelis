#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* escapedLexeme(const char* lexeme);
char* escapeCharacter(char c);
void printSymbolTableRow(FILE *output, int line, int column, const char *tokenType, const char *lexeme, const char *transitions);

#endif  // LEXER_H