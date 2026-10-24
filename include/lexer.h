#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <stdbool.h>

// ============================================================================
// TOKEN TYPES
// ============================================================================

typedef enum {
    // ---- Literals ----
    TOKEN_IDENTIFIER,
    TOKEN_INT_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,

    // ---- Keywords: Data Types ----
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_BOOL,
    TOKEN_CHAR,

    // ---- Keywords: Boolean Literals & Special Values ----
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,

    // ---- Keywords: Structural ----
    TOKEN_STATE,
    TOKEN_REACT,
    TOKEN_LOOP,

    // ---- Keywords: Control Flow ----
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_MATCH,
    TOKEN_TRANSITION,
    TOKEN_ENSURE,

    // ---- Keywords: I/O ----
    TOKEN_PRINT,
    TOKEN_READLINE,
    TOKEN_READINT,

    // ---- Keywords: Modularity ----
    TOKEN_FN,
    TOKEN_RETURN,

    // ---- Keywords: System ----
    TOKEN_QUIT,
    TOKEN_SLEEP,

    // ---- Operators: Arithmetic ----
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_BACKSLASH,
    TOKEN_CARET,

    // ---- Operators: Relational ----
    TOKEN_EQ,        // ==
    TOKEN_NE,        // !=
    TOKEN_LT,        // <
    TOKEN_GT,        // >
    TOKEN_LE,        // <=
    TOKEN_GE,        // >=

    // ---- Operators: Logical ----
    TOKEN_NOT,       // !
    TOKEN_AND,       // &&
    TOKEN_OR,        // ||

    // ---- Operators: Assignment ----
    TOKEN_ASSIGN,         // =
    TOKEN_PLUS_ASSIGN,    // +=
    TOKEN_MINUS_ASSIGN,   // -=
    TOKEN_STAR_ASSIGN,    // *=
    TOKEN_SLASH_ASSIGN,   // /=
    TOKEN_PERCENT_ASSIGN, // %=

    // ---- Operators: Special ----
    TOKEN_PIPELINE,  // |>
    TOKEN_ARROW,     // ->
    TOKEN_QUESTION,  // ?  (optional-type suffix)

    // ---- Delimiters ----
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_COLON,

    // ---- Brackets ----
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,

    // ---- Comments ----
    TOKEN_LINE_COMMENT,
    TOKEN_BLOCK_COMMENT,

    // ---- Special ----
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

// ============================================================================
// TOKEN STRUCTURE
// ============================================================================

typedef struct {
    TokenType type;
    char lexeme[256];  // Increased from 33 to support string/comment content
} Token;

// ============================================================================
// LEXER FUNCTION DECLARATIONS
// ============================================================================

Token getNextToken(FILE *source);
const char* getTokenTypeName(TokenType type);

#endif