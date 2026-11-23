#include "helper.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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
void printSymbolTableRow(FILE* out, int line, int column, const char* type, const char* lexeme, const char* transitions) {
    const int LINE_WIDTH  = 6;
    const int COL_WIDTH   = 6;
    const int TYPE_WIDTH  = 32;
    const int LEX_WIDTH   = 50;
    const int TRANS_WIDTH = 80; // Wraps transitions so they don't stretch infinitely

    int lexLen = (lexeme != NULL) ? (int)strlen(lexeme) : 0;
    int transLen = (transitions != NULL) ? (int)strlen(transitions) : 0;
    int lexPos = 0, transPos = 0;
    int isFirstLine = 1;

    // Apply red text for errors, but only if printing to the terminal (stdout)
    int isError = (strncmp(type, "ERR_", 4) == 0);
    const char *color = (isError && out == stdout) ? "\e[0;31m" : "";
    const char *reset = (out == stdout) ? "\e[0m" : "";

    while (lexPos < lexLen || transPos < transLen || isFirstLine) {
        // --- COLUMNS 1 & 2: LINE & COLUMN ---
        if (isFirstLine) {
            fprintf(out, "%s%-*d%s | %s%-*d%s | ",
                    color, LINE_WIDTH, line, reset,
                    color, COL_WIDTH, column, reset);
        } else {
            fprintf(out, "%*s | %*s | ", LINE_WIDTH, "", COL_WIDTH, "");
        }

        // --- COLUMN 3: TOKEN TYPE ---
        if (isFirstLine) {
            fprintf(out, "%s%-*s%s | ", color, TYPE_WIDTH, type, reset);
        } else {
            fprintf(out, "%*s | ", TYPE_WIDTH, ""); // Empty space for wrapped lines
        }

        // --- COLUMN 4: LEXEME ---
        char lexChunk[256] = {0};
        if (lexPos < lexLen) {
            int chunkLen = (lexLen - lexPos > LEX_WIDTH) ? LEX_WIDTH : (lexLen - lexPos);
            strncpy(lexChunk, lexeme + lexPos, chunkLen);
            lexPos += chunkLen;
        }
        fprintf(out, "%s%-*s%s | ", color, LEX_WIDTH, lexChunk, reset);

        // --- COLUMN 5: STATE TRANSITIONS ---
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

#ifdef _WIN32
#include <process.h>
#define GET_PID() _getpid()
#define PATH_SEP  '\\'
#else
#include <unistd.h>
#define GET_PID() getpid()
#define PATH_SEP  '/'
#endif

void makeTempPath(char *buf, size_t bufSize, const char *suffix) {
    const char *dir = getenv("TEMP");
    if (dir == NULL || dir[0] == '\0') dir = getenv("TMP");
    if (dir == NULL || dir[0] == '\0') dir = "/tmp";

    size_t len = strlen(dir);
    bool hasSep = (len > 0 && (dir[len - 1] == '/' || dir[len - 1] == '\\'));
    if (hasSep) {
        snprintf(buf, bufSize, "%sfidelis_%d_%s", dir, (int)GET_PID(), suffix);
    } else {
        snprintf(buf, bufSize, "%s%cfidelis_%d_%s", dir, PATH_SEP, (int)GET_PID(), suffix);
    }
}

void printPhaseHeader(const char *title) {
    printf("\n" CLR_CYAN "===============================================================" CLR_RESET "\n");
    printf("  " CLR_BOLD "%s" CLR_RESET "\n", title);
    printf(CLR_CYAN "===============================================================" CLR_RESET "\n\n");
}

// Formats a phase status code: 0 = PASS, -1 = SKIP, anything else = FAIL
const char *phaseBadge(int code) {
    if (code == 0)  return CLR_GREEN  "[PASS]" CLR_RESET;
    if (code == -1) return CLR_YELLOW "[SKIP]" CLR_RESET;
    return CLR_RED "[FAIL]" CLR_RESET;
}