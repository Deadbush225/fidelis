#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <stdbool.h>

// Define Token Types based on Fidelis syntax
typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_INT_LITERAL,
    
    // Keywords & Reserved Words
    TOKEN_INT, TOKEN_STRING, TOKEN_STATE, TOKEN_ENSURE, TOKEN_TRANSITION,
    
    // Operators[cite: 1]
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_ASSIGN, TOKEN_MINUS_ASSIGN,
    TOKEN_PIPELINE,  // |>
    TOKEN_ARROW,     // ->
    
    // Delimiters[cite: 1]
    TOKEN_SEMICOLON, TOKEN_LBRACE, TOKEN_RBRACE,
    
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[33]; // 32 characters max + null terminator[cite: 1]
} Token;

Token getNextToken(FILE *source);
void printToken(Token t);

#endif