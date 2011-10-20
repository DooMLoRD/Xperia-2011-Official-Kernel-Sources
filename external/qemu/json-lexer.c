/*
 * JSON lexer
 *
 * Copyright IBM, Corp. 2009
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#include "qstring.h"
#include "qlist.h"
#include "qdict.h"
#include "qint.h"
#include "qemu-common.h"
#include "json-lexer.h"

/*
 * \"([^\\\"]|(\\\"\\'\\\\\\/\\b\\f\\n\\r\\t\\u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))*\"
 * '([^\\']|(\\\"\\'\\\\\\/\\b\\f\\n\\r\\t\\u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]))*'
 * 0|([1-9][0-9]*(.[0-9]+)?([eE]([-+])?[0-9]+))
 * [{}\[\],:]
 * [a-z]+
 *
 */

#undef ERROR

enum json_lexer_state {
    ERROR = 0,
    IN_DONE_STRING,
    IN_DQ_UCODE3,
    IN_DQ_UCODE2,
    IN_DQ_UCODE1,
    IN_DQ_UCODE0,
    IN_DQ_STRING_ESCAPE,
    IN_DQ_STRING,
    IN_SQ_UCODE3,
    IN_SQ_UCODE2,
    IN_SQ_UCODE1,
    IN_SQ_UCODE0,
    IN_SQ_STRING_ESCAPE,
    IN_SQ_STRING,
    IN_ZERO,
    IN_DIGITS,
    IN_DIGIT,
    IN_EXP_E,
    IN_MANTISSA,
    IN_MANTISSA_DIGITS,
    IN_NONZERO_NUMBER,
    IN_NEG_NONZERO_NUMBER,
    IN_KEYWORD,
    IN_ESCAPE,
    IN_ESCAPE_L,
    IN_ESCAPE_LL,
    IN_ESCAPE_I,
    IN_ESCAPE_I6,
    IN_ESCAPE_I64,
    IN_ESCAPE_DONE,
    IN_WHITESPACE,
    IN_OPERATOR_DONE,
    IN_START,
};

#define TERMINAL(state) [0 ... 0x7F] = (state)

static const uint8_t json_lexer[][256] =  {
    [IN_DONE_STRING] = {
        TERMINAL(JSON_STRING),
    },

    /* double quote string */
    [IN_DQ_UCODE3] = {
        ['0' ... '9'] = IN_DQ_STRING,
        ['a' ... 'f'] = IN_DQ_STRING,
        ['A' ... 'F'] = IN_DQ_STRING,
    },
    [IN_DQ_UCODE2] = {
        ['0' ... '9'] = IN_DQ_UCODE3,
        ['a' ... 'f'] = IN_DQ_UCODE3,
        ['A' ... 'F'] = IN_DQ_UCODE3,
    },
    [IN_DQ_UCODE1] = {
        ['0' ... '9'] = IN_DQ_UCODE2,
        ['a' ... 'f'] = IN_DQ_UCODE2,
        ['A' ... 'F'] = IN_DQ_UCODE2,
    },
    [IN_DQ_UCODE0] = {
        ['0' ... '9'] = IN_DQ_UCODE1,
        ['a' ... 'f'] = IN_DQ_UCODE1,
        ['A' ... 'F'] = IN_DQ_UCODE1,
    },
    [IN_DQ_STRING_ESCAPE] = {
        ['b'] = IN_DQ_STRING,
        ['f'] =  IN_DQ_STRING,
        ['n'] =  IN_DQ_STRING,
        ['r'] =  IN_DQ_STRING,
        ['t'] =  IN_DQ_STRING,
        ['\''] = IN_DQ_STRING,
        ['\"'] = IN_DQ_STRING,
        ['u'] = IN_DQ_UCODE0,
    },
    [IN_DQ_STRING] = {
        [1 ... 0xFF] = IN_DQ_STRING,
        ['\\'] = IN_DQ_STRING_ESCAPE,
        ['"'] = IN_DONE_STRING,
    },

    /* single quote string */
    [IN_SQ_UCODE3] = {
        ['0' ... '9'] = IN_SQ_STRING,
        ['a' ... 'f'] = IN_SQ_STRING,
        ['A' ... 'F'] = IN_SQ_STRING,
    },
    [IN_SQ_UCODE2] = {
        ['0' ... '9'] = IN_SQ_UCODE3,
        ['a' ... 'f'] = IN_SQ_UCODE3,
        ['A' ... 'F'] = IN_SQ_UCODE3,
    },
    [IN_SQ_UCODE1] = {
        ['0' ... '9'] = IN_SQ_UCODE2,
        ['a' ... 'f'] = IN_SQ_UCODE2,
        ['A' ... 'F'] = IN_SQ_UCODE2,
    },
    [IN_SQ_UCODE0] = {
        ['0' ... '9'] = IN_SQ_UCODE1,
        ['a' ... 'f'] = IN_SQ_UCODE1,
        ['A' ... 'F'] = IN_SQ_UCODE1,
    },
    [IN_SQ_STRING_ESCAPE] = {
        ['b'] = IN_SQ_STRING,
        ['f'] =  IN_SQ_STRING,
        ['n'] =  IN_SQ_STRING,
        ['r'] =  IN_SQ_STRING,
        ['t'] =  IN_SQ_STRING,
        ['\''] = IN_SQ_STRING,
        ['\"'] = IN_SQ_STRING,
        ['u'] = IN_SQ_UCODE0,
    },
    [IN_SQ_STRING] = {
        [1 ... 0xFF] = IN_SQ_STRING,
        ['\\'] = IN_SQ_STRING_ESCAPE,
        ['\''] = IN_DONE_STRING,
    },

    /* Zero */
    [IN_ZERO] = {
        TERMINAL(JSON_INTEGER),
        ['0' ... '9'] = ERROR,
        ['.'] = IN_MANTISSA,
    },

    /* Float */
    [IN_DIGITS] = {
        TERMINAL(JSON_FLOAT),
        ['0' ... '9'] = IN_DIGITS,
    },

    [IN_DIGIT] = {
        ['0' ... '9'] = IN_DIGITS,
    },

    [IN_EXP_E] = {
        ['-'] = IN_DIGIT,
        ['+'] = IN_DIGIT,
        ['0' ... '9'] = IN_DIGITS,
    },

    [IN_MANTISSA_DIGITS] = {
        TERMINAL(JSON_FLOAT),
        ['0' ... '9'] = IN_MANTISSA_DIGITS,
        ['e'] = IN_EXP_E,
        ['E'] = IN_EXP_E,
    },

    [IN_MANTISSA] = {
        ['0' ... '9'] = IN_MANTISSA_DIGITS,
    },

    /* Number */
    [IN_NONZERO_NUMBER] = {
        TERMINAL(JSON_INTEGER),
        ['0' ... '9'] = IN_NONZERO_NUMBER,
        ['e'] = IN_EXP_E,
        ['E'] = IN_EXP_E,
        ['.'] = IN_MANTISSA,
    },

    [IN_NEG_NONZERO_NUMBER] = {
        ['0'] = IN_ZERO,
        ['1' ... '9'] = IN_NONZERO_NUMBER,
    },

    /* keywords */
    [IN_KEYWORD] = {
        TERMINAL(JSON_KEYWORD),
        ['a' ... 'z'] = IN_KEYWORD,
    },

    /* whitespace */
    [IN_WHITESPACE] = {
        TERMINAL(JSON_SKIP),
        [' '] = IN_WHITESPACE,
        ['\t'] = IN_WHITESPACE,
        ['\r'] = IN_WHITESPACE,
        ['\n'] = IN_WHITESPACE,
    },        

    /* operator */
    [IN_OPERATOR_DONE] = {
        TERMINAL(JSON_OPERATOR),
    },

    /* escape */
    [IN_ESCAPE_DONE] = {
        TERMINAL(JSON_ESCAPE),
    },

    [IN_ESCAPE_LL] = {
        ['d'] = IN_ESCAPE_DONE,
    },

    [IN_ESCAPE_L] = {
        ['d'] = IN_ESCAPE_DONE,
        ['l'] = IN_ESCAPE_LL,
    },

    [IN_ESCAPE_I64] = {
        ['d'] = IN_ESCAPE_DONE,
    },

    [IN_ESCAPE_I6] = {
        ['4'] = IN_ESCAPE_I64,
    },

    [IN_ESCAPE_I] = {
        ['6'] = IN_ESCAPE_I6,
    },

    [IN_ESCAPE] = {
        ['d'] = IN_ESCAPE_DONE,
        ['i'] = IN_ESCAPE_DONE,
        ['p'] = IN_ESCAPE_DONE,
        ['s'] = IN_ESCAPE_DONE,
        ['f'] = IN_ESCAPE_DONE,
        ['l'] = IN_ESCAPE_L,
        ['I'] = IN_ESCAPE_I,
    },

    /* top level rule */
    [IN_START] = {
        ['"'] = IN_DQ_STRING,
        ['\''] = IN_SQ_STRING,
        ['0'] = IN_ZERO,
        ['1' ... '9'] = IN_NONZERO_NUMBER,
        ['-'] = IN_NEG_NONZERO_NUMBER,
        ['{'] = IN_OPERATOR_DONE,
        ['}'] = IN_OPERATOR_DONE,
        ['['] = IN_OPERATOR_DONE,
        [']'] = IN_OPERATOR_DONE,
        [','] = IN_OPERATOR_DONE,
        [':'] = IN_OPERATOR_DONE,
        ['a' ... 'z'] = IN_KEYWORD,
        ['%'] = IN_ESCAPE,
        [' '] = IN_WHITESPACE,
        ['\t'] = IN_WHITESPACE,
        ['\r'] = IN_WHITESPACE,
        ['\n'] = IN_WHITESPACE,
    },
};

void json_lexer_init(JSONLexer *lexer, JSONLexerEmitter func)
{
    lexer->emit = func;
    lexer->state = IN_START;
    lexer->token = qstring_new();
}

static int json_lexer_feed_char(JSONLexer *lexer, char ch)
{
    char buf[2];

    lexer->x++;
    if (ch == '\n') {
        lexer->x = 0;
        lexer->y++;
    }

    lexer->state = json_lexer[lexer->state][(uint8_t)ch];

    switch (lexer->state) {
    case JSON_OPERATOR:
    case JSON_ESCAPE:
    case JSON_INTEGER:
    case JSON_FLOAT:
    case JSON_KEYWORD:
    case JSON_STRING:
        lexer->emit(lexer, lexer->token, lexer->state, lexer->x, lexer->y);
    case JSON_SKIP:
        lexer->state = json_lexer[IN_START][(uint8_t)ch];
        QDECREF(lexer->token);
        lexer->token = qstring_new();
        break;
    case ERROR:
        return -EINVAL;
    default:
        break;
    }

    buf[0] = ch;
    buf[1] = 0;

    qstring_append(lexer->token, buf);

    return 0;
}

int json_lexer_feed(JSONLexer *lexer, const char *buffer, size_t size)
{
    size_t i;

    for (i = 0; i < size; i++) {
        int err;

        err = json_lexer_feed_char(lexer, buffer[i]);
        if (err < 0) {
            return err;
        }
    }

    return 0;
}

int json_lexer_flush(JSONLexer *lexer)
{
    return json_lexer_feed_char(lexer, 0);
}

void json_lexer_destroy(JSONLexer *lexer)
{
    QDECREF(lexer->token);
}
