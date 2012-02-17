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

#include "ph-log.h"

/* Database retrieval and storage {{{1 */

/*
 * Load the list of logs stored for a given geocache, sorted by descending
 * timestamps.
 */
gboolean
ph_logs_load_by_geocache_id(GList **list,
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

    query = sqlite3_mprintf("SELECT id, geocache_id, type, logger, "
            "logged, details FROM logs WHERE geocache_id = %Q "
            "ORDER BY logged ASC",  /* building list in reverse */
            id);
    stmt = ph_database_prepare(database, query, error);
    sqlite3_free(query);
    if (stmt == NULL)
        return FALSE;

    while ((status = ph_database_step(database, stmt, error)) == SQLITE_ROW) {
        PHLog *current = g_new(PHLog, 1);
        current->id = sqlite3_column_int(stmt, 0);
        current->geocache_id = g_strdup((const gchar *)
                sqlite3_column_text(stmt, 1));
        current->type = (PHLogType) sqlite3_column_int(stmt, 2);
        current->logger = g_strdup((const gchar *)
                sqlite3_column_text(stmt, 3));
        current->logged = sqlite3_column_int64(stmt, 4);
        current->details = g_strdup((const gchar *)
                sqlite3_column_text(stmt, 5));
        result = g_list_prepend(result, current);
    }

    (void) sqlite3_finalize(stmt);

    if (status == SQLITE_DONE) {
        *list = result;
        return TRUE;
    }
    else {
        ph_logs_free(result);
        return FALSE;
    }
}

/*
 * Store the given log in the database.  Uses an INSERT OR REPLACE statement
 * to avoid duplicates, so the (id, geocache_id) tuple must be unique.
 * Returns FALSE on error.
 */
gboolean
ph_log_store(const PHLog *log,
             PHDatabase *database,
             GError **error)
{
    char *query;
    gboolean success;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    query = sqlite3_mprintf("INSERT OR REPLACE INTO logs "
            "(id, geocache_id, type, logger, logged, details) "
            "VALUES (%d, %Q, %d, %Q, %ld, %Q)",
            log->id, log->geocache_id, log->type, log->logger, log->logged,
            log->details);
    success = ph_database_exec(database, query, error);
    sqlite3_free(query);

    return success;
}

/* Memory management {{{1 */

/*
 * Free the memory associated with a single log.  Safe no-op if called with
 * NULL.
 */
void
ph_log_free(PHLog *log)
{
    if (log == NULL)
        return;

    g_free(log->geocache_id);
    g_free(log->logger);
    g_free(log->details);
    g_free(log);
}

/*
 * Free an entire list of logs as obtained with ph_logs_load_by_geocache_id().
 */
void
ph_logs_free(GList *logs)
{
    g_list_free_full(logs, (GDestroyNotify) ph_log_free);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
