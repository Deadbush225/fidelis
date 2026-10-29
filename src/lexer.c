#include "lexer.h"
#include "helper.h"
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
    STATE_IN_FLOAT_DOT,
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
            token.lexeme[lexemeIdx] = '\0'; /* Null-terminate after each append */\
            printf("Consumed %-2s to lexeme. Current lexeme is \e[0;35m%-40s\e[0m | Current token: \e[0;35m%s\e[0m\n", escapeCharacter((char)(ch)), escapedLexeme(token.lexeme), getStateName(state)); \
            fprintf(transition, "Consumed %-2s to lexeme. Current lexeme is %-40s | Current token: %s \n", escapeCharacter((char)(ch)), escapedLexeme(token.lexeme) , getStateName(state)); \
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
        if (strncmp(getTokenTypeName(token.type), "ERR_", 4) == 0) { \
            printf("\e[0;31mRejected Lexeme:\e[0m %s | \e[0;31mtoken type:\e[0m %s\n", escapedLexeme(token.lexeme), getTokenTypeName(token.type)); \
            fprintf(transition, "Rejected Lexeme: %s | token type: %s\n", token.lexeme, getTokenTypeName(token.type)); \
        } else { \
            printf("\e[0;32mAccepted Lexeme:\e[0m %s | \e[0;32mtoken type:\e[0m %s\n", escapedLexeme(token.lexeme), getTokenTypeName(token.type)); \
            fprintf(transition, "Accepted Lexeme: %s | token type: %s\n", token.lexeme, getTokenTypeName(token.type)); \
        } \
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
// ============================================================================
// DFA STATE NAME MAPPING & TRANSITION TRACKING
// ============================================================================

/*
getStateName - Maps a LexerState enum value to its corresponding string identifier.
               Used to generate human-readable DFA transition traces.

Parameters:
    LexerState state - The DFA state to convert.

Returns:
    const char* - The name of the state (e.g. "STATE_START", "STATE_INT").
*/
static const char* getStateName(LexerState state) {
    switch (state) {
        case STATE_START:                   return "STATE_START";

        case STATE_IN_SLASH:                return "STATE_IN_SLASH";
        case STATE_IN_LINE_COMMENT:         return "STATE_IN_LINE_COMMENT";
        case STATE_IN_BLOCK_COMMENT:        return "STATE_IN_BLOCK_COMMENT";
        case STATE_IN_BLOCK_COMMENT_STAR:   return "STATE_IN_BLOCK_COMMENT_STAR";

        case STATE_IN_NUMBER:               return "STATE_IN_NUMBER";
        case STATE_IN_FLOAT:                return "STATE_IN_FLOAT";

        case STATE_IN_STRING:               return "STATE_IN_STRING";
        case STATE_IN_CHAR:                 return "STATE_IN_CHAR";

        case STATE_IN_MINUS:                return "STATE_IN_MINUS";
        case STATE_IN_PLUS:                 return "STATE_IN_PLUS";
        case STATE_IN_STAR:                 return "STATE_IN_STAR";
        case STATE_IN_PERCENT:              return "STATE_IN_PERCENT";
        case STATE_IN_PIPE:                 return "STATE_IN_PIPE";
        case STATE_IN_AMPERSAND:            return "STATE_IN_AMPERSAND";
        case STATE_IN_EQUALS:               return "STATE_IN_EQUALS";
        case STATE_IN_NOT:                  return "STATE_IN_NOT";
        case STATE_IN_LT:                   return "STATE_IN_LT";
        case STATE_IN_GT:                   return "STATE_IN_GT";

        case STATE_I:                       return "STATE_I";
        case STATE_IS:                      return "STATE_IS";
        case STATE_IN_KW:                   return "STATE_IN_KW";
        case STATE_INT:                     return "STATE_INT";
        case STATE_IF:                      return "STATE_IF";

        case STATE_D:                       return "STATE_D";
        case STATE_DO:                      return "STATE_DO";

        case STATE_T:                       return "STATE_T";
        case STATE_TO:                      return "STATE_TO";
        case STATE_TR:                      return "STATE_TR";
        case STATE_TRA:                     return "STATE_TRA";
        case STATE_TRAN:                    return "STATE_TRAN";
        case STATE_TRANS:                   return "STATE_TRANS";
        case STATE_TRANSI:                  return "STATE_TRANSI";
        case STATE_TRANSIT:                 return "STATE_TRANSIT";
        case STATE_TRANSITI:                return "STATE_TRANSITI";
        case STATE_TRANSITIO:               return "STATE_TRANSITIO";
        case STATE_TRANSITION:              return "STATE_TRANSITION";
        case STATE_TRU:                     return "STATE_TRU";
        case STATE_TRUE:                    return "STATE_TRUE";

        case STATE_S:                       return "STATE_S";
        case STATE_ST:                      return "STATE_ST";
        case STATE_STA:                     return "STATE_STA";
        case STATE_STAT:                    return "STATE_STAT";
        case STATE_STATE_KW:                return "STATE_STATE_KW";
        case STATE_STR:                     return "STATE_STR";
        case STATE_STRI:                    return "STATE_STRI";
        case STATE_STRIN:                   return "STATE_STRIN";
        case STATE_STRING:                  return "STATE_STRING";
        case STATE_SL:                      return "STATE_SL";
        case STATE_SLE:                     return "STATE_SLE";
        case STATE_SLEE:                    return "STATE_SLEE";
        case STATE_SLEEP:                   return "STATE_SLEEP";

        case STATE_E:                       return "STATE_E";
        case STATE_EN:                      return "STATE_EN";
        case STATE_ENS:                     return "STATE_ENS";
        case STATE_ENSU:                    return "STATE_ENSU";
        case STATE_ENSUR:                   return "STATE_ENSUR";
        case STATE_ENSURE:                  return "STATE_ENSURE";
        case STATE_EL:                      return "STATE_EL";
        case STATE_ELS:                     return "STATE_ELS";
        case STATE_ELSE:                    return "STATE_ELSE";

        case STATE_B:                       return "STATE_B";
        case STATE_BO:                      return "STATE_BO";
        case STATE_BOO:                     return "STATE_BOO";
        case STATE_BOOL:                    return "STATE_BOOL";

        case STATE_C:                       return "STATE_C";
        case STATE_CH:                      return "STATE_CH";
        case STATE_CHA:                     return "STATE_CHA";
        case STATE_CHAR_KW:                 return "STATE_CHAR_KW";

        case STATE_F:                       return "STATE_F";
        case STATE_FL:                      return "STATE_FL";
        case STATE_FLO:                     return "STATE_FLO";
        case STATE_FLOA:                    return "STATE_FLOA";
        case STATE_FLOAT:                   return "STATE_FLOAT";
        case STATE_FA:                      return "STATE_FA";
        case STATE_FAL:                     return "STATE_FAL";
        case STATE_FALS:                    return "STATE_FALS";
        case STATE_FALSE:                   return "STATE_FALSE";
        case STATE_FN:                      return "STATE_FN";
        case STATE_FO:                      return "STATE_FO";
        case STATE_FOR:                     return "STATE_FOR";

        case STATE_N:                       return "STATE_N";
        case STATE_NU:                      return "STATE_NU";
        case STATE_NUL:                     return "STATE_NUL";
        case STATE_NULL:                    return "STATE_NULL";

        case STATE_R:                       return "STATE_R";
        case STATE_RE:                      return "STATE_RE";
        case STATE_REA:                     return "STATE_REA";
        case STATE_REAC:                    return "STATE_REAC";
        case STATE_REACT:                   return "STATE_REACT";
        case STATE_RET:                     return "STATE_RET";
        case STATE_RETU:                    return "STATE_RETU";
        case STATE_RETUR:                   return "STATE_RETUR";
        case STATE_RETURN:                  return "STATE_RETURN";
        case STATE_READ:                    return "STATE_READ";
        case STATE_READL:                   return "STATE_READL";
        case STATE_READLI:                  return "STATE_READLI";
        case STATE_READLIN:                 return "STATE_READLIN";
        case STATE_READLINE:                return "STATE_READLINE";
        case STATE_READI:                   return "STATE_READI";
        case STATE_READIN:                  return "STATE_READIN";
        case STATE_READINT:                 return "STATE_READINT";

        case STATE_L:                       return "STATE_L";
        case STATE_LO:                      return "STATE_LO";
        case STATE_LOO:                     return "STATE_LOO";
        case STATE_LOOP:                    return "STATE_LOOP";

        case STATE_M:                       return "STATE_M";
        case STATE_MA:                      return "STATE_MA";
        case STATE_MAT:                     return "STATE_MAT";
        case STATE_MATC:                    return "STATE_MATC";
        case STATE_MATCH:                   return "STATE_MATCH";

        case STATE_P:                       return "STATE_P";
        case STATE_PR:                      return "STATE_PR";
        case STATE_PRI:                     return "STATE_PRI";
        case STATE_PRIN:                    return "STATE_PRIN";
        case STATE_PRINT:                   return "STATE_PRINT";

        case STATE_Q:                       return "STATE_Q";
        case STATE_QU:                      return "STATE_QU";
        case STATE_QUI:                     return "STATE_QUI";
        case STATE_QUIT:                    return "STATE_QUIT";

        case STATE_IN_IDENTIFIER:           return "STATE_IN_IDENTIFIER";
        case STATE_DONE:                    return "STATE_DONE";
        default:                            return "STATE_UNKNOWN";
    }
}

/*
appendTransition - Appends a state transition to the token's transition trace string.
                   Transitions are formatted as: "STATE_A -> STATE_B -> ...".

Parameters:
    Token *token     - Pointer to the token receiving the transition.
    LexerState state - The newly reached DFA state to append.
*/
static void appendTransition(Token *token, LexerState state) {
    const char *name = getStateName(state);
    size_t currentLen = strlen(token->transitions);
    size_t nameLen = strlen(name);
    const char *sep = (currentLen > 0) ? " -> " : "";
    size_t sepLen = strlen(sep);

    if (currentLen + sepLen + nameLen < sizeof(token->transitions)) {
        strcat(token->transitions, sep);
        strcat(token->transitions, name);
    }
}

// ============================================================================
// TOKEN GENERATION FUNCTIONS
// ============================================================================

Token getNextToken(FILE *source, FILE *transition) {
    Token token;
    token.lexeme[0] = '\0';
    int lexemeIdx = 0;
    int escaped = 0;
    token.transitions[0] = '\0'; // Initialize empty DFA state transition trace
    LexerState state = STATE_START;
    int c;

    printf("=================================================== Scanning token... =================================================\n");
    fprintf(transition, "=================================================== Scanning token... =================================================\n");
    
    while (state != STATE_DONE) {
        c = fgetc(source);
        
        // Skip leading whitespace between tokens without recording a state transition
        if (state == STATE_START && isspace(c)) {
            continue;
        }

        // Record the initial STATE_START once the first token byte is encountered
        // if (token.transitions[0] == '\0') {
        //     printf("Consumed \e[0;35m  \e[0m to lexeme. Current lexeme is \e[0;35m%-40s\e[0m | Current token: \e[0;35mSTATE_START\e[0m\n", "");
        //     fprintf(transition, "Consumed    to lexeme. Current lexeme is %-40s | Current token: STATE_START\n", "");
        //     appendTransition(&token, STATE_START);
        // }

        LexerState prevState = state; // Track current state to detect transitions after switch

        switch (state) {
            // ================================================================
            // START STATE: Initial character dispatch
            // ================================================================
            case STATE_START: {
                appendTransition(&token, STATE_START);
                printf("Started scanning token. Current lexeme is \e[0;35m%-40s\e[0m | Current token: \e[0;35mSTATE_START\e[0m\n", "");
                fprintf(transition, "Started scanning token. Current lexeme is %-40s | Current token: STATE_START\n", "");

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
                        case '.':
                            CONSUME_CHAR(c);
                            ACCEPT_LEXEME(TOKEN_DOT);
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
                                ACCEPT_LEXEME(TOKEN_ERR_UNRECOGNIZED);
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
                        // Restore the initial '/' that was cleared, then consume the new '/'
                        token.lexeme[0] = '/';
                        lexemeIdx = 1;
                        CONSUME_CHAR(c);
                        break;
                    case '*':
                        state = STATE_IN_BLOCK_COMMENT;
                        // Restore the initial '/' that was cleared, then consume the '*'
                        token.lexeme[0] = '/';
                        lexemeIdx = 1;
                        CONSUME_CHAR(c);
                        break;
                    case '=':
                        // Restore the initial '/' that was cleared, then consume the '='
                        token.lexeme[0] = '/';
                        lexemeIdx = 1;
                        CONSUME_CHAR(c);
                        ACCEPT_LEXEME(TOKEN_SLASH_ASSIGN);
                        break;
                    default:
                        ungetc(c, source);
                        // Restore the initial '/' that was cleared, then accept
                        token.lexeme[0] = '/';
                        lexemeIdx = 1;
                        ACCEPT_LEXEME(TOKEN_SLASH);
                        break;
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
                        ACCEPT_LEXEME(TOKEN_ERR_UNTERMINATED_BLOCK_COMMENT);
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
                        ACCEPT_LEXEME(TOKEN_ERR_UNTERMINATED_BLOCK_COMMENT);
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
                    state = STATE_IN_FLOAT_DOT;
                    CONSUME_CHAR(c);
                } else if (isalpha(c) || c == '_') {
                    CONSUME_CHAR(c);
                    while ((c = fgetc(source)) != EOF && (isalnum(c) || c == '_')) {
                        if (lexemeIdx < (int)sizeof(token.lexeme) - 1) {
                            CONSUME_CHAR(c);
                        }
                    }
                    if (c != EOF) ungetc(c, source);
                    ACCEPT_LEXEME(TOKEN_ERR_INVALID_NUMBER);
                } else {
                    ungetc(c, source);
                    ACCEPT_LEXEME(TOKEN_INT_LITERAL);
                }
                break;

            case STATE_IN_FLOAT_DOT:
                if (isdigit(c)) {
                    state = STATE_IN_FLOAT;
                    CONSUME_CHAR(c);
                } else {
                    // Trailing dot without digits (e.g., "42.") is malformed
                    ungetc(c, source);
                    ACCEPT_LEXEME(TOKEN_ERR_MALFORMED_FLOAT);
                }
                break;

            case STATE_IN_FLOAT:
                if (isdigit(c)) {
                    CONSUME_CHAR(c);
                } else if (isalpha(c) || c == '_' || c == '.') {
                    // Multiple dots (e.g., "1.2.3") or letters (e.g., "1.2abc")
                    CONSUME_CHAR(c);
                    while ((c = fgetc(source)) != EOF && (isalnum(c) || c == '_' || c == '.')) {
                        if (lexemeIdx < (int)sizeof(token.lexeme) - 1) {
                            CONSUME_CHAR(c);
                        }
                    }
                    if (c != EOF) ungetc(c, source);
                    ACCEPT_LEXEME(TOKEN_ERR_INVALID_NUMBER);
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
                        ACCEPT_LEXEME(TOKEN_ERR_UNTERMINATED_STRING);
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
                    while (c != '\'' && c != '\n' && c != '\r' && c != EOF) {
                        c = fgetc(source);
                    }
                    ACCEPT_LEXEME(TOKEN_ERR_CHAR_TOO_LONG);
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
                            ACCEPT_LEXEME((contentLen == 0) ? TOKEN_ERR_EMPTY_CHAR : TOKEN_ERR_CHAR_TOO_LONG);

                        }
                        break;
                    }
                    case EOF:
                    case '\n':
                    case '\r':
                        ACCEPT_LEXEME(TOKEN_ERR_UNTERMINATED_CHAR);
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
                        ACCEPT_LEXEME(TOKEN_ERR_INVALID_OPERATOR);
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
                        ACCEPT_LEXEME(TOKEN_ERR_INVALID_OPERATOR);
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
                    ACCEPT_LEXEME((lexemeIdx > 32) ? TOKEN_ERR_IDENTIFIER_TOO_LONG : TOKEN_IDENTIFIER);
                }
                break;

            case STATE_DONE:
                break;
        }

        // If the DFA transitioned to a new state in this step, record it in the trace
        if (state != prevState) {
            appendTransition(&token, state);
        }

        switch (0) {
            default:
                break;
        }
    }

    printf("=======================================================================================================================\n");
    fprintf(transition, "=======================================================================================================================\n");
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
        case TOKEN_DOT:             return "DOT";
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
        
        case TOKEN_ERR_UNTERMINATED_BLOCK_COMMENT: return "ERR_UNTERMINATED_BLOCK_COMMENT";
        case TOKEN_ERR_INVALID_NUMBER:      return "ERR_INVALID_NUMBER";
        case TOKEN_ERR_MALFORMED_FLOAT:     return "ERR_MALFORMED_FLOAT";
        case TOKEN_ERR_UNTERMINATED_STRING: return "ERR_UNTERMINATED_STRING";
        case TOKEN_ERR_UNTERMINATED_CHAR:   return "ERR_UNTERMINATED_CHAR";
        case TOKEN_ERR_CHAR_TOO_LONG:       return "ERR_CHAR_TOO_LONG";
        case TOKEN_ERR_EMPTY_CHAR:          return "ERR_EMPTY_CHAR";
        case TOKEN_ERR_INVALID_OPERATOR:    return "ERR_INVALID_OPERATOR";
        case TOKEN_ERR_IDENTIFIER_TOO_LONG: return "ERR_IDENTIFIER_TOO_LONG";
        case TOKEN_ERR_UNRECOGNIZED:        return "ERR_UNRECOGNIZED";


        default:                    return "UNKNOWN";
    }
}
