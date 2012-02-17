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

#include "ph-common.h"
#include "ph-config.h"
#include "ph-util.h"
#include <string.h>

/* Data files {{{1 */

/*
 * Get the path to a data file like a sprite SVG, an UI definition, etc.  The
 * resulting string must be freed by the caller.
 */
gchar *
ph_util_find_data_file(const gchar *directory,
                       const gchar *name)
{
    return g_build_filename(PH_DATA_DIRECTORY, directory, name, NULL);
}

/* Open in browser {{{1 */

/*
 * Invoke the default browser for the specified URI.
 */
void
ph_util_open_in_browser(const gchar *uri)
{
    gchar **argv;
    GValue value = { 0 };

    g_value_init(&value, G_TYPE_STRING);
    ph_config_get_browser(&value);

    argv = g_new(gchar *, 2);
    argv[0] = g_value_dup_string(&value);
    argv[1] = g_strdup(uri);

    g_value_unset(&value);

    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
            NULL, NULL, NULL, NULL);

    g_strfreev(argv);
}

/* String utilities {{{1 */

/*
 * Return TRUE if the given string contains only whitespace characters, FALSE
 * otherwise.
 */
gboolean
ph_util_all_whitespace(const gchar *text)
{
    while (*text != '\0') {
        if (!g_ascii_isspace(*text))
            return FALSE;
        ++text;
    }

    return TRUE;
}

/*
 * strcmp() function with a GCompareDataFunc signature.
 */
gint
ph_util_strcmp_full(gconstpointer a,
                    gconstpointer b,
                    gpointer data)
{
    return strcmp((const gchar *) a, (const gchar *) b);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
