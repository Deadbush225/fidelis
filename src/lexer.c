#include "lexer.h"
#include <ctype.h>
#include <string.h>

// Internal DFA States
typedef enum {
    STATE_START,
    STATE_IN_IDENTIFIER,
    STATE_IN_NUMBER,
    STATE_IN_PIPELINE,
    STATE_IN_MINUS,
    STATE_DONE
} LexerState;

// Symbol Table Lookup: Identifies keywords and flags noise words
TokenType lookupKeyword(const char* lexeme, bool* isNoiseWord) {
    *isNoiseWord = false;
    
    // Noise words have no semantic value and are discarded[cite: 1]
    if (strcmp(lexeme, "is") == 0 || strcmp(lexeme, "then") == 0 || 
        strcmp(lexeme, "do") == 0 || strcmp(lexeme, "to") == 0) {
        *isNoiseWord = true;
        return TOKEN_ERROR; 
    }
    
    // Reserved Words[cite: 1]
    if (strcmp(lexeme, "int") == 0) return TOKEN_INT;
    if (strcmp(lexeme, "state") == 0) return TOKEN_STATE;
    if (strcmp(lexeme, "ensure") == 0) return TOKEN_ENSURE;
    if (strcmp(lexeme, "transition") == 0) return TOKEN_TRANSITION;
    
    // If not a keyword or noise word, it is a valid identifier[cite: 1]
    return TOKEN_IDENTIFIER;
}

Token getNextToken(FILE *source) {
    Token token;
    token.lexeme[0] = '\0';
    int lexemeIdx = 0;
    LexerState state = STATE_START;
    int c;

    while (state != STATE_DONE) {
        c = fgetc(source);

        switch (state) {
            case STATE_START:
                if (c == EOF) {
                    token.type = TOKEN_EOF;
                    strcpy(token.lexeme, "EOF");
                    state = STATE_DONE;
                } 
                // Whitespace is treated purely as a separator and ignored[cite: 1]
                else if (isspace(c)) {
                    continue; 
                } 
                // Identifiers must begin with a letter or underscore[cite: 1]
                else if (isalpha(c) || c == '_') {
                    state = STATE_IN_IDENTIFIER;
                    token.lexeme[lexemeIdx++] = c;
                } 
                else if (isdigit(c)) {
                    state = STATE_IN_NUMBER;
                    token.lexeme[lexemeIdx++] = c;
                } 
                else if (c == '|') {
                    state = STATE_IN_PIPELINE;
                    token.lexeme[lexemeIdx++] = c;
                } 
                else if (c == '-') {
                    state = STATE_IN_MINUS;
                    token.lexeme[lexemeIdx++] = c;
                } 
                else if (c == ';') {
                    token.type = TOKEN_SEMICOLON;
                    token.lexeme[lexemeIdx++] = c;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_IDENTIFIER:
                // Subsequent characters can be alphanumeric or underscores[cite: 1]
                if (isalnum(c) || c == '_') {
                    // Enforce the 32-character maximum length[cite: 1]
                    if (lexemeIdx < 32) {
                        token.lexeme[lexemeIdx++] = c;
                    }
                } else {
                    ungetc(c, source); // Put the non-identifier char back in the stream
                    token.lexeme[lexemeIdx] = '\0';
                    
                    bool isNoise;
                    token.type = lookupKeyword(token.lexeme, &isNoise);
                    
                    // If a noise word is detected, skip it by recursively calling getNextToken[cite: 1]
                    if (isNoise) {
                        return getNextToken(source);
                    }
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_PIPELINE:
                if (c == '>') {
                    token.lexeme[lexemeIdx++] = c;
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_PIPELINE; // Yields '|>'[cite: 1]
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_ERROR; 
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_MINUS:
                if (c == '>') {
                    token.lexeme[lexemeIdx++] = c;
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_ARROW; // Yields '->'[cite: 1]
                    state = STATE_DONE;
                } else if (c == '=') {
                    token.lexeme[lexemeIdx++] = c;
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_MINUS_ASSIGN; // Yields '-='[cite: 1]
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_MINUS; // Yields '-'[cite: 1]
                    state = STATE_DONE;
                }
                break;
                
            case STATE_IN_NUMBER:
                if (isdigit(c)) {
                    token.lexeme[lexemeIdx++] = c;
                } else {
                    ungetc(c, source);
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_INT_LITERAL;
                    state = STATE_DONE;
                }
                break;
                
            case STATE_DONE:
                break;
        }
    }
    return token;
}

void printToken(Token t) {
    printf("Token Type: %02d | Lexeme: %s\n", t.type, t.lexeme);
}