#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* escapedLexeme(const char* lexeme);
char* escapeCharacter(char c);
void printSymbolTableRow(FILE *output, int line, int column, const char *tokenType, const char *lexeme, const char *transitions);
void makeTempPath(char *buf, size_t bufSize, const char *suffix);
void printPhaseHeader(const char *title);
const char *phaseBadge(int code);

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_DIM     "\033[2m"
#define CLR_RED     "\033[1;31m"
#define CLR_GREEN   "\033[1;32m"
#define CLR_YELLOW  "\033[1;33m"
#define CLR_CYAN    "\033[1;36m"

#endif  // HELPER_H