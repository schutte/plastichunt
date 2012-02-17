/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_QUERY_H
#define PH_QUERY_H

/* Includes {{{1 */

#include "ph-database.h"
#include <glib.h>

/* Public interface {{{1 */

gchar *ph_query_compile(const gchar *query,
                        PHDatabaseTable tables,
                        const gchar *columns,
                        GError **error);

/* Error reporting {{{1 */

#define PH_QUERY_ERROR ph_query_error_quark()
GQuark ph_query_error_quark();
typedef enum {
    PH_QUERY_ERROR_LEXER,
    PH_QUERY_ERROR_PARSER
} PHQueryError;

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
