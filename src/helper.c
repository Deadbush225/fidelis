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
    char* escaped = calloc(1, 1024);
    char current;

    for (int i = 0; lexeme[i] != '\0'; i++) {
        current = lexeme[i];
        switch (current) {
            case '\n':
                strcat(escaped, "\\n");
                break;
            case '\t':
                strcat(escaped, "\\t");
                break;
            case '\r':
                strcat(escaped, "\\r");
                break;
            case '\0':
                strcat(escaped, "\\0");
                break;
            case '\\':
                strcat(escaped, "\\\\");
                break;
            default:
                strcat(escaped, &current);
        }
    }
    return escaped;
}