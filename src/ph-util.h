/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_UTIL_H
#define PH_UTIL_H

/* Includes {{{1 */

#include <glib.h>

/* Utility functions {{{1 */

gchar *ph_util_find_data_file(const gchar *directory,
                              const gchar *name);

void ph_util_open_in_browser(const gchar *uri);

gboolean ph_util_all_whitespace(const gchar *text);
gint ph_util_strcmp_full(gconstpointer a,
                         gconstpointer b,
                         gpointer data);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
