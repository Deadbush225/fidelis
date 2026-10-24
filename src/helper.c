#include "helper.h"

/*
escapedLexeme - Handles how the lexeme will be printed. If the loop encounters a
special character, it will print the corresponding special character (e.g \n is
formatted as "\\n") Update (Psalm) - Added FILE *fp parameter to mirror outputs
to a file.

Parameters
    const char* lexeme - A pointer to the lexeme.
*/
char* escapedLexeme(const char* lexeme) {
    enum { ESCAPED_CAPACITY = 1024 };
    char* escaped = calloc(ESCAPED_CAPACITY, sizeof(*escaped));
    size_t outputIndex = 0;

    if (escaped == NULL) {
        return NULL;
    }

    for (size_t inputIndex = 0; lexeme[inputIndex] != '\0'; inputIndex++) {
        const char* replacement = NULL;

        switch (lexeme[inputIndex]) {
            case '\n':
                replacement = "\\n";
                break;
            case '\t':
                replacement = "\\t";
                break;
            case '\r':
                replacement = "\\r";
                break;
            case '\\':
                replacement = "\\\\";
                break;
            default:
                if (outputIndex < ESCAPED_CAPACITY - 1) {
                    escaped[outputIndex++] = lexeme[inputIndex];
                }
                continue;
        }

        while (*replacement != '\0' && outputIndex < ESCAPED_CAPACITY - 1) {
            escaped[outputIndex++] = *replacement++;
        }
    }

    escaped[outputIndex] = '\0';
    return escaped;
}