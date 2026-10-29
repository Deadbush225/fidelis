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