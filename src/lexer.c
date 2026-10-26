#include "lexer.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// EXTENDED DFA STATES FOR FULL FIDELIS LEXICAL SPECIFICATION
// ============================================================================

typedef enum {
    STATE_START,

    // ---- Comments ----
    STATE_IN_SLASH,
    STATE_IN_LINE_COMMENT,
    STATE_IN_BLOCK_COMMENT,
    STATE_IN_BLOCK_COMMENT_STAR,

    // ---- Number Parsing ----
    STATE_IN_NUMBER,
    STATE_IN_FLOAT,

    // ---- String & Character Parsing ----
    STATE_IN_STRING,
    STATE_IN_CHAR,

    // ---- Operator Branching (Multi-char operators) ----
    STATE_IN_MINUS,
    STATE_IN_PLUS,
    STATE_IN_STAR,
    STATE_IN_PERCENT,
    STATE_IN_PIPE,
    STATE_IN_AMPERSAND,
    STATE_IN_EQUALS,
    STATE_IN_NOT,
    STATE_IN_LT,
    STATE_IN_GT,

    // ---- Keyword DFA Branches ----
    // 'i' branch: is, int, if
    STATE_I, STATE_IS, STATE_IN_KW, STATE_INT, STATE_IF,

    // 'd' branch: do
    STATE_D, STATE_DO,

    // 't' branch: to, then, transition, true
    STATE_T, STATE_TO, STATE_TH, STATE_THE, STATE_THEN,
    STATE_TR, STATE_TRA, STATE_TRAN, STATE_TRANS, STATE_TRANSI, STATE_TRANSIT, STATE_TRANSITI, STATE_TRANSITIO, STATE_TRANSITION,
    STATE_TRU, STATE_TRUE,

    // 's' branch: state, string, sleep
    STATE_S, STATE_ST, STATE_STA, STATE_STAT, STATE_STATE_KW,
    STATE_STR, STATE_STRI, STATE_STRIN, STATE_STRING,
    STATE_SL, STATE_SLE, STATE_SLEE, STATE_SLEEP,

    // 'e' branch: ensure, else
    STATE_E, STATE_EN, STATE_ENS, STATE_ENSU, STATE_ENSUR, STATE_ENSURE,
    STATE_EL, STATE_ELS, STATE_ELSE,

    // 'b' branch: bool
    STATE_B, STATE_BO, STATE_BOO, STATE_BOOL,

    // 'c' branch: char
    STATE_C, STATE_CH, STATE_CHA, STATE_CHAR_KW,

    // 'f' branch: float, false, fn, for
    STATE_F, STATE_FL, STATE_FLO, STATE_FLOA, STATE_FLOAT,
    STATE_FA, STATE_FAL, STATE_FALS, STATE_FALSE,
    STATE_FN, STATE_FO, STATE_FOR,

    // 'n' branch: null
    STATE_N, STATE_NU, STATE_NUL, STATE_NULL,

    // 'r' branch: react, return, readline, readint
    STATE_R, STATE_RE, STATE_REA, STATE_REAC, STATE_REACT,
    STATE_RET, STATE_RETU, STATE_RETUR, STATE_RETURN,
    STATE_READ, STATE_READL, STATE_READLI, STATE_READLIN, STATE_READLINE,
    STATE_READI, STATE_READIN, STATE_READINT,

    // 'l' branch: loop
    STATE_L, STATE_LO, STATE_LOO, STATE_LOOP,

    // 'm' branch: match
    STATE_M, STATE_MA, STATE_MAT, STATE_MATC, STATE_MATCH,

    // 'p' branch: print
    STATE_P, STATE_PR, STATE_PRI, STATE_PRIN, STATE_PRINT,

    // 'q' branch: quit
    STATE_Q, STATE_QU, STATE_QUI, STATE_QUIT,

    // Standard identifier fallback
    STATE_IN_IDENTIFIER,
    STATE_DONE
} LexerState;

// ============================================================================
// UTILITY MACROS
// ============================================================================

// Appends one character to the current lexeme. Input longer than the lexeme
// buffer is truncated rather than written past the end.
#define LEXEME_CAPACITY ((int)sizeof(token.lexeme))

// Safely append one byte to token.lexeme and advance lexemeIdx.
// Inputs:
// - ch: character/byte to append
// Side effects:
// - Writes into token.lexeme when capacity remains
// - Increments lexemeIdx on successful append
// - Silently drops bytes after capacity is reached
#define APPEND_CHAR(ch) \
    do { \
        if (lexemeIdx < LEXEME_CAPACITY - 1) { \
            token.lexeme[lexemeIdx++] = (char)(ch); \
        } \
    } while (0)

// Transition helper for keyword DFA paths.
// Behavior:
// - If c matches expected_char, move to next_state and keep building lexeme.
// - If c continues an identifier, fall back to STATE_IN_IDENTIFIER.
// - Otherwise, c terminates the token: push it back and emit IDENTIFIER.
// This avoids keyword text comparisons by deciding via state transitions.
#define BRANCH_OR_IDENTIFIER(expected_char, next_state) \
    do { \
        if (c == expected_char) { \
            state = next_state; \
            APPEND_CHAR(c); \
        } else if (isalnum(c) || c == '_') { \
            state = STATE_IN_IDENTIFIER; \
            APPEND_CHAR(c); \
        } else { \
            ungetc(c, source); \
            token.lexeme[lexemeIdx] = '\0'; \
            token.type = TOKEN_IDENTIFIER; \
            state = STATE_DONE; \
        } \
    } while(0)

// Finalize a candidate keyword state.
// Behavior:
// - If c is still identifier content, this is not a standalone keyword,
//   so continue as STATE_IN_IDENTIFIER.
// - Otherwise, push c back, terminate lexeme, and emit the keyword token.
#define YIELD_KEYWORD_OR_IDENTIFIER(token_type) \
    do { \
        if (isalnum(c) || c == '_') { \
            state = STATE_IN_IDENTIFIER; \
            APPEND_CHAR(c); \
        } else { \
            ungetc(c, source); \
            token.lexeme[lexemeIdx] = '\0'; \
            token.type = token_type; \
            state = STATE_DONE; \
        } \
    } while(0)

// Handle noise words that should be ignored unless they grow into identifiers.
// Behavior:
// - If c is identifier content, continue scanning as STATE_IN_IDENTIFIER.
// - Otherwise, reset temporary token-building state and restart at STATE_START
//   without recursion, so long runs of ignored text do not grow the call stack.
#define DISCARD_NOISE_OR_IDENTIFIER() \
    do { \
        if (isalnum(c) || c == '_') { \
            state = STATE_IN_IDENTIFIER; \
            APPEND_CHAR(c); \
        } else { \
            /* Restart scanning in the caller's loop instead of recursing so a \
             * long run of noise words cannot exhaust the stack. */ \
            ungetc(c, source); \
            lexemeIdx = 0; \
            escaped = 0; \
            token.lexeme[0] = '\0'; \
            state = STATE_START; \
        } \
    } while(0)

// ============================================================================
// TOKEN GENERATION FUNCTIONS
// ============================================================================

Token getNextToken(FILE *source) {
    Token token;
    token.lexeme[0] = '\0';
    int lexemeIdx = 0;
    int escaped = 0;
    LexerState state = STATE_START;
    int c;

    while (state != STATE_DONE) {
        c = fgetc(source);

        switch (state) {
            // ================================================================
            // START STATE: Initial character dispatch
            // ================================================================
            case STATE_START:
                if (c == EOF) {
                    token.type = TOKEN_EOF;
                    strcpy(token.lexeme, "EOF");
                    state = STATE_DONE;
                }
                else if (isspace(c)) {
                    continue;
                }
                // ---- COMMENTS ----
                else if (c == '/') {
                    state = STATE_IN_SLASH;
                    APPEND_CHAR(c);
                    lexemeIdx = 0;  // Reset for comment content
                }
                // ---- NUMBERS ----
                else if (isdigit(c)) {
                    state = STATE_IN_NUMBER;
                    APPEND_CHAR(c);
                }
                // ---- STRINGS ----
                else if (c == '"') {
                    state = STATE_IN_STRING;
                    APPEND_CHAR(c);
                }
                // ---- CHARACTERS ----
                else if (c == '\'') {
                    state = STATE_IN_CHAR;
                    APPEND_CHAR(c);
                }
                // ---- OPERATORS & MULTI-CHAR ----
                else if (c == '+') {
                    state = STATE_IN_PLUS;
                    APPEND_CHAR(c);
                }
                else if (c == '-') {
                    state = STATE_IN_MINUS;
                    APPEND_CHAR(c);
                }
                else if (c == '*') {
                    state = STATE_IN_STAR;
                    APPEND_CHAR(c);
                }
                else if (c == '%') {
                    state = STATE_IN_PERCENT;
                    APPEND_CHAR(c);
                }
                else if (c == '|') {
                    state = STATE_IN_PIPE;
                    APPEND_CHAR(c);
                }
                else if (c == '&') {
                    state = STATE_IN_AMPERSAND;
                    APPEND_CHAR(c);
                }
                else if (c == '=') {
                    state = STATE_IN_EQUALS;
                    APPEND_CHAR(c);
                }
                else if (c == '!') {
                    state = STATE_IN_NOT;
                    APPEND_CHAR(c);
                }
                else if (c == '<') {
                    state = STATE_IN_LT;
                    APPEND_CHAR(c);
                }
                else if (c == '>') {
                    state = STATE_IN_GT;
                    APPEND_CHAR(c);
                }
                else if (c == '?') {
                    token.type = TOKEN_QUESTION;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                else if (c == '^') {
                    token.type = TOKEN_CARET;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                else if (c == '\\') {
                    token.type = TOKEN_BACKSLASH;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                // ---- DELIMITERS ----
                else if (c == ';') {
                    token.type = TOKEN_SEMICOLON;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                else if (c == ',') {
                    token.type = TOKEN_COMMA;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                else if (c == ':') {
                    token.type = TOKEN_COLON;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                // ---- BRACKETS ----
                else if (c == '(') {
                    token.type = TOKEN_LPAREN;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                else if (c == ')') {
                    token.type = TOKEN_RPAREN;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                else if (c == '{') {
                    token.type = TOKEN_LBRACE;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                else if (c == '}') {
                    token.type = TOKEN_RBRACE;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                else if (c == '[') {
                    token.type = TOKEN_LBRACKET;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                else if (c == ']') {
                    token.type = TOKEN_RBRACKET;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                // ---- KEYWORDS (letter entry points) ----
                else if (c == 'i') { state = STATE_I; APPEND_CHAR(c); }
                else if (c == 'd') { state = STATE_D; APPEND_CHAR(c); }
                else if (c == 't') { state = STATE_T; APPEND_CHAR(c); }
                else if (c == 's') { state = STATE_S; APPEND_CHAR(c); }
                else if (c == 'e') { state = STATE_E; APPEND_CHAR(c); }
                else if (c == 'b') { state = STATE_B; APPEND_CHAR(c); }
                else if (c == 'c') { state = STATE_C; APPEND_CHAR(c); }
                else if (c == 'f') { state = STATE_F; APPEND_CHAR(c); }
                else if (c == 'n') { state = STATE_N; APPEND_CHAR(c); }
                else if (c == 'r') { state = STATE_R; APPEND_CHAR(c); }
                else if (c == 'l') { state = STATE_L; APPEND_CHAR(c); }
                else if (c == 'm') { state = STATE_M; APPEND_CHAR(c); }
                else if (c == 'p') { state = STATE_P; APPEND_CHAR(c); }
                else if (c == 'q') { state = STATE_Q; APPEND_CHAR(c); }
                else if (isalpha(c) || c == '_') {
                    state = STATE_IN_IDENTIFIER;
                    APPEND_CHAR(c);
                }
                else {
                    token.type = TOKEN_ERROR;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            // ================================================================
            // COMMENTS
            // ================================================================
            case STATE_IN_SLASH:
                if (c == '/') {
                    state = STATE_IN_LINE_COMMENT;
                    token.lexeme[0] = '/';
                    token.lexeme[1] = '/';
                    lexemeIdx = 2;
                } else if (c == '*') {
                    state = STATE_IN_BLOCK_COMMENT;
                    token.lexeme[0] = '/';
                    token.lexeme[1] = '*';
                    lexemeIdx = 2;
                } else if (c == '=') {
                    // lexemeIdx was reset to 0 for comment content, so rebuild
                    // the whole operator instead of appending to it.
                    token.type = TOKEN_SLASH_ASSIGN;
                    token.lexeme[0] = '/';
                    token.lexeme[1] = '=';
                    token.lexeme[2] = '\0';
                    lexemeIdx = 0;
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_SLASH;
                    token.lexeme[1] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_LINE_COMMENT:
                if (c == '\n' || c == EOF) {
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_LINE_COMMENT;
                    ungetc(c, source);
                    state = STATE_DONE;
                } else {
                    APPEND_CHAR(c);
                }
                break;

            case STATE_IN_BLOCK_COMMENT:
                if (c == '*') {
                    state = STATE_IN_BLOCK_COMMENT_STAR;
                    APPEND_CHAR(c);
                } else if (c == EOF) {
                    token.type = TOKEN_ERROR;
                    token.lexeme[lexemeIdx] = '\0';
                    strcpy(token.lexeme, "Unterminated block comment");
                    state = STATE_DONE;
                } else {
                    APPEND_CHAR(c);
                }
                break;

            case STATE_IN_BLOCK_COMMENT_STAR:
                if (c == '/') {
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_BLOCK_COMMENT;
                    state = STATE_DONE;
                } else if (c == '*') {
                    state = STATE_IN_BLOCK_COMMENT_STAR;
                    APPEND_CHAR(c);
                } else if (c == EOF) {
                    token.type = TOKEN_ERROR;
                    token.lexeme[lexemeIdx] = '\0';
                    strcpy(token.lexeme, "Unterminated block comment");
                    state = STATE_DONE;
                } else {
                    state = STATE_IN_BLOCK_COMMENT;
                    APPEND_CHAR(c);
                }
                break;

            // ================================================================
            // NUMBER & FLOAT PARSING
            // ================================================================
            case STATE_IN_NUMBER:
                if (isdigit(c)) {
                    APPEND_CHAR(c);
                } else if (c == '.') {
                    state = STATE_IN_FLOAT;
                    APPEND_CHAR(c);
                } else if (isalpha(c) || c == '_') {
                    APPEND_CHAR(c);
                    while ((c = fgetc(source)) != EOF && (isalnum(c) || c == '_')) {
                        if (lexemeIdx < (int)sizeof(token.lexeme) - 1) {
                            APPEND_CHAR(c);
                        }
                    }
                    if (c != EOF) ungetc(c, source);
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_ERROR;
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_INT_LITERAL;
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_FLOAT:
                if (isdigit(c)) {
                    APPEND_CHAR(c);
                } else if (isalpha(c) || c == '_') {
                    APPEND_CHAR(c);
                    while ((c = fgetc(source)) != EOF && (isalnum(c) || c == '_')) {
                        if (lexemeIdx < (int)sizeof(token.lexeme) - 1) {
                            APPEND_CHAR(c);
                        }
                    }
                    if (c != EOF) ungetc(c, source);
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_ERROR;
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_FLOAT_LITERAL;
                    state = STATE_DONE;
                }
                break;

            // ================================================================
            // STRING PARSING
            // ================================================================
            case STATE_IN_STRING:
                if (escaped) {
                    // Keep the backslash and the escaped byte in the lexeme so
                    // the token text matches the source.
                    escaped = 0;
                    APPEND_CHAR(c);
                } else if (c == '\\') {
                    escaped = 1;
                    APPEND_CHAR(c);
                } else if (c == '"') {
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = TOKEN_STRING_LITERAL;
                    state = STATE_DONE;
                } else if (c == EOF || c == '\n' || c == '\r') {
                    token.type = TOKEN_ERROR;
                    token.lexeme[lexemeIdx] = '\0';
                    strcpy(token.lexeme, "Unterminated string");
                    state = STATE_DONE;
                } else {
                    APPEND_CHAR(c);
                }
                break;

            // ================================================================
            // CHARACTER PARSING
            // ================================================================
            case STATE_IN_CHAR:
                if (c == '\'') {
                    // Content sits between the opening quote at index 0 and the
                    // closing quote about to be appended.
                    int contentLen = lexemeIdx - 1;
                    if (contentLen == 1 || (contentLen == 2 && token.lexeme[1] == '\\')) {
                        APPEND_CHAR(c);
                        token.lexeme[lexemeIdx] = '\0';
                        token.type = TOKEN_CHAR_LITERAL;
                    } else {
                        token.type = TOKEN_ERROR;
                        strcpy(token.lexeme, (contentLen == 0) ? "Empty char literal"
                                                               : "Char literal too long");
                    }
                    state = STATE_DONE;
                } else if (c == EOF || c == '\n' || c == '\r') {
                    token.type = TOKEN_ERROR;
                    strcpy(token.lexeme, "Unterminated char");
                    state = STATE_DONE;
                } else if (lexemeIdx - 1 >= 2) {
                    token.type = TOKEN_ERROR;
                    strcpy(token.lexeme, "Char literal too long");
                    state = STATE_DONE;
                } else {
                    APPEND_CHAR(c);
                }
                break;

            // ================================================================
            // OPERATOR BRANCHING
            // ================================================================
            case STATE_IN_PLUS:
                if (c == '=') {
                    token.type = TOKEN_PLUS_ASSIGN;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_PLUS;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_MINUS:
                if (c == '>') {
                    token.type = TOKEN_ARROW;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else if (c == '=') {
                    token.type = TOKEN_MINUS_ASSIGN;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_MINUS;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_STAR:
                if (c == '=') {
                    token.type = TOKEN_STAR_ASSIGN;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_STAR;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_PERCENT:
                if (c == '=') {
                    token.type = TOKEN_PERCENT_ASSIGN;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_PERCENT;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_PIPE:
                if (c == '>') {
                    token.type = TOKEN_PIPELINE;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else if (c == '|') {
                    token.type = TOKEN_OR;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_ERROR;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_AMPERSAND:
                if (c == '&') {
                    token.type = TOKEN_AND;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_ERROR;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_EQUALS:
                if (c == '=') {
                    token.type = TOKEN_EQ;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_ASSIGN;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_NOT:
                if (c == '=') {
                    token.type = TOKEN_NE;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_NOT;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_LT:
                if (c == '=') {
                    token.type = TOKEN_LE;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_LT;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            case STATE_IN_GT:
                if (c == '=') {
                    token.type = TOKEN_GE;
                    APPEND_CHAR(c);
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                } else {
                    ungetc(c, source);
                    token.type = TOKEN_GT;
                    token.lexeme[lexemeIdx] = '\0';
                    state = STATE_DONE;
                }
                break;

            // ================================================================
            // KEYWORD DFA BRANCHES
            // ================================================================

            // 'i' BRANCH: is, int, if
            case STATE_I:
                if (c == 's') { state = STATE_IS; APPEND_CHAR(c); }
                else if (c == 'n') { state = STATE_IN_KW; APPEND_CHAR(c); }
                else if (c == 'f') { state = STATE_IF; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_IS: DISCARD_NOISE_OR_IDENTIFIER(); break;
            case STATE_IN_KW: BRANCH_OR_IDENTIFIER('t', STATE_INT); break;
            case STATE_INT: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_INT); break;
            case STATE_IF: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_IF); break;

            // 'd' BRANCH: do
            case STATE_D: BRANCH_OR_IDENTIFIER('o', STATE_DO); break;
            case STATE_DO: DISCARD_NOISE_OR_IDENTIFIER(); break;

            // 's' BRANCH: state, string, sleep
            case STATE_S:
                if (c == 't') { state = STATE_ST; APPEND_CHAR(c); }
                else if (c == 'l') { state = STATE_SL; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_ST:
                if (c == 'a') { state = STATE_STA; APPEND_CHAR(c); }
                else if (c == 'r') { state = STATE_STR; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_STA: BRANCH_OR_IDENTIFIER('t', STATE_STAT); break;
            case STATE_STAT:
                if (c == 'e') { state = STATE_STATE_KW; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_STATE_KW: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_STATE); break;
            case STATE_STR: BRANCH_OR_IDENTIFIER('i', STATE_STRI); break;
            case STATE_STRI: BRANCH_OR_IDENTIFIER('n', STATE_STRIN); break;
            case STATE_STRIN: BRANCH_OR_IDENTIFIER('g', STATE_STRING); break;
            case STATE_STRING: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_STRING); break;
            case STATE_SL: BRANCH_OR_IDENTIFIER('e', STATE_SLE); break;
            case STATE_SLE: BRANCH_OR_IDENTIFIER('e', STATE_SLEE); break;
            case STATE_SLEE: BRANCH_OR_IDENTIFIER('p', STATE_SLEEP); break;
            case STATE_SLEEP: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_SLEEP); break;

            // 'e' BRANCH: ensure, else
            case STATE_E:
                if (c == 'n') { state = STATE_EN; APPEND_CHAR(c); }
                else if (c == 'l') { state = STATE_EL; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_EN: BRANCH_OR_IDENTIFIER('s', STATE_ENS); break;
            case STATE_ENS: BRANCH_OR_IDENTIFIER('u', STATE_ENSU); break;
            case STATE_ENSU: BRANCH_OR_IDENTIFIER('r', STATE_ENSUR); break;
            case STATE_ENSUR: BRANCH_OR_IDENTIFIER('e', STATE_ENSURE); break;
            case STATE_ENSURE: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_ENSURE); break;
            case STATE_EL: BRANCH_OR_IDENTIFIER('s', STATE_ELS); break;
            case STATE_ELS: BRANCH_OR_IDENTIFIER('e', STATE_ELSE); break;
            case STATE_ELSE: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_ELSE); break;

            // 't' BRANCH: to, then, transition, true
            case STATE_T:
                if (c == 'o') { state = STATE_TO; APPEND_CHAR(c); }
                else if (c == 'h') { state = STATE_TH; APPEND_CHAR(c); }
                else if (c == 'r') { state = STATE_TR; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_TO: DISCARD_NOISE_OR_IDENTIFIER(); break;
            case STATE_TH: BRANCH_OR_IDENTIFIER('e', STATE_THE); break;
            case STATE_THE: BRANCH_OR_IDENTIFIER('n', STATE_THEN); break;
            case STATE_THEN: DISCARD_NOISE_OR_IDENTIFIER(); break;
            case STATE_TR:
                if (c == 'a') { state = STATE_TRA; APPEND_CHAR(c); }
                else if (c == 'u') { state = STATE_TRU; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_TRA: BRANCH_OR_IDENTIFIER('n', STATE_TRAN); break;
            case STATE_TRAN: BRANCH_OR_IDENTIFIER('s', STATE_TRANS); break;
            case STATE_TRANS: BRANCH_OR_IDENTIFIER('i', STATE_TRANSI); break;
            case STATE_TRANSI: BRANCH_OR_IDENTIFIER('t', STATE_TRANSIT); break;
            case STATE_TRANSIT: BRANCH_OR_IDENTIFIER('i', STATE_TRANSITI); break;
            case STATE_TRANSITI: BRANCH_OR_IDENTIFIER('o', STATE_TRANSITIO); break;
            case STATE_TRANSITIO: BRANCH_OR_IDENTIFIER('n', STATE_TRANSITION); break;
            case STATE_TRANSITION: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_TRANSITION); break;
            case STATE_TRU: BRANCH_OR_IDENTIFIER('e', STATE_TRUE); break;
            case STATE_TRUE: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_TRUE); break;

            // 'b' BRANCH: bool
            case STATE_B: BRANCH_OR_IDENTIFIER('o', STATE_BO); break;
            case STATE_BO: BRANCH_OR_IDENTIFIER('o', STATE_BOO); break;
            case STATE_BOO: BRANCH_OR_IDENTIFIER('l', STATE_BOOL); break;
            case STATE_BOOL: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_BOOL); break;

            // 'c' BRANCH: char
            case STATE_C: BRANCH_OR_IDENTIFIER('h', STATE_CH); break;
            case STATE_CH: BRANCH_OR_IDENTIFIER('a', STATE_CHA); break;
            case STATE_CHA: BRANCH_OR_IDENTIFIER('r', STATE_CHAR_KW); break;
            case STATE_CHAR_KW: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_CHAR); break;

            // 'f' BRANCH: float, false, fn, for
            case STATE_F:
                if (c == 'l') { state = STATE_FL; APPEND_CHAR(c); }
                else if (c == 'a') { state = STATE_FA; APPEND_CHAR(c); }
                else if (c == 'n') { state = STATE_FN; APPEND_CHAR(c); }
                else if (c == 'o') { state = STATE_FO; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_FL: BRANCH_OR_IDENTIFIER('o', STATE_FLO); break;
            case STATE_FLO: BRANCH_OR_IDENTIFIER('a', STATE_FLOA); break;
            case STATE_FLOA: BRANCH_OR_IDENTIFIER('t', STATE_FLOAT); break;
            case STATE_FLOAT: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_FLOAT); break;
            case STATE_FA: BRANCH_OR_IDENTIFIER('l', STATE_FAL); break;
            case STATE_FAL: BRANCH_OR_IDENTIFIER('s', STATE_FALS); break;
            case STATE_FALS: BRANCH_OR_IDENTIFIER('e', STATE_FALSE); break;
            case STATE_FALSE: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_FALSE); break;
            case STATE_FN: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_FN); break;
            case STATE_FO: BRANCH_OR_IDENTIFIER('r', STATE_FOR); break;
            case STATE_FOR: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_FOR); break;

            // 'n' BRANCH: null
            case STATE_N: BRANCH_OR_IDENTIFIER('u', STATE_NU); break;
            case STATE_NU: BRANCH_OR_IDENTIFIER('l', STATE_NUL); break;
            case STATE_NUL: BRANCH_OR_IDENTIFIER('l', STATE_NULL); break;
            case STATE_NULL: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_NULL); break;

            // 'r' BRANCH: react, return, readline, readint
            case STATE_R:
                if (c == 'e') { state = STATE_RE; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_RE:
                if (c == 'a') { state = STATE_REA; APPEND_CHAR(c); }
                else if (c == 't') { state = STATE_RET; APPEND_CHAR(c); }
                else if (c == 'd') { state = STATE_READ; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_REA:
                if (c == 'c') { state = STATE_REAC; APPEND_CHAR(c); }
                else if (c == 'd') { state = STATE_READ; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_REAC: BRANCH_OR_IDENTIFIER('t', STATE_REACT); break;
            case STATE_REACT: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_REACT); break;
            case STATE_RET: BRANCH_OR_IDENTIFIER('u', STATE_RETU); break;
            case STATE_RETU: BRANCH_OR_IDENTIFIER('r', STATE_RETUR); break;
            case STATE_RETUR: BRANCH_OR_IDENTIFIER('n', STATE_RETURN); break;
            case STATE_RETURN: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_RETURN); break;
            case STATE_READ:
                if (c == 'L') { state = STATE_READL; APPEND_CHAR(c); }
                else if (c == 'I') { state = STATE_READI; APPEND_CHAR(c); }
                else if (isalnum(c) || c == '_') { state = STATE_IN_IDENTIFIER; APPEND_CHAR(c); }
                else { ungetc(c, source); token.lexeme[lexemeIdx] = '\0'; token.type = TOKEN_IDENTIFIER; state = STATE_DONE; }
                break;
            case STATE_READL: BRANCH_OR_IDENTIFIER('i', STATE_READLI); break;
            case STATE_READLI: BRANCH_OR_IDENTIFIER('n', STATE_READLIN); break;
            case STATE_READLIN: BRANCH_OR_IDENTIFIER('e', STATE_READLINE); break;
            case STATE_READLINE: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_READLINE); break;
            case STATE_READI: BRANCH_OR_IDENTIFIER('n', STATE_READIN); break;
            case STATE_READIN: BRANCH_OR_IDENTIFIER('t', STATE_READINT); break;
            case STATE_READINT: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_READINT); break;

            // 'l' BRANCH: loop
            case STATE_L: BRANCH_OR_IDENTIFIER('o', STATE_LO); break;
            case STATE_LO: BRANCH_OR_IDENTIFIER('o', STATE_LOO); break;
            case STATE_LOO: BRANCH_OR_IDENTIFIER('p', STATE_LOOP); break;
            case STATE_LOOP: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_LOOP); break;

            // 'm' BRANCH: match
            case STATE_M: BRANCH_OR_IDENTIFIER('a', STATE_MA); break;
            case STATE_MA: BRANCH_OR_IDENTIFIER('t', STATE_MAT); break;
            case STATE_MAT: BRANCH_OR_IDENTIFIER('c', STATE_MATC); break;
            case STATE_MATC: BRANCH_OR_IDENTIFIER('h', STATE_MATCH); break;
            case STATE_MATCH: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_MATCH); break;

            // 'p' BRANCH: print
            case STATE_P: BRANCH_OR_IDENTIFIER('r', STATE_PR); break;
            case STATE_PR: BRANCH_OR_IDENTIFIER('i', STATE_PRI); break;
            case STATE_PRI: BRANCH_OR_IDENTIFIER('n', STATE_PRIN); break;
            case STATE_PRIN: BRANCH_OR_IDENTIFIER('t', STATE_PRINT); break;
            case STATE_PRINT: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_PRINT); break;

            // 'q' BRANCH: quit
            case STATE_Q: BRANCH_OR_IDENTIFIER('u', STATE_QU); break;
            case STATE_QU: BRANCH_OR_IDENTIFIER('i', STATE_QUI); break;
            case STATE_QUI: BRANCH_OR_IDENTIFIER('t', STATE_QUIT); break;
            case STATE_QUIT: YIELD_KEYWORD_OR_IDENTIFIER(TOKEN_QUIT); break;

            // ================================================================
            // IDENTIFIER FALLBACK
            // ================================================================
            case STATE_IN_IDENTIFIER:
                if (isalnum(c) || c == '_') {
                    if (lexemeIdx < 255) { APPEND_CHAR(c); }
                } else {
                    ungetc(c, source);
                    token.lexeme[lexemeIdx] = '\0';
                    token.type = (lexemeIdx > 32) ? TOKEN_ERROR : TOKEN_IDENTIFIER;
                    state = STATE_DONE;
                }
                break;

            case STATE_DONE:
                break;
        }
    }

    if (token.type == TOKEN_IDENTIFIER && strlen(token.lexeme) > 32) {
        token.type = TOKEN_ERROR;
    }

    return token;
}

// ============================================================================
// TOKEN TYPE NAME MAPPING
// ============================================================================

const char* getTokenTypeName(TokenType type) {
    switch (type) {
        case TOKEN_IDENTIFIER:      return "IDENTIFIER";
        case TOKEN_INT_LITERAL:     return "INT_LITERAL";
        case TOKEN_FLOAT_LITERAL:   return "FLOAT_LITERAL";
        case TOKEN_STRING_LITERAL:  return "STRING_LITERAL";
        case TOKEN_CHAR_LITERAL:    return "CHAR_LITERAL";

        case TOKEN_INT:             return "INT";
        case TOKEN_FLOAT:           return "FLOAT";
        case TOKEN_STRING:          return "STRING";
        case TOKEN_BOOL:            return "BOOL";
        case TOKEN_CHAR:            return "CHAR";

        case TOKEN_TRUE:            return "TRUE";
        case TOKEN_FALSE:           return "FALSE";
        case TOKEN_NULL:            return "NULL";

        case TOKEN_STATE:           return "STATE";
        case TOKEN_REACT:           return "REACT";
        case TOKEN_LOOP:            return "LOOP";

        case TOKEN_IF:              return "IF";
        case TOKEN_ELSE:            return "ELSE";
        case TOKEN_FOR:             return "FOR";
        case TOKEN_MATCH:           return "MATCH";
        case TOKEN_TRANSITION:      return "TRANSITION";
        case TOKEN_ENSURE:          return "ENSURE";

        case TOKEN_PRINT:           return "PRINT";
        case TOKEN_READLINE:        return "READLINE";
        case TOKEN_READINT:         return "READINT";

        case TOKEN_FN:              return "FN";
        case TOKEN_RETURN:          return "RETURN";

        case TOKEN_QUIT:            return "QUIT";
        case TOKEN_SLEEP:           return "SLEEP";

        case TOKEN_PLUS:            return "PLUS";
        case TOKEN_MINUS:           return "MINUS";
        case TOKEN_STAR:            return "STAR";
        case TOKEN_SLASH:           return "SLASH";
        case TOKEN_PERCENT:         return "PERCENT";
        case TOKEN_BACKSLASH:       return "BACKSLASH";
        case TOKEN_CARET:           return "CARET";

        case TOKEN_EQ:              return "EQ";
        case TOKEN_NE:              return "NE";
        case TOKEN_LT:              return "LT";
        case TOKEN_GT:              return "GT";
        case TOKEN_LE:              return "LE";
        case TOKEN_GE:              return "GE";

        case TOKEN_NOT:             return "NOT";
        case TOKEN_AND:             return "AND";
        case TOKEN_OR:              return "OR";

        case TOKEN_ASSIGN:          return "ASSIGN";
        case TOKEN_PLUS_ASSIGN:     return "PLUS_ASSIGN";
        case TOKEN_MINUS_ASSIGN:    return "MINUS_ASSIGN";
        case TOKEN_STAR_ASSIGN:     return "STAR_ASSIGN";
        case TOKEN_SLASH_ASSIGN:    return "SLASH_ASSIGN";
        case TOKEN_PERCENT_ASSIGN:  return "PERCENT_ASSIGN";

        case TOKEN_PIPELINE:        return "PIPELINE";
        case TOKEN_ARROW:           return "ARROW";
        case TOKEN_QUESTION:        return "QUESTION";

        case TOKEN_SEMICOLON:       return "SEMICOLON";
        case TOKEN_COMMA:           return "COMMA";
        case TOKEN_COLON:           return "COLON";

        case TOKEN_LPAREN:          return "LPAREN";
        case TOKEN_RPAREN:          return "RPAREN";
        case TOKEN_LBRACE:          return "LBRACE";
        case TOKEN_RBRACE:          return "RBRACE";
        case TOKEN_LBRACKET:        return "LBRACKET";
        case TOKEN_RBRACKET:        return "RBRACKET";

        case TOKEN_LINE_COMMENT:    return "LINE_COMMENT";
        case TOKEN_BLOCK_COMMENT:   return "BLOCK_COMMENT";
        case TOKEN_EOF:             return "EOF";
        case TOKEN_ERROR:           return "TOKEN_ERROR";
        default:                    return "UNKNOWN";
    }
}

