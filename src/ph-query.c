/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

/* Includes {{{1 */

#include "ph-query.h"
#include "ph-geocache.h"
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>

/* Forward declarations {{{1 */

typedef struct _PHQueryLexerState PHQueryLexerState;
typedef struct _PHQueryToken PHQueryToken;
typedef enum _PHQueryTokenType PHQueryTokenType;
typedef struct _PHQueryParserState PHQueryParserState;
typedef struct _PHQueryCondition PHQueryCondition;
typedef struct _PHQueryConditionType PHQueryConditionType;
typedef struct _PHQueryConditionAlias PHQueryConditionAlias;
typedef struct _PHQueryBoolean PHQueryBoolean;

static gboolean ph_query_get_token(PHQueryLexerState *state,
                                   PHQueryToken *token,
                                   GError **error);
static void ph_query_unget_token(PHQueryLexerState *state,
                                 const PHQueryToken *token);

static gchar *ph_query_get_string(const PHQueryToken *token);
static glong ph_query_get_long(const PHQueryToken *token);
static gdouble ph_query_get_double(const PHQueryToken *token);

static const gchar *ph_query_sql_operator(PHQueryTokenType operator);

static gboolean ph_query_text_condition(
    PHQueryParserState *state, const PHQueryCondition *condition,
    GError **error);
static gboolean ph_query_dt_condition(
    PHQueryParserState *state, const PHQueryCondition *condition,
    GError **error);
static gboolean ph_query_size_condition(
    PHQueryParserState *state, const PHQueryCondition *condition,
    GError **error);
static gboolean ph_query_type_condition(
    PHQueryParserState *state, const PHQueryCondition *condition,
    GError **error);

static gboolean ph_query_parse_or(PHQueryParserState *state, GError **error);
static gboolean ph_query_parse_and(PHQueryParserState *state, GError **error);
static gboolean ph_query_parse_condition(PHQueryParserState *state,
                                         GError **error);
static gboolean ph_query_parse_relation(
    PHQueryParserState *state, const gchar *attr,
    PHQueryTokenType operator, GError **error);
static gboolean ph_query_parse_boolean(
    PHQueryParserState *state, PHQueryTokenType operator, GError **error);

/* Lexer consts and structs {{{1 */

/*
 * Set of tokens for the query language.
 */
enum _PHQueryTokenType {
    /* initial state */
    PH_QUERY_TOKEN_TYPE_NONE = 0,
    /* fixed-length symbol tokens */
    PH_QUERY_TOKEN_TYPE_SYMBOLS,
    PH_QUERY_TOKEN_TYPE_OPEN_PAREN = PH_QUERY_TOKEN_TYPE_SYMBOLS,
    PH_QUERY_TOKEN_TYPE_CLOSE_PAREN,
    PH_QUERY_TOKEN_TYPE_AND,
    PH_QUERY_TOKEN_TYPE_OR,
    PH_QUERY_TOKEN_TYPE_NOT,
    PH_QUERY_TOKEN_TYPE_RELATIONS,
    PH_QUERY_TOKEN_TYPE_COLON = PH_QUERY_TOKEN_TYPE_RELATIONS,
    PH_QUERY_TOKEN_TYPE_LIKE,
    PH_QUERY_TOKEN_TYPE_EQUALS,
    PH_QUERY_TOKEN_TYPE_NOTEQUALS,
    PH_QUERY_TOKEN_TYPE_LESS,
    PH_QUERY_TOKEN_TYPE_LESSEQ,
    PH_QUERY_TOKEN_TYPE_GREATER,
    PH_QUERY_TOKEN_TYPE_GREATEREQ,
    PH_QUERY_TOKEN_TYPE_PLUS,
    PH_QUERY_TOKEN_TYPE_MINUS,
    /* arbitrary-length literal tokens */
    PH_QUERY_TOKEN_TYPE_LITERALS,
    PH_QUERY_TOKEN_TYPE_STRING = PH_QUERY_TOKEN_TYPE_LITERALS,
    PH_QUERY_TOKEN_TYPE_INTEGER,
    PH_QUERY_TOKEN_TYPE_FLOAT,
    PH_QUERY_TOKEN_TYPE_BAREWORD
};

/*
 * Information about a single token.
 */
struct _PHQueryToken {
    PHQueryTokenType type;
    const gchar *start;
    gint length;
};

/*
 * Current state of the lexer.
 */
struct _PHQueryLexerState {
    const gchar *input;
    gint length;
    gint index;
    PHQueryTokenType type;
    PHQueryToken ungot;
};

/* Lexer implementation {{{1 */

/*
 * Quick-and-dirty lexer for the query language.
 */
static gboolean
ph_query_get_token(PHQueryLexerState *state,
                   PHQueryToken *token,
                   GError **error)
{
    const gchar *c;
    gboolean again = FALSE;

    /* retrieve the previous token, if the parser needs it again */
    if (state->ungot.type != PH_QUERY_TOKEN_TYPE_NONE) {
        memcpy(token, &state->ungot, sizeof(PHQueryToken));
        state->ungot.type = PH_QUERY_TOKEN_TYPE_NONE;
        return TRUE;
    }

    state->type = token->type = PH_QUERY_TOKEN_TYPE_NONE;
    token->length = 0;

    /* skip initial whitespace */
    while (state->index < state->length &&
            g_ascii_isspace(state->input[state->index]))
        ++state->index;

    c = token->start = &state->input[state->index];

    if (state->index >= state->length)
        return TRUE;        /* EOF reached */

    token->length = 1;

    /* determine token type from first character */
    g_assert(*c != '\0');
    if (*c == '(')
        state->type = PH_QUERY_TOKEN_TYPE_OPEN_PAREN;
    else if (*c == ')')
        state->type = PH_QUERY_TOKEN_TYPE_CLOSE_PAREN;
    else if (*c == '+')
        state->type = PH_QUERY_TOKEN_TYPE_PLUS;
    else if (*c == '-')
        state->type = PH_QUERY_TOKEN_TYPE_MINUS;
    /* allow doubling of & and | */
    else if (*c == '&') {
        state->type = PH_QUERY_TOKEN_TYPE_AND;
        if (state->index + 1 < state->length) {
            if (*(c + 1) == '&')
                ++token->length;
        }
    }
    else if (*c == '|') {
        state->type = PH_QUERY_TOKEN_TYPE_OR;
        if (state->index + 1 < state->length) {
            if (*(c + 1) == '|')
                ++token->length;
        }
    }
    /* ! could also be != */
    else if (*c == '!') {
        state->type = PH_QUERY_TOKEN_TYPE_NOT;
        if (state->index + 1 < state->length) {
            if (*(c + 1) == '=') {
                state->type = PH_QUERY_TOKEN_TYPE_NOTEQUALS;
                ++token->length;
            }
        }
    }
    /* = could also be == or =~ */
    else if (*c == '=') {
        state->type = PH_QUERY_TOKEN_TYPE_EQUALS;
        if (state->index + 1 < state->length) {
            if (*(c + 1) == '=')
                ++token->length;
            else if (*(c + 1) == '~') {
                state->type = PH_QUERY_TOKEN_TYPE_LIKE;
                ++token->length;
            }
        }
    }
    else if (*c == ':')
        state->type = PH_QUERY_TOKEN_TYPE_COLON;
    /* ~ could also be ~= */
    else if (*c == '~') {
        state->type = PH_QUERY_TOKEN_TYPE_LIKE;
        if (state->index + 1 < state->length) {
            if (*(c + 1) == '=')
                ++token->length;
        }
    }
    /* < and > could also be <= or >= */
    else if (*c == '<') {
        state->type = PH_QUERY_TOKEN_TYPE_LESS;
        if (state->index + 1 < state->length) {
            if (*(c + 1) == '=') {
                state->type = PH_QUERY_TOKEN_TYPE_LESSEQ;
                ++token->length;
            }
        }
    }
    else if (*c == '>') {
        state->type = PH_QUERY_TOKEN_TYPE_GREATER;
        if (state->index + 1 < state->length) {
            if (*(c + 1) == '=') {
                state->type = PH_QUERY_TOKEN_TYPE_GREATEREQ;
                ++token->length;
            }
        }
    }
    else if (*c == '"')
        state->type = PH_QUERY_TOKEN_TYPE_STRING;
    else if (g_ascii_isdigit(*c))
        state->type = PH_QUERY_TOKEN_TYPE_INTEGER;
    else if (*c == '.')
        state->type = PH_QUERY_TOKEN_TYPE_FLOAT;
    else if (g_ascii_isalpha(*c))
        state->type = PH_QUERY_TOKEN_TYPE_BAREWORD;
    else {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_LEXER,
                _("Unrecognized character in query: %c"), *c);
        return FALSE;
    }

    state->index += token->length;

    /* finish here on fixed-length tokens and EOF */
    if (state->type < PH_QUERY_TOKEN_TYPE_LITERALS) {
        token->type = state->type;
        return TRUE;
    }

    for (; state->index < state->length; ++state->index) {
        c = &state->input[state->index];
        g_assert(*c != '\0');

        if (state->type == PH_QUERY_TOKEN_TYPE_STRING) {
            /* C-style strings */
            ++token->length;
            if (*c == '"')
                break;
            if (*c == '\\') {
                if (state->index + 1 < state->length) {
                    ++state->index;
                    ++token->length;
                }
            }
        }

        else if (g_ascii_isspace(*c))
            /* whitespace ends all tokens */
            break;

        /* integers and floats become barewords if letters appear */
        else if (state->type == PH_QUERY_TOKEN_TYPE_INTEGER) {
            if (*c == '.')
                state->type = PH_QUERY_TOKEN_TYPE_FLOAT;
            else if (g_ascii_isalpha(*c))
                state->type = PH_QUERY_TOKEN_TYPE_BAREWORD;
            else if (!g_ascii_isdigit(*c)) {
                again = TRUE;
                break;
            }
            ++token->length;
        }
        else if (state->type == PH_QUERY_TOKEN_TYPE_FLOAT) {
            if (g_ascii_isalpha(*c))
                state->type = PH_QUERY_TOKEN_TYPE_BAREWORD;
            else if (!g_ascii_isdigit(*c)) {
                again = TRUE;
                break;
            }
            ++token->length;
        }

        /* barewords end with the first non-alphanumeric character */
        else if (state->type == PH_QUERY_TOKEN_TYPE_BAREWORD) {
            if (!g_ascii_isalnum(*c)) {
                again = TRUE;
                break;
            }
            ++token->length;
        }
    }

    if (state->type == PH_QUERY_TOKEN_TYPE_STRING &&
            state->index >= state->length) {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_LEXER,
                _("Runaway string literal in query"));
        return FALSE;
    }

    if (!again)
        ++state->index;
    token->type = state->type;

    return TRUE;
}

/*
 * Hand a token back to the lexer so it is returned again on the next
 * ph_query_get_token invokation.
 */
static void
ph_query_unget_token(PHQueryLexerState *state,
                     const PHQueryToken *token)
{
    g_return_if_fail(state->ungot.type == PH_QUERY_TOKEN_TYPE_NONE);

    memcpy(&state->ungot, token, sizeof(PHQueryToken));
}

/* Parser consts and structs {{{1 */

/*
 * State of the parser.
 */
struct _PHQueryParserState {
    PHQueryLexerState *lexer;   /* state of the lexer */
    GString *result;            /* WHERE clause being built */
    PHDatabaseTable tables;     /* database tables needed for the query */
};

/*
 * "Raw" condition consisting of an attribute name, a comparison operator, a
 * token containing the value, and a pointer to the matching row in the table
 * of condition types.
 */
struct _PHQueryCondition {
    const gchar *attr;
    PHQueryTokenType operator;
    const PHQueryToken *token;
    const PHQueryConditionType *type;
};

/*
 * Row in the table of condition types.
 */
struct _PHQueryConditionType {
    const gchar *attr;
    gboolean (*handler)(PHQueryParserState *state,
            const PHQueryCondition *condition, GError **error);
    PHDatabaseTable table;
};

/*
 * Table of known condition types.  This needs to be kept in ascending
 * alphabetic order to allow binary searching.
 */
static const PHQueryConditionType ph_query_condition_types[] = {
    {"creator", ph_query_text_condition,        PH_DATABASE_TABLE_GEOCACHES},
    {"description", ph_query_text_condition,    PH_DATABASE_TABLE_GEOCACHES},
    {"difficulty", ph_query_dt_condition,       PH_DATABASE_TABLE_GEOCACHES},
    {"id",      ph_query_text_condition,        PH_DATABASE_TABLE_GEOCACHES},
    {"name",    ph_query_text_condition,        PH_DATABASE_TABLE_GEOCACHES},
    {"owner",   ph_query_text_condition,        PH_DATABASE_TABLE_GEOCACHES},
    {"size",    ph_query_size_condition,        PH_DATABASE_TABLE_GEOCACHES},
    {"summary", ph_query_text_condition,        PH_DATABASE_TABLE_GEOCACHES},
    {"terrain", ph_query_dt_condition,          PH_DATABASE_TABLE_GEOCACHES},
    {"type",    ph_query_type_condition,        PH_DATABASE_TABLE_GEOCACHES}
};

/*
 * Row in the table of condition aliases.
 */
struct _PHQueryConditionAlias {
    const gchar *short_name;
    const gchar *long_name;
};

/*
 * Known aliases for condition types.  This, too, has to remain sorted.
 */
static const PHQueryConditionAlias ph_query_condition_aliases[] = {
    {"c", "creator"},
    {"d", "difficulty"},
    {"dt", "description"},
    {"i", "id"},
    {"k", "type"},
    {"n", "name"},
    {"o", "owner"},
    {"s", "size"},
    {"st", "summary"},
    {"t", "terrain"}
};

/*
 * Row in the table of possible boolean queries.
 */
struct _PHQueryBoolean {
    const gchar *name;
    PHDatabaseTable table;
    const gchar *column;
    gboolean attribute_match;
    gint value;
};

/*
 * Information about known boolean queries.  Binary searches are performed on
 * this array, so it has to remain sorted.
 */
static const PHQueryBoolean ph_query_booleans[] = {
    { "abandoned", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_ABANDONED },
    { "always", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_ALWAYS },
    { "animals", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_DANGER_ANIMALS },
    { "archived", PH_DATABASE_TABLE_GEOCACHES, "archived", FALSE, 1 },
    { "available", PH_DATABASE_TABLE_GEOCACHES, "available", FALSE, 1 },
    { "beacon", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_BEACON },
    { "bike", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_BICYCLES },
    { "boat", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_BOAT },
    { "campfire", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_CAMPFIRES },
    { "camping", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_CAMPING },
    { "child", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_KIDS },
    { "cliff", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_CLIFF },
    { "climb", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_CLIMBING },
    { "climbgear", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_CLIMBING_GEAR },
    { "danger", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_DANGER_AREA },
    { "dog", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_DOGS },
    { "fee", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_FEE },
    { "fieldpuzzle", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_FIELD_PUZZLE },
    { "flashlight", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_FLASHLIGHT },
    { "food", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_FOOD },
    { "found", PH_DATABASE_TABLE_GEOCACHES | PH_DATABASE_TABLE_GEOCACHE_NOTES,
        NULL, FALSE, 0 },               /* special case */
    { "fuel", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_FUEL },
    { "hike", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_HIKE },
    { "horse", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_HORSES },
    { "hunting", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_HUNTING },
    { "livestock", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_LIVESTOCK },
    { "logged", PH_DATABASE_TABLE_GEOCACHES, "logged", FALSE, 1 },
    { "long", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_LONG_HIKE },
    { "lostfound", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_LOST_AND_FOUND },
    { "maint", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_MAINTENANCE },
    { "medium", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_MEDIUM_HIKE },
    { "mines", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_MINES },
    { "motorbike", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_MOTORCYCLES },
    { "nc", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_NIGHT_CACHE },
    { "night", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_NIGHT },
    { "offroad", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_OFFROAD },
    { "parkgrab", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_PARK_AND_GRAB },
    { "parking", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_PARKING },
    { "phone", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_TELEPHONE },
    { "picnic", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_PICNIC_TABLES },
    { "poison", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_POISONOUS },
    { "pubtrans", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_PUBLIC_TRANSPORT },
    { "quad", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_QUADS },
    { "quick", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_ONE_HOUR },
    { "restrooms", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_RESTROOMS },
    { "rv", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_RV },
    { "scenic", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_SCENIC },
    { "scuba", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_SCUBA_GEAR },
    { "short", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_SHORT_HIKE },
    { "snowmob", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_SNOWMOBILES },
    { "snowshoes", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_SNOWSHOES },
    { "stealth", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_STEALTH },
    { "stroller", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_STROLLER },
    { "swim", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_SWIMMING },
    { "thorns", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_THORNS },
    { "ticks", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_TICKS },
    { "tool", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_SPECIAL_TOOL },
    { "uv", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_UV },
    { "wade", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_WADING },
    { "water", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_DRINKING_WATER },
    { "wheelchair", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_WHEELCHAIR },
    { "winter", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_WINTER },
    { "xcskis", PH_DATABASE_TABLE_GEOCACHES, "attributes", TRUE,
        PH_GEOCACHE_ATTR_XC_SKIS }
};

/* Helper functions for the parser {{{1 */

/*
 * Obtain the "true" value of a string (without the quotation marks and after
 * handling all escapes).
 */
static gchar *
ph_query_get_string(const PHQueryToken *token)
{
    gchar *result, *out;
    const gchar *in;
    gboolean escape = FALSE;

    g_return_val_if_fail(token->type == PH_QUERY_TOKEN_TYPE_STRING, NULL);

    out = result = g_new(gchar, token->length);
    for (in = token->start + 1; in < token->start + token->length - 1; ++in) {
        if (escape) {
            switch (*in) {
            case 'n':   *out = '\n'; break;
            case 't':   *out = '\t'; break;
            default:    *out = *in;
            }
            escape = FALSE;
        }
        else if (*in == '\\') {
            escape = TRUE;
            continue;
        }
        else
            *out = *in;
        ++out;
    }
    *out = '\0';

    return result;
}

/*
 * Get a long value from an integer token.
 */
static glong
ph_query_get_long(const PHQueryToken *token)
{
    gchar *source;
    glong result;

    source = g_strndup(token->start, token->length);
    result = strtol(source, NULL, 10);
    g_free(source);

    return result;
}

/*
 * Get a double value from a float token.
 */
static gdouble
ph_query_get_double(const PHQueryToken *token)
{
    gchar *source;
    gdouble result;

    source = g_strndup(token->start, token->length);
    result = g_ascii_strtod(source, NULL);
    g_free(source);

    return result;
}

/* Interpretation of conditions {{{1 */

/*
 * Convert an operator token from the query language to an SQL operator.
 */
static const gchar *
ph_query_sql_operator(PHQueryTokenType operator)
{
    switch (operator) {
    case PH_QUERY_TOKEN_TYPE_COLON:
    case PH_QUERY_TOKEN_TYPE_EQUALS:
        return "=";
    case PH_QUERY_TOKEN_TYPE_NOTEQUALS:
        return "<>";
    case PH_QUERY_TOKEN_TYPE_LIKE:
        return "LIKE";
    case PH_QUERY_TOKEN_TYPE_LESS:
        return "<";
    case PH_QUERY_TOKEN_TYPE_LESSEQ:
        return "<=";
    case PH_QUERY_TOKEN_TYPE_GREATER:
        return ">";
    case PH_QUERY_TOKEN_TYPE_GREATEREQ:
        return ">=";
    default:
        g_return_val_if_reached(NULL);
    }
}

/*
 * Match on a textual column.
 */
static gboolean
ph_query_text_condition(PHQueryParserState *state,
                        const PHQueryCondition* condition,
                        GError **error)
{
    const gchar *sqlop = ph_query_sql_operator(condition->operator);
    const gchar *table = ph_database_table_name(condition->type->table);
    gchar *value;
    char *sql;

    if (condition->token->type == PH_QUERY_TOKEN_TYPE_STRING)
        value = ph_query_get_string(condition->token);
    else
        value = g_strndup(condition->token->start, condition->token->length);

    sql = sqlite3_mprintf("%s.%s %s %Q", table, condition->attr, sqlop, value);
    g_string_append(state->result, sql);
    sqlite3_free(sql);
    g_free(value);

    return TRUE;
}

/*
 * Match on the difficulty/terrain columns.
 */
static gboolean
ph_query_dt_condition(PHQueryParserState *state,
                      const PHQueryCondition *condition,
                      GError **error)
{
    const gchar *sqlop, *table;
    gdouble raw_value;
    gint value;
    char *sql;

    if (condition->operator != PH_QUERY_TOKEN_TYPE_COLON &&
            condition->operator != PH_QUERY_TOKEN_TYPE_EQUALS &&
            condition->operator != PH_QUERY_TOKEN_TYPE_NOTEQUALS &&
            condition->operator != PH_QUERY_TOKEN_TYPE_LESS &&
            condition->operator != PH_QUERY_TOKEN_TYPE_LESSEQ &&
            condition->operator != PH_QUERY_TOKEN_TYPE_GREATER &&
            condition->operator != PH_QUERY_TOKEN_TYPE_GREATEREQ) {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                _("Cannot compare %s value with this operator"),
                condition->attr);
        return FALSE;
    }
    else if (condition->token->type != PH_QUERY_TOKEN_TYPE_INTEGER &&
            condition->token->type != PH_QUERY_TOKEN_TYPE_FLOAT) {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                _("Cannot compare %s with non-numerical value"),
                condition->attr);
        return FALSE;
    }

    sqlop = ph_query_sql_operator(condition->operator);
    table = ph_database_table_name(condition->type->table);
    raw_value = ph_query_get_double(condition->token);
    value = (gint) (raw_value * 10);

    sql = sqlite3_mprintf("%s.%s %s %d", table, condition->attr, sqlop, value);
    g_string_append(state->result, sql);
    sqlite3_free(sql);

    return TRUE;
}

/*
 * Match on the size column.
 */
static gboolean
ph_query_size_condition(PHQueryParserState *state,
                        const PHQueryCondition *condition,
                        GError **error)
{
    const gchar *sqlop, *table;
    PHGeocacheSize value = PH_GEOCACHE_SIZE_UNKNOWN;
    char *sql;

    if (condition->operator != PH_QUERY_TOKEN_TYPE_COLON &&
            condition->operator != PH_QUERY_TOKEN_TYPE_EQUALS &&
            condition->operator != PH_QUERY_TOKEN_TYPE_NOTEQUALS) {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                _("Can only match %s on equality"), condition->attr);
        return FALSE;
    }

    if (condition->token->type == PH_QUERY_TOKEN_TYPE_INTEGER)
        value = ph_query_get_long(condition->token);
    else if (condition->token->type == PH_QUERY_TOKEN_TYPE_BAREWORD) {
        gchar *raw_value = g_strndup(
                condition->token->start, condition->token->length);

        if (strcasecmp(raw_value, "micro") == 0)
            value = PH_GEOCACHE_SIZE_MICRO;
        else if (strcasecmp(raw_value, "small") == 0)
            value = PH_GEOCACHE_SIZE_SMALL;
        else if (strcasecmp(raw_value, "regular") == 0)
            value = PH_GEOCACHE_SIZE_REGULAR;
        else if (strcasecmp(raw_value, "large") == 0)
            value = PH_GEOCACHE_SIZE_LARGE;
        else if (strcasecmp(raw_value, "virtual") == 0)
            value = PH_GEOCACHE_SIZE_VIRTUAL;
        else if (strcasecmp(raw_value, "other") == 0)
            value = PH_GEOCACHE_SIZE_OTHER;

        g_free(raw_value);
    }

    if (value == PH_GEOCACHE_SIZE_UNKNOWN) {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                _("Cannot match %s with %.*s"), condition->attr,
                condition->token->length, condition->token->start);
        return FALSE;
    }

    sqlop = ph_query_sql_operator(condition->operator);
    table = ph_database_table_name(condition->type->table);

    sql = sqlite3_mprintf("%s.%s %s %d",
            table, condition->attr, sqlop, (gint) value);
    g_string_append(state->result, sql);
    sqlite3_free(sql);

    return TRUE;
}

/*
 * Match on the type column.
 */
static gboolean
ph_query_type_condition(PHQueryParserState *state,
                        const PHQueryCondition *condition,
                        GError **error)
{
    const gchar *sqlop, *table;
    PHGeocacheType value = PH_GEOCACHE_TYPE_UNKNOWN;
    char *sql;

    if (condition->operator != PH_QUERY_TOKEN_TYPE_COLON &&
            condition->operator != PH_QUERY_TOKEN_TYPE_EQUALS &&
            condition->operator != PH_QUERY_TOKEN_TYPE_NOTEQUALS) {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                _("Can only match %s on equality"), condition->attr);
        return FALSE;
    }

    if (condition->token->type == PH_QUERY_TOKEN_TYPE_INTEGER)
        value = ph_query_get_long(condition->token);
    else if (condition->token->type == PH_QUERY_TOKEN_TYPE_BAREWORD) {
        gchar *raw_value = g_strndup(
                condition->token->start, condition->token->length);

        if (strcasecmp(raw_value, "traditional") == 0)
            value = PH_GEOCACHE_TYPE_TRADITIONAL;
        else if (strcasecmp(raw_value, "multi") == 0)
            value = PH_GEOCACHE_TYPE_MULTI;
        else if (strcasecmp(raw_value, "mystery") == 0)
            value = PH_GEOCACHE_TYPE_MYSTERY;
        else if (strcasecmp(raw_value, "letterbox") == 0)
            value = PH_GEOCACHE_TYPE_LETTERBOX;
        else if (strcasecmp(raw_value, "wherigo") == 0)
            value = PH_GEOCACHE_TYPE_WHERIGO;
        else if (strcasecmp(raw_value, "event") == 0)
            value = PH_GEOCACHE_TYPE_EVENT;
        else if (strcasecmp(raw_value, "mega") == 0)
            value = PH_GEOCACHE_TYPE_MEGA_EVENT;
        else if (strcasecmp(raw_value, "cito") == 0)
            value = PH_GEOCACHE_TYPE_CITO;
        else if (strcasecmp(raw_value, "earth") == 0)
            value = PH_GEOCACHE_TYPE_EARTH;
        else if (strcasecmp(raw_value, "virtual") == 0)
            value = PH_GEOCACHE_TYPE_VIRTUAL;
        else if (strcasecmp(raw_value, "webcam") == 0)
            value = PH_GEOCACHE_TYPE_WEBCAM;

        g_free(raw_value);
    }

    if (value == PH_GEOCACHE_TYPE_UNKNOWN) {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                _("Cannot match %s with %.*s"), condition->attr,
                condition->token->length, condition->token->start);
        return FALSE;
    }

    sqlop = ph_query_sql_operator(condition->operator);
    table = ph_database_table_name(condition->type->table);

    sql = sqlite3_mprintf("%s.%s %s %d",
            table, condition->attr, sqlop, (gint) value);
    g_string_append(state->result, sql);
    sqlite3_free(sql);

    return TRUE;
}

/* Parser {{{1 */

/*
 * Parse an OR expression.  OR has the lowest operator precedence, so this is
 * used for the root of the expression as well.
 */
static gboolean
ph_query_parse_or(PHQueryParserState *state,
                  GError **error)
{
    PHQueryToken token;
    gboolean success;

    g_string_append_c(state->result, '(');

    success = ph_query_parse_and(state, error);

    while (success && (success = ph_query_get_token(
                    state->lexer, &token, error))) {
        if (token.type == PH_QUERY_TOKEN_TYPE_OR ||
                (token.type == PH_QUERY_TOKEN_TYPE_BAREWORD &&
                 token.length == 2 &&
                 strncasecmp(token.start, "or", 2) == 0))
            /* found "or" */
            ;
        else {
            ph_query_unget_token(state->lexer, &token);
            break;
        }

        g_string_append(state->result, " OR ");
        success = ph_query_parse_and(state, error);
    }

    g_string_append_c(state->result, ')');

    return success;
}

/*
 * Parse an AND expression.
 */
static gboolean
ph_query_parse_and(PHQueryParserState *state,
                   GError **error)
{
    PHQueryToken token;
    gboolean success;

    g_string_append_c(state->result, '(');

    success = ph_query_parse_condition(state, error);

    while (success && (success = ph_query_get_token(
                    state->lexer, &token, error))) {
        if (token.type == PH_QUERY_TOKEN_TYPE_OR ||
                (token.type == PH_QUERY_TOKEN_TYPE_BAREWORD &&
                 token.length == 2 &&
                 strncasecmp(token.start, "or", 2) == 0)) {
            /* found "or": lower precedence */
            ph_query_unget_token(state->lexer, &token);
            break;
        }
        else if (token.type == PH_QUERY_TOKEN_TYPE_CLOSE_PAREN ||
                token.type == PH_QUERY_TOKEN_TYPE_NONE) {
            /* found ")" or end of query: lower precedence */
            ph_query_unget_token(state->lexer, &token);
            break;
        }
        else if (token.type == PH_QUERY_TOKEN_TYPE_AND ||
                (token.type == PH_QUERY_TOKEN_TYPE_BAREWORD &&
                 token.length == 3 &&
                 strncasecmp(token.start, "and", 3) == 0))
            /* found explicit "and" */
            ;
        else
            /* no operator: implicit "and" */
            ph_query_unget_token(state->lexer, &token);

        g_string_append(state->result, " AND ");
        success = ph_query_parse_condition(state, error);
    }

    g_string_append_c(state->result, ')');

    return success;
}

/*
 * Parse a boolean match, a binary relation, a keyword match or a
 * parenthesized sub-expression.
 */
static gboolean
ph_query_parse_condition(PHQueryParserState *state,
                         GError **error)
{
    PHQueryToken token;

    g_string_append_c(state->result, '(');

    /* interpret negations */
    for (;;) {
        if (!ph_query_get_token(state->lexer, &token, error))
            return FALSE;
        else if (token.type == PH_QUERY_TOKEN_TYPE_NOT ||
                (token.type == PH_QUERY_TOKEN_TYPE_BAREWORD &&
                 token.length == 3 &&
                 strncasecmp(token.start, "not", 3) == 0))
            g_string_append(state->result, "NOT ");
        else
            break;
    }

    /* subexpression */
    if (token.type == PH_QUERY_TOKEN_TYPE_OPEN_PAREN) {
        if (!ph_query_parse_or(state, error))
            return FALSE;
        else if (!ph_query_get_token(state->lexer, &token, error))
            return FALSE;
        else if (token.type != PH_QUERY_TOKEN_TYPE_CLOSE_PAREN) {
            g_set_error_literal(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                    _("Expected ')' at end of subexpression"));
            return FALSE;
        }
    }

    /* boolean condition */
    else if (token.type == PH_QUERY_TOKEN_TYPE_PLUS ||
            token.type == PH_QUERY_TOKEN_TYPE_MINUS) {
        if (!ph_query_parse_boolean(state, token.type, error))
            return FALSE;
    }

    /* relation */
    else if (token.type >= PH_QUERY_TOKEN_TYPE_LITERALS) {
        gchar *attr;
        gboolean name_match;

        if (token.type == PH_QUERY_TOKEN_TYPE_STRING)
            attr = ph_query_get_string(&token);
        else
            attr = g_strndup(token.start, token.length);

        name_match = (token.type != PH_QUERY_TOKEN_TYPE_BAREWORD);

        /* see if we have something like "d > 2.5" */
        if (!name_match) {
            if (!ph_query_get_token(state->lexer, &token, error)) {
                g_free(attr);
                return FALSE;
            }
            else if (token.type >= PH_QUERY_TOKEN_TYPE_RELATIONS &&
                    token.type < PH_QUERY_TOKEN_TYPE_LITERALS) {
                if (!ph_query_parse_relation(state, attr, token.type, error)) {
                    g_free(attr);
                    return FALSE;
                }
            }
            else {
                ph_query_unget_token(state->lexer, &token);
                name_match = TRUE;
            }
        }

        /* nope, just match on geocache names */
        if (name_match) {
            char *sql = sqlite3_mprintf("geocaches.name LIKE '%%%q%%'", attr);
            g_string_append(state->result, sql);
            sqlite3_free(sql);
            state->tables |= PH_DATABASE_TABLE_GEOCACHES;
        }

        g_free(attr);
    }

    /* invalid condition */
    else {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                _("Unexpected '%.*s' at start of search condition"),
                token.length, token.start);
        return FALSE;
    }

    g_string_append_c(state->result, ')');

    return TRUE;
}

/*
 * Parse a relation such as "owner ~= john".
 */
static gboolean
ph_query_parse_relation(PHQueryParserState *state,
                        const gchar *attr,
                        PHQueryTokenType operator,
                        GError **error)
{
    PHQueryToken token;
    const gchar *real_attr = NULL;
    const PHQueryConditionType *type = NULL;
    PHQueryCondition condition;
    gint k, l, m;

    if (!ph_query_get_token(state->lexer, &token, error))
        return FALSE;

    /* binary search in alias table */
    for (l = 0, m = sizeof(ph_query_condition_aliases) /
            sizeof(PHQueryConditionAlias) - 1; m >= l;) {
        int cmp;
        k = (l + m) / 2;
        cmp = strcmp(attr, ph_query_condition_aliases[k].short_name);
        if (cmp == 0) {
            real_attr = ph_query_condition_aliases[k].long_name;
            break;
        }
        else if (cmp < 0)
            m = k - 1;
        else
            l = k + 1;
    }

    if (real_attr == NULL)
        real_attr = attr;

    /* binary search in condition type table */
    for (l = 0, m = sizeof(ph_query_condition_types) /
            sizeof(PHQueryConditionType) - 1; m >= l;) {
        int cmp;
        k = (l + m) / 2;
        cmp = strcmp(real_attr, ph_query_condition_types[k].attr);
        if (cmp == 0) {
            type = &ph_query_condition_types[k];
            break;
        }
        else if (cmp < 0)
            m = k - 1;
        else
            l = k + 1;
    }

    if (type == NULL) {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                _("Unknown attribute in comparison: %s"), attr);
        return FALSE;
    }

    condition.attr = real_attr;
    condition.operator = operator;
    condition.token = &token;
    condition.type = type;

    state->tables |= type->table;

    return type->handler(state, &condition, error);
}

/*
 * Parse a boolean match such as "+flashlight".
 */
static gboolean
ph_query_parse_boolean(PHQueryParserState *state,
                       PHQueryTokenType operator,
                       GError **error)
{
    gint k, l, m;
    const PHQueryBoolean *match = NULL;
    PHQueryToken token;
    gchar *name;
    const gchar *table;
    char *sql;

    if (!ph_query_get_token(state->lexer, &token, error))
        return FALSE;
    else if (token.type != PH_QUERY_TOKEN_TYPE_BAREWORD) {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                _("Unexpected non-bareword token in boolean query: %.*s"),
                    token.length, token.start);
        return FALSE;
    }

    name = g_strndup(token.start, token.length);

    /* binary search in boolean query table */
    for (l = 0, m = sizeof(ph_query_booleans) / sizeof(PHQueryBoolean) - 1;
            m >= l;) {
        int cmp;
        k = (l + m) / 2;
        cmp = strcmp(name, ph_query_booleans[k].name);
        if (cmp == 0) {
            match = &ph_query_booleans[k];
            break;
        }
        else if (cmp < 0)
            m = k - 1;
        else
            l = k + 1;
    }

    g_free(name);

    if (match == NULL) {
        g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                _("Unknown attribute in boolean query: %.*s"),
                token.length, token.start);
        return FALSE;
    }

    table = ph_database_table_name(match->table);

    if (match->column == NULL && strcmp(match->name, "found") == 0)
        /* geocache logged or manually marked as found? */
        sql = sqlite3_mprintf(
                "%s(geocaches.logged = 1 OR geocache_notes.found IS NOT NULL)",
                (operator == PH_QUERY_TOKEN_TYPE_PLUS) ? "" : "NOT ");
    else if (match->attribute_match)
        /* for geocache attributes, match on the TEXT column "attributes" */
        sql = sqlite3_mprintf("%s.%s LIKE '%%%c%d;%%'",
                table, match->column,
                (operator == PH_QUERY_TOKEN_TYPE_PLUS) ? '+' : '-',
                match->value);
    else
        /* otherwise test for equality */
        sql = sqlite3_mprintf("%s.%s %s %d",
                table, match->column,
                (operator == PH_QUERY_TOKEN_TYPE_PLUS) ? "=" : "<>",
                match->value);

    g_string_append(state->result, sql);
    sqlite3_free(sql);

    state->tables |= match->table;

    return TRUE;
}

/* Public interface {{{1 */

/*
 * Obtain an SQL SELECT statement for the given query.  The caller is
 * responsible for freeing the returned string.
 */
gchar *
ph_query_compile(const gchar *query,
                 PHDatabaseTable tables,
                 const gchar *columns,
                 GError **error)
{
    PHQueryLexerState lexer = {0};
    PHQueryParserState parser = {0};
    PHQueryToken token;
    gboolean success = TRUE;
    gchar *where;
    GString *sql;

    g_return_val_if_fail(query != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    lexer.input = query;
    lexer.length = strlen(query);

    parser.lexer = &lexer;
    parser.result = g_string_new(NULL);
    parser.tables = tables;

    success = ph_query_get_token(&lexer, &token, error);
    if (!success)
        ;
    else if (token.type == PH_QUERY_TOKEN_TYPE_NONE)
        /* empty query */
        g_string_append_c(parser.result, '1');
    else {
        /* non-empty query */
        ph_query_unget_token(&lexer, &token);
        if (!ph_query_parse_or(&parser, error))
            success = FALSE;
        else if (ph_query_get_token(&lexer, &token, error)) {
            if (token.type != PH_QUERY_TOKEN_TYPE_NONE) {
                g_set_error(error, PH_QUERY_ERROR, PH_QUERY_ERROR_PARSER,
                        _("Superfluous token '%.*s'"),
                        token.length, token.start);
                success = FALSE;
            }
        }
        else
            success = FALSE;
    }

    where = g_string_free(parser.result, FALSE);

    if (!success) {
        g_free(where);
        return NULL;
    }

    sql = g_string_new("SELECT ");
    g_string_append(sql, (columns == NULL) ? "geocaches.*" : columns);
    g_string_append(sql, " FROM geocaches ");
    if (parser.tables & PH_DATABASE_TABLE_WAYPOINTS)
        g_string_append(sql,
                "INNER JOIN waypoints ON waypoints.id = geocaches.id ");
    if (parser.tables & PH_DATABASE_TABLE_GEOCACHE_NOTES)
        g_string_append(sql,
                "LEFT JOIN geocache_notes ON geocache_notes.id = geocaches.id ");
    if (parser.tables & PH_DATABASE_TABLE_WAYPOINT_NOTES)
        g_string_append(sql,
                "LEFT JOIN waypoint_notes ON waypoint_notes.id = geocaches.id ");
    g_string_append(sql, "WHERE ");
    g_string_append(sql, where);

    return g_string_free(sql, FALSE);
}

/* Error reporting {{{1 */

/*
 * Unique identifier for query errors.
 */
GQuark
ph_query_error_quark()
{
    return g_quark_from_static_string("ph-query-error");
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
