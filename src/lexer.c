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

    // 't' branch: to, transition, true
    STATE_T, STATE_TO,
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
#define GET_MAX_LEXEME_LENGTH ((int)sizeof(token.lexeme))

// Safely append one byte to token.lexeme and advance lexemeIdx.
// Inputs:
// - ch: character/byte to append
// Side effects:
// - Writes into token.lexeme when capacity remains
// - Increments lexemeIdx on successful append
// - Silently drops bytes after capacity is reached
#define CONSUME_CHAR(ch) \
    do { \
        if (lexemeIdx < GET_MAX_LEXEME_LENGTH - 1) { \
            token.lexeme[lexemeIdx++] = (char)(ch); \
        } \
    } while (0)

// Finalize the currently built lexeme as a specific token type.
// Side effects:
// - Null-terminates token.lexeme
// - Sets token.type
// - Ends the DFA loop by switching to STATE_DONE
#define ACCEPT_LEXEME(_token) \
    do { \
        token.lexeme[lexemeIdx] = '\0'; \
        token.type = _token; \
        state = STATE_DONE; \
    } while (0)

// Emit a lexer error token with a fixed error message.
// Side effects:
// - Copies _str into token.lexeme
// - Sets token type to TOKEN_ERROR
// - Ends scanning for the current token
#define ACCEPT_ERROR(_str) \
    do { \
        strcpy(token.lexeme, _str); \
        token.type = TOKEN_ERROR; \
        state = STATE_DONE; \
    } while (0)

// Identifier Recognizer - if it's alphanumeric, keep building an identifier 
// Otherwise, accept the token.
#define ACCEPT_TOKEN_OR_IDENTIFIER(_token_type) \
    if (isalnum(c) || c == '_') { \
        state = STATE_IN_IDENTIFIER; \
        CONSUME_CHAR(c); \
    } else { \
        ungetc(c, source); \
        ACCEPT_LEXEME(_token_type); \
    }

// Switch-case helper for keyword DFA transitions.
// On match, move to next_state and consume the matched character.
#define CASE_X_BRANCH_TO(expected_char, next_state) \
    case expected_char: \
        state = next_state; \
        CONSUME_CHAR(c); \
        break;

// Default switch-case helper for keyword DFA transitions.
// If no keyword branch matches, resolve to identifier continuation/finalization.
#define ELSE_CHECK_IF_POSSIBLE_IDENTIFIER() \
    default: \
        ACCEPT_TOKEN_OR_IDENTIFIER(TOKEN_IDENTIFIER) \
        break;

// Transition helper for keyword DFA paths.
// Behavior:
// - If c matches expected_char, move to next_state and keep building lexeme.
// - If c continues an identifier, fall back to STATE_IN_IDENTIFIER.
// - Otherwise, c terminates the token: push it back and emit IDENTIFIER.
// This avoids keyword text comparisons by deciding via state transitions.
#define CAN_WE_BRANCH_OR_IDENTIFIER(expected_char, next_state) \
    do { \
        switch (c) { \
            CASE_X_BRANCH_TO(expected_char, next_state); \
            ELSE_CHECK_IF_POSSIBLE_IDENTIFIER(); \
        } \
    } while(0)

// Finalize a candidate keyword state.
// Behavior:
// - If c is still identifier content, this is not a standalone keyword,
//   so continue as STATE_IN_IDENTIFIER.
// - Otherwise, push c back, terminate lexeme, and emit the keyword token.
#define CONFIRM_KEYWORD_OR_IDENTIFIER(token_type) \
    do { \
        ACCEPT_TOKEN_OR_IDENTIFIER(token_type) \
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
            case STATE_START: {
                if (isspace(c)) {
                    continue;
                } else if (isdigit(c)) { // ---- NUMBERS ----
                    state = STATE_IN_NUMBER;
                    CONSUME_CHAR(c);
                } else {
                    switch (c) {
                        case EOF:
                            ACCEPT_LEXEME(TOKEN_EOF);
                            break;
                        case ' ':
                        case '\t':
                        case '\n':
                            continue;
                            break;
                        // ---- COMMENTS ----
                        case '/':
                            state = STATE_IN_SLASH;
                            CONSUME_CHAR(c);
                            lexemeIdx = 0;  // Reset for comment content
                            break;
                        // ---- STRINGS ----
                        case '"':
                            state = STATE_IN_STRING;
                            CONSUME_CHAR(c);
                            break;
                        // ---- CHARACTERS ----
                        case '\'':
                            state = STATE_IN_CHAR;
                            CONSUME_CHAR(c);
                            break;
                        // ---- OPERATORS & MULTI-CHAR ----
                        case '+':
                            state = STATE_IN_PLUS;
                            CONSUME_CHAR(c);
                            break;
                        case '-':
                            state = STATE_IN_MINUS;
                            CONSUME_CHAR(c);
                            break;
                        case '*':
                            state = STATE_IN_STAR;
                            CONSUME_CHAR(c);
                            break;
                        case '%':
                            state = STATE_IN_PERCENT;
                            CONSUME_CHAR(c);
                            break;
                        case '|':
                            state = STATE_IN_PIPE;
                            CONSUME_CHAR(c);
                            break;
                        case '&':
                            state = STATE_IN_AMPERSAND;
                            CONSUME_CHAR(c);
                            break;
                        case '=':
                            state = STATE_IN_EQUALS;
                            CONSUME_CHAR(c);
                            break;
                        case '!':
                            state = STATE_IN_NOT;
                            CONSUME_CHAR(c);
                            break;
                        case '<':
                            state = STATE_IN_LT;
                            CONSUME_CHAR(c);
                            break;
                        case '>':
                            state = STATE_IN_GT;
                            CONSUME_CHAR(c);
                            break;
                        case '?':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_QUESTION);
                            break;
                        case '^':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_CARET);
                            break;
                        case '\\':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_BACKSLASH);
                            break;
                        // ---- DELIMITERS ----
                        case ';':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_SEMICOLON);
                            break;
                        case ',':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_COMMA);
                            break;
                        case ':': 
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_COLON);
                            break;
                        // ---- BRACKETS ----
                        case '(':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_LPAREN);
                            break;
                        case ')':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_RPAREN);
                            break;
                        case '{':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_LBRACE);
                            break;
                        case '}':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_RBRACE);
                            break;
                        case '[':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_LBRACKET);
                            break;
                        case ']':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_RBRACKET);
                            break;
                        // ---- KEYWORDS (letter entry points) ----
                        case 'i':
                            state = STATE_I;
                            CONSUME_CHAR(c);
                            break;
                        case 'd':
                            state = STATE_D;
                            CONSUME_CHAR(c);
                            break;
                        case 't':
                            state = STATE_T;
                            CONSUME_CHAR(c);
                            break;
                        case 's':
                            state = STATE_S;
                            CONSUME_CHAR(c);
                            break;
                        case 'e':
                            state = STATE_E;
                            CONSUME_CHAR(c);
                            break;
                        case 'b':
                            state = STATE_B;
                            CONSUME_CHAR(c);
                            break;
                        case 'c':
                            state = STATE_C;
                            CONSUME_CHAR(c);
                            break;
                        case 'f':
                            state = STATE_F;
                            CONSUME_CHAR(c);
                            break;
                        case 'n':
                            state = STATE_N;
                            CONSUME_CHAR(c);
                            break;
                        case 'r':
                            state = STATE_R;
                            CONSUME_CHAR(c);
                            break;
                        case 'l':
                            state = STATE_L;
                            CONSUME_CHAR(c);
                            break;
                        case 'm':
                            state = STATE_M;
                            CONSUME_CHAR(c);
                            break;
                        case 'p':
                            state = STATE_P;
                            CONSUME_CHAR(c);
                            break;
                        case 'q':
                            state = STATE_Q;
                            CONSUME_CHAR(c);
                            break;
                        
                        default:
                            if (isalpha(c) || c == '_') {
                                state = STATE_IN_IDENTIFIER;
                                CONSUME_CHAR(c);
                            } else {
                                CONSUME_CHAR(c);
                                ACCEPT_LEXEME(TOKEN_ERROR);
                            }
                        }
                    }
                break;
                }

            // ================================================================
            // COMMENTS
            // ================================================================
            case STATE_IN_SLASH:
                switch (c) {
                    case '/':
                        state = STATE_IN_LINE_COMMENT;
                        token.lexeme[0] = '/';
                        token.lexeme[1] = '/';
                        lexemeIdx = 2;
                        break;
                    case '*':
                        state = STATE_IN_BLOCK_COMMENT;
                        token.lexeme[0] = '/';
                        token.lexeme[1] = '*';
                        lexemeIdx = 2;
                        break;
                    case '=':
                        // lexemeIdx was reset to 0 for comment content, so rebuild
                        // the whole operator instead of appending to it.
                        token.type = TOKEN_SLASH_ASSIGN;
                        token.lexeme[0] = '/';
                        token.lexeme[1] = '=';
                        token.lexeme[2] = '\0';
                        lexemeIdx = 0;
                        state = STATE_DONE;
                        break;
                    default:
                        ungetc(c, source);
                        token.type = TOKEN_SLASH;
                        token.lexeme[1] = '\0';
                        state = STATE_DONE;
                }
                break;

            case STATE_IN_LINE_COMMENT:
                switch (c) {
                    case EOF:
                    case '\n':
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_LINE_COMMENT);
                        break;
                    default:
                        CONSUME_CHAR(c);
                }
                break;

            case STATE_IN_BLOCK_COMMENT:
                switch (c) {
                    case '*':
                        state = STATE_IN_BLOCK_COMMENT_STAR;
                        CONSUME_CHAR(c);
                        break;
                    case EOF:
                        ACCEPT_LEXEME(TOKEN_ERROR); // Unterminated block comment
                        break;
                    default:
                        CONSUME_CHAR(c);
                }
                break;

            case STATE_IN_BLOCK_COMMENT_STAR:
                switch (c) {
                    case '/':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_BLOCK_COMMENT);
                        break;
                    case '*':
                        state = STATE_IN_BLOCK_COMMENT_STAR;
                        CONSUME_CHAR(c);
                        break;
                    case EOF:
                        strcpy(token.lexeme, "Unterminated block comment");
                        ACCEPT_LEXEME(TOKEN_ERROR);
                        break;
                    default:
                        state = STATE_IN_BLOCK_COMMENT;
                        CONSUME_CHAR(c);
                }
                break;

            // ================================================================
            // NUMBER & FLOAT PARSING
            // ================================================================
            case STATE_IN_NUMBER:
                if (isdigit(c)) {
                    CONSUME_CHAR(c);
                } else if (c == '.') {
                    state = STATE_IN_FLOAT;
                    CONSUME_CHAR(c);
                } else if (isalpha(c) || c == '_') {
                    CONSUME_CHAR(c);
                    while ((c = fgetc(source)) != EOF && (isalnum(c) || c == '_')) {
                        if (lexemeIdx < (int)sizeof(token.lexeme) - 1) {
                            CONSUME_CHAR(c);
                        }
                    }
                    if (c != EOF) ungetc(c, source);
                    ACCEPT_LEXEME(TOKEN_ERROR);
                } else {
                    ungetc(c, source);
                    ACCEPT_LEXEME(TOKEN_INT_LITERAL);
                }
                break;

            case STATE_IN_FLOAT:
                if (isdigit(c)) {
                    CONSUME_CHAR(c);
                } else if (isalpha(c) || c == '_') {
                    CONSUME_CHAR(c);
                    while ((c = fgetc(source)) != EOF && (isalnum(c) || c == '_')) {
                        if (lexemeIdx < (int)sizeof(token.lexeme) - 1) {
                            CONSUME_CHAR(c);
                        }
                    }
                    if (c != EOF) ungetc(c, source);
                    ACCEPT_LEXEME(TOKEN_ERROR);
                } else {
                    ungetc(c, source);
                    ACCEPT_LEXEME(TOKEN_FLOAT_LITERAL);
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
                    CONSUME_CHAR(c);
                    break;
                }
                
                switch (c) {
                    case '\\':
                        escaped = 1;
                        CONSUME_CHAR(c);
                        break;
                    case '"':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_STRING_LITERAL);
                        break;
                    case EOF:
                    case '\n':
                    case '\r':
                        ACCEPT_LEXEME(TOKEN_ERROR);
                        break;
                    default:
                        CONSUME_CHAR(c);
                }
                break;

            // ================================================================
            // CHARACTER PARSING
            // ================================================================
            case STATE_IN_CHAR:
                if (lexemeIdx - 1 >= 2) {
                    ACCEPT_ERROR("Char literal too long");
                    break;
                }

                switch (c) {
                    case '\'': {
                        // Content sits between the opening quote at index 0 and the
                        // closing quote about to be appended.
                        int contentLen = lexemeIdx - 1;
                        if (contentLen == 1 || (contentLen == 2 && token.lexeme[1] == '\\')) {
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_CHAR_LITERAL);
                        } else {
                            ACCEPT_ERROR((contentLen == 0) ? "Empty char literal" : "Char literal too long");
                        }
                        break;
                    }
                    case EOF:
                    case '\n':
                    case '\r':
                        ACCEPT_ERROR("Unterminated char");
                        break;
                    default: 
                        CONSUME_CHAR(c);
                }
                break;

            // ================================================================
            // OPERATOR BRANCHING
            // ================================================================
            case STATE_IN_PLUS:
                switch (c) {
                    case '=':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_PLUS_ASSIGN);
                        break;
                    default:
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_PLUS);
                        break;
                }
                break;

            case STATE_IN_MINUS:
                switch (c) {
                    case '>':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_ARROW);
                        break;
                    case '=':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_MINUS_ASSIGN);
                        break;
                    default:
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_MINUS);
                }
                break;

            case STATE_IN_STAR:
                switch (c) {
                    case '=':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_STAR_ASSIGN);
                        break;
                    default:
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_STAR);
                }
                break;

            case STATE_IN_PERCENT:
                switch (c) {
                    case '=':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_PERCENT_ASSIGN);
                        break;
                    default:
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_PERCENT);
                }
                break;

            case STATE_IN_PIPE:
                switch (c) {
                    case '>':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_PIPELINE);
                        break;
                    case '|':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_OR);
                        break;
                    default:
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_ERROR);
                }
                break;

            case STATE_IN_AMPERSAND:
                switch (c) {
                    case '&':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_AND);
                        break;
                    default:
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_ERROR);
                        break;
                }
                break;

            case STATE_IN_EQUALS:
                switch (c) {
                    case '=':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_EQ);
                        break;
                    default:
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_ASSIGN);
                        break;
                }
                break;

            case STATE_IN_NOT:
                switch (c) {
                    case '=':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_NE);
                        break;
                    default:
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_NOT);
                        break;
                }
                break;

            case STATE_IN_LT:
                switch (c) {
                    case '=':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_LE);
                        break;
                    default:
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_LT);
                        break;
                }
                break;

            case STATE_IN_GT:
                switch (c) {
                    case '=':
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_GE);
                        break;
                    default:
                        ungetc(c, source);
                        ACCEPT_LEXEME(TOKEN_GT);
                        break;
                }
                break;

            // ================================================================
            // KEYWORD DFA BRANCHES
            // ================================================================

            // 'i' BRANCH: is, int, if
            case STATE_I:
                switch (c) {
                    CASE_X_BRANCH_TO('s', STATE_IS);
                    CASE_X_BRANCH_TO('n', STATE_IN_KW);
                    CASE_X_BRANCH_TO('f', STATE_IF);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_IS: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_NOISE_WORD); break;
            case STATE_IN_KW: CAN_WE_BRANCH_OR_IDENTIFIER('t', STATE_INT); break;
            case STATE_INT: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_INT); break;
            case STATE_IF: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_IF); break;

            // 'd' BRANCH: do
            case STATE_D: CAN_WE_BRANCH_OR_IDENTIFIER('o', STATE_DO); break;
            case STATE_DO: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_NOISE_WORD); break;

            // 's' BRANCH: state, string, sleep
            case STATE_S:
                switch (c) {
                    CASE_X_BRANCH_TO('t', STATE_ST);
                    CASE_X_BRANCH_TO('l', STATE_SL);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_ST:
                switch (c) {
                    CASE_X_BRANCH_TO('a', STATE_STA);
                    CASE_X_BRANCH_TO('r', STATE_STR);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_STA: CAN_WE_BRANCH_OR_IDENTIFIER('t', STATE_STAT); break;
            case STATE_STAT: CAN_WE_BRANCH_OR_IDENTIFIER('e', STATE_STATE_KW); break;
            case STATE_STATE_KW: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_STATE); break;
            case STATE_STR: CAN_WE_BRANCH_OR_IDENTIFIER('i', STATE_STRI); break;
            case STATE_STRI: CAN_WE_BRANCH_OR_IDENTIFIER('n', STATE_STRIN); break;
            case STATE_STRIN: CAN_WE_BRANCH_OR_IDENTIFIER('g', STATE_STRING); break;
            case STATE_STRING: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_STRING); break;
            case STATE_SL: CAN_WE_BRANCH_OR_IDENTIFIER('e', STATE_SLE); break;
            case STATE_SLE: CAN_WE_BRANCH_OR_IDENTIFIER('e', STATE_SLEE); break;
            case STATE_SLEE: CAN_WE_BRANCH_OR_IDENTIFIER('p', STATE_SLEEP); break;
            case STATE_SLEEP: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_SLEEP); break;

            // 'e' BRANCH: ensure, else
            case STATE_E:
                switch (c) {
                    CASE_X_BRANCH_TO('n', STATE_EN);
                    CASE_X_BRANCH_TO('l', STATE_EL);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_EN: CAN_WE_BRANCH_OR_IDENTIFIER('s', STATE_ENS); break;
            case STATE_ENS: CAN_WE_BRANCH_OR_IDENTIFIER('u', STATE_ENSU); break;
            case STATE_ENSU: CAN_WE_BRANCH_OR_IDENTIFIER('r', STATE_ENSUR); break;
            case STATE_ENSUR: CAN_WE_BRANCH_OR_IDENTIFIER('e', STATE_ENSURE); break;
            case STATE_ENSURE: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_ENSURE); break;
            case STATE_EL: CAN_WE_BRANCH_OR_IDENTIFIER('s', STATE_ELS); break;
            case STATE_ELS: CAN_WE_BRANCH_OR_IDENTIFIER('e', STATE_ELSE); break;
            case STATE_ELSE: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_ELSE); break;

            // 't' BRANCH: to, transition, true
            case STATE_T:
                switch (c) {
                    CASE_X_BRANCH_TO('o', STATE_TO);
                    CASE_X_BRANCH_TO('r', STATE_TR);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_TO: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_NOISE_WORD); break;
            case STATE_TR:
                switch (c) {
                    CASE_X_BRANCH_TO('a', STATE_TRA);
                    CASE_X_BRANCH_TO('u', STATE_TRU);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_TRA: CAN_WE_BRANCH_OR_IDENTIFIER('n', STATE_TRAN); break;
            case STATE_TRAN: CAN_WE_BRANCH_OR_IDENTIFIER('s', STATE_TRANS); break;
            case STATE_TRANS: CAN_WE_BRANCH_OR_IDENTIFIER('i', STATE_TRANSI); break;
            case STATE_TRANSI: CAN_WE_BRANCH_OR_IDENTIFIER('t', STATE_TRANSIT); break;
            case STATE_TRANSIT: CAN_WE_BRANCH_OR_IDENTIFIER('i', STATE_TRANSITI); break;
            case STATE_TRANSITI: CAN_WE_BRANCH_OR_IDENTIFIER('o', STATE_TRANSITIO); break;
            case STATE_TRANSITIO: CAN_WE_BRANCH_OR_IDENTIFIER('n', STATE_TRANSITION); break;
            case STATE_TRANSITION: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_TRANSITION); break;
            case STATE_TRU: CAN_WE_BRANCH_OR_IDENTIFIER('e', STATE_TRUE); break;
            case STATE_TRUE: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_TRUE); break;

            // 'b' BRANCH: bool
            case STATE_B: CAN_WE_BRANCH_OR_IDENTIFIER('o', STATE_BO); break;
            case STATE_BO: CAN_WE_BRANCH_OR_IDENTIFIER('o', STATE_BOO); break;
            case STATE_BOO: CAN_WE_BRANCH_OR_IDENTIFIER('l', STATE_BOOL); break;
            case STATE_BOOL: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_BOOL); break;

            // 'c' BRANCH: char
            case STATE_C: CAN_WE_BRANCH_OR_IDENTIFIER('h', STATE_CH); break;
            case STATE_CH: CAN_WE_BRANCH_OR_IDENTIFIER('a', STATE_CHA); break;
            case STATE_CHA: CAN_WE_BRANCH_OR_IDENTIFIER('r', STATE_CHAR_KW); break;
            case STATE_CHAR_KW: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_CHAR); break;

            // 'f' BRANCH: float, false, fn, for
            case STATE_F:
                switch (c) {
                    CASE_X_BRANCH_TO('l', STATE_FL);
                    CASE_X_BRANCH_TO('a', STATE_FA);
                    CASE_X_BRANCH_TO('n', STATE_FN);
                    CASE_X_BRANCH_TO('o', STATE_FO);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_FL: CAN_WE_BRANCH_OR_IDENTIFIER('o', STATE_FLO); break;
            case STATE_FLO: CAN_WE_BRANCH_OR_IDENTIFIER('a', STATE_FLOA); break;
            case STATE_FLOA: CAN_WE_BRANCH_OR_IDENTIFIER('t', STATE_FLOAT); break;
            case STATE_FLOAT: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_FLOAT); break;
            case STATE_FA: CAN_WE_BRANCH_OR_IDENTIFIER('l', STATE_FAL); break;
            case STATE_FAL: CAN_WE_BRANCH_OR_IDENTIFIER('s', STATE_FALS); break;
            case STATE_FALS: CAN_WE_BRANCH_OR_IDENTIFIER('e', STATE_FALSE); break;
            case STATE_FALSE: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_FALSE); break;
            case STATE_FN: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_FN); break;
            case STATE_FO: CAN_WE_BRANCH_OR_IDENTIFIER('r', STATE_FOR); break;
            case STATE_FOR: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_FOR); break;

            // 'n' BRANCH: null
            case STATE_N: CAN_WE_BRANCH_OR_IDENTIFIER('u', STATE_NU); break;
            case STATE_NU: CAN_WE_BRANCH_OR_IDENTIFIER('l', STATE_NUL); break;
            case STATE_NUL: CAN_WE_BRANCH_OR_IDENTIFIER('l', STATE_NULL); break;
            case STATE_NULL: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_NULL); break;

            // 'r' BRANCH: react, return, readline, readint
            case STATE_R:
                switch (c) {
                    CASE_X_BRANCH_TO('e', STATE_RE);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_RE:
                switch (c) {
                    CASE_X_BRANCH_TO('a', STATE_REA);
                    CASE_X_BRANCH_TO('t', STATE_RET);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_REA:
                switch (c) {
                    CASE_X_BRANCH_TO('c', STATE_REAC);
                    CASE_X_BRANCH_TO('d', STATE_READ);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_REAC: CAN_WE_BRANCH_OR_IDENTIFIER('t', STATE_REACT); break;
            case STATE_REACT: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_REACT); break;
            case STATE_RET: CAN_WE_BRANCH_OR_IDENTIFIER('u', STATE_RETU); break;
            case STATE_RETU: CAN_WE_BRANCH_OR_IDENTIFIER('r', STATE_RETUR); break;
            case STATE_RETUR: CAN_WE_BRANCH_OR_IDENTIFIER('n', STATE_RETURN); break;
            case STATE_RETURN: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_RETURN); break;
            case STATE_READ:
                switch (c) {
                    CASE_X_BRANCH_TO('L', STATE_READL);
                    CASE_X_BRANCH_TO('I', STATE_READI);
                    ELSE_CHECK_IF_POSSIBLE_IDENTIFIER();
                }
                break;
            case STATE_READL: CAN_WE_BRANCH_OR_IDENTIFIER('i', STATE_READLI); break;
            case STATE_READLI: CAN_WE_BRANCH_OR_IDENTIFIER('n', STATE_READLIN); break;
            case STATE_READLIN: CAN_WE_BRANCH_OR_IDENTIFIER('e', STATE_READLINE); break;
            case STATE_READLINE: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_READLINE); break;
            case STATE_READI: CAN_WE_BRANCH_OR_IDENTIFIER('n', STATE_READIN); break;
            case STATE_READIN: CAN_WE_BRANCH_OR_IDENTIFIER('t', STATE_READINT); break;
            case STATE_READINT: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_READINT); break;

            // 'l' BRANCH: loop
            case STATE_L: CAN_WE_BRANCH_OR_IDENTIFIER('o', STATE_LO); break;
            case STATE_LO: CAN_WE_BRANCH_OR_IDENTIFIER('o', STATE_LOO); break;
            case STATE_LOO: CAN_WE_BRANCH_OR_IDENTIFIER('p', STATE_LOOP); break;
            case STATE_LOOP: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_LOOP); break;

            // 'm' BRANCH: match
            case STATE_M: CAN_WE_BRANCH_OR_IDENTIFIER('a', STATE_MA); break;
            case STATE_MA: CAN_WE_BRANCH_OR_IDENTIFIER('t', STATE_MAT); break;
            case STATE_MAT: CAN_WE_BRANCH_OR_IDENTIFIER('c', STATE_MATC); break;
            case STATE_MATC: CAN_WE_BRANCH_OR_IDENTIFIER('h', STATE_MATCH); break;
            case STATE_MATCH: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_MATCH); break;

            // 'p' BRANCH: print
            case STATE_P: CAN_WE_BRANCH_OR_IDENTIFIER('r', STATE_PR); break;
            case STATE_PR: CAN_WE_BRANCH_OR_IDENTIFIER('i', STATE_PRI); break;
            case STATE_PRI: CAN_WE_BRANCH_OR_IDENTIFIER('n', STATE_PRIN); break;
            case STATE_PRIN: CAN_WE_BRANCH_OR_IDENTIFIER('t', STATE_PRINT); break;
            case STATE_PRINT: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_PRINT); break;

            // 'q' BRANCH: quit
            case STATE_Q: CAN_WE_BRANCH_OR_IDENTIFIER('u', STATE_QU); break;
            case STATE_QU: CAN_WE_BRANCH_OR_IDENTIFIER('i', STATE_QUI); break;
            case STATE_QUI: CAN_WE_BRANCH_OR_IDENTIFIER('t', STATE_QUIT); break;
            case STATE_QUIT: CONFIRM_KEYWORD_OR_IDENTIFIER(TOKEN_QUIT); break;

            // ================================================================
            // IDENTIFIER FALLBACK
            // ================================================================
            case STATE_IN_IDENTIFIER:
                if (isalnum(c) || c == '_') {
                    if (lexemeIdx < 255) { CONSUME_CHAR(c); }
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
        case TOKEN_NOISE_WORD:      return "NOISE_WORD";
        case TOKEN_EOF:             return "EOF";
        case TOKEN_ERROR:           return "TOKEN_ERROR";
        default:                    return "UNKNOWN";
    }
}
