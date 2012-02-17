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

#include "ph-trackable.h"

/* Database retrieval and storage {{{1 */

/*
 * Load the list of trackables that currently reside in a geocache container,
 * ordered by name.
 */
gboolean
ph_trackables_load_by_geocache_id(GList **list,
                                  PHDatabase *database,
                                  const gchar *id,
                                  GError **error)
{
    char *query;
    sqlite3_stmt *stmt;
    GList *result = NULL;
    gint status;

    g_return_val_if_fail(database != NULL, FALSE);
    g_return_val_if_fail(id != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    query = sqlite3_mprintf(
            "SELECT id, name, geocache_id FROM trackables "
            "WHERE geocache_id = %Q "
            "ORDER BY name DESC",   /* building list in reverse */
            id);
    stmt = ph_database_prepare(database, query, error);
    sqlite3_free(query);
    if (stmt == NULL)
        return FALSE;

    while ((status = ph_database_step(database, stmt, error)) == SQLITE_ROW) {
        PHTrackable *trackable = g_new(PHTrackable, 1);

        trackable->id = g_strdup((const gchar *)
                sqlite3_column_text(stmt, 0));
        trackable->name = g_strdup((const gchar *)
                sqlite3_column_text(stmt, 1));
        trackable->geocache_id = g_strdup((const gchar *)
                sqlite3_column_text(stmt, 2));
        result = g_list_prepend(result, trackable);
    }

    (void) sqlite3_finalize(stmt);

    if (status == SQLITE_DONE) {
        *list = result;
        return TRUE;
    }
    else {
        ph_trackables_free(result);
        return FALSE;
    }
}

/*
 * Store the given trackable in the database.  Uses an INSERT OR REPLACE
 * statement to make sure that the trackable is only ever present in one
 * geocache; this also implies that the IDs must be unique.  Returns FALSE on
 * error.
 */
gboolean
ph_trackable_store(const PHTrackable *trackable,
                   PHDatabase *database,
                   GError **error)
{
    char *query;
    gboolean success;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    query = sqlite3_mprintf("INSERT OR REPLACE INTO trackables "
            "(id, name, geocache_id) VALUES (%Q, %Q, %Q)",
            trackable->id, trackable->name, trackable->geocache_id);
    success = ph_database_exec(database, query, error);
    sqlite3_free(query);

    return success;
}

/* Memory management {{{1 */

/*
 * Free the memory associated with a single trackable.  Safe no-op if called
 * with NULL.
 */
void
ph_trackable_free(PHTrackable *trackable)
{
    if (trackable == NULL)
        return;

    g_free(trackable->id);
    g_free(trackable->name);
    g_free(trackable->geocache_id);
    g_free(trackable);
}

/*
 * Free an entire list of trackables as obtained with
 * ph_trackables_load_by_geocache_id().
 */
void
ph_trackables_free(GList *list)
{
    g_list_free_full(list, (GDestroyNotify) ph_trackable_free);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
