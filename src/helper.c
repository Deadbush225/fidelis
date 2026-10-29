#include "helper.h"
#include <stdio.h>
#include <string.h>

/*
escapedLexeme - Handles how the lexeme will be printed. If the loop encounters a
special character, it will print the corresponding special character (e.g \n is
formatted as "\\n") Update (Psalm) - Added FILE *fp parameter to mirror outputs
to a file.

Parameters
    const char* lexeme - A pointer to the lexeme.
*/
char* escapedLexeme(const char* lexeme) {
    if (lexeme == NULL) {
        return NULL;
    }

    size_t inputLength = strlen(lexeme);
    size_t escapedLength = 1;

    for (size_t i = 0; i < inputLength; i++) {
        switch (lexeme[i]) {
            case '\n':
            case '\t':
            case '\r':
            case '\\':
                escapedLength += 2;
                break;
            default:
                escapedLength += 1;
                break;
        }
    }

    char* escaped = malloc(escapedLength);
    if (escaped == NULL) {
        return NULL;
    }

    size_t outputIndex = 0;

    for (size_t i = 0; i < inputLength; i++) {
        switch (lexeme[i]) {
            case '\n':
                escaped[outputIndex++] = '\\';
                escaped[outputIndex++] = 'n';
                break;
            case '\t':
                escaped[outputIndex++] = '\\';
                escaped[outputIndex++] = 't';
                break;
            case '\r':
                escaped[outputIndex++] = '\\';
                escaped[outputIndex++] = 'r';
                break;
            case '\\':
                escaped[outputIndex++] = '\\';
                escaped[outputIndex++] = '\\';
                break;
            default:
                escaped[outputIndex++] = lexeme[i];
                break;
        }
    }

    escaped[outputIndex] = '\0';
    return escaped;
}

char* escapeCharacter(char c) {
    switch (c) {
        case '\n':
            return "\\n";
        case '\t':
            return "\\t";
        case '\r':
            return "\\r";
        case '\\':
            return "\\\\";
        default: {
            char* str = malloc(2);
            if (str == NULL) {
                return NULL;
            }
            str[0] = c;
            str[1] = '\0';
            return str;
        }
    }
}

// Auto-wrapping table printer
void printSymbolTableRow(FILE* out, const char* type, const char* lexeme, const char* transitions) {
    const int TYPE_WIDTH = 32;
    const int LEX_WIDTH  = 50;
    const int TRANS_WIDTH = 80; // Wraps transitions so they don't stretch infinitely
    
    int lexLen = strlen(lexeme);
    int transLen = strlen(transitions);
    int lexPos = 0, transPos = 0;
    int isFirstLine = 1;

    // Apply red text for errors, but only if printing to the terminal (stdout)
    int isError = (strncmp(type, "ERR_", 4) == 0);
    const char *color = (isError && out == stdout) ? "\e[0;31m" : "";
    const char *reset = (out == stdout) ? "\e[0m" : "";

    while (lexPos < lexLen || transPos < transLen || isFirstLine) {
        // --- COLUMN 1: TOKEN TYPE ---
        if (isFirstLine) {
            fprintf(out, "%s%-*s%s | ", color, TYPE_WIDTH, type, reset);
        } else {
            fprintf(out, "%*s | ", TYPE_WIDTH, ""); // Empty space for wrapped lines
        }

        // --- COLUMN 2: LEXEME ---
        char lexChunk[256] = {0};
        if (lexPos < lexLen) {
            int chunkLen = (lexLen - lexPos > LEX_WIDTH) ? LEX_WIDTH : (lexLen - lexPos);
            strncpy(lexChunk, lexeme + lexPos, chunkLen);
            lexPos += chunkLen;
        }
        fprintf(out, "%s%-*s%s | ", color, LEX_WIDTH, lexChunk, reset);

        // --- COLUMN 3: STATE TRANSITIONS ---
        char transChunk[256] = {0};
        if (transPos < transLen) {
            int chunkLen = (transLen - transPos > TRANS_WIDTH) ? TRANS_WIDTH : (transLen - transPos);
            strncpy(transChunk, transitions + transPos, chunkLen);
            transPos += chunkLen;
            fprintf(out, "%s\n", transChunk);
        } else {
            fprintf(out, "\n");
        }
        
        isFirstLine = 0;
    }
}