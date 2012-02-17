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

#include "ph-waypoint.h"
#include <glib/gi18n.h>

/* Forward declarations {{{1 */

static void ph_waypoint_from_row(PHWaypoint *waypoint, sqlite3_stmt *stmt,
                                 gboolean full);

/* Database retrieval {{{1 */

/*
 * Fill in a PHWaypoint structure according to the current row in the
 * "waypoints" table.
 */
static void
ph_waypoint_from_row(PHWaypoint *waypoint,
                     sqlite3_stmt *stmt,
                     gboolean full)
{
    waypoint->id = g_strdup((const gchar *) sqlite3_column_text(stmt, 0));
    waypoint->geocache_id = g_strdup((const gchar *)
            sqlite3_column_text(stmt, 1));
    waypoint->name = g_strdup((const gchar *) sqlite3_column_text(stmt, 2));
    waypoint->placed = (glong) sqlite3_column_int64(stmt, 3);
    waypoint->type = sqlite3_column_int(stmt, 4);
    waypoint->url = g_strdup((const gchar *) sqlite3_column_text(stmt, 5));
    waypoint->summary = g_strdup((const gchar *) sqlite3_column_text(stmt, 6));
    waypoint->description = g_strdup((const gchar *)
            sqlite3_column_text(stmt, 7));
    waypoint->latitude = sqlite3_column_int(stmt, 8);
    waypoint->longitude = sqlite3_column_int(stmt, 9);

    if (full) {
        waypoint->note.id = waypoint->id;
        waypoint->note.custom = FALSE;
        if (sqlite3_column_type(stmt, 10) == SQLITE_NULL)
            waypoint->note.new_latitude = waypoint->latitude;
        else {
            waypoint->note.new_latitude = sqlite3_column_int(stmt, 10);
            waypoint->note.custom = TRUE;
        }
        if (sqlite3_column_type(stmt, 11) == SQLITE_NULL)
            waypoint->note.new_longitude = waypoint->longitude;
        else {
            waypoint->note.new_longitude = sqlite3_column_int(stmt, 11);
            waypoint->note.custom = TRUE;
        }
    }
    else
        memset(&waypoint->note, 0, sizeof(waypoint->note));
}

/*
 * Load the list of waypoints belonging to a geocache, sorted by ascending
 * type values (but always with the PH_WAYPOINT_TYPE_GEOCACHE at the head of
 * the list).
 */
gboolean
ph_waypoints_load_by_geocache_id(GList **list,
                                 PHDatabase *database,
                                 const gchar *id,
                                 gboolean full,
                                 GError **error)
{
    char *query;
    sqlite3_stmt *stmt;
    GList *result = NULL;
    PHWaypoint *gc_waypoint = NULL;
    gint status;

    g_return_val_if_fail(database != NULL, FALSE);
    g_return_val_if_fail(id != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (full)
        query = sqlite3_mprintf("SELECT id, geocache_id, name, placed, type, "
                "url, summary, description, latitude, longitude, "
                "new_latitude, new_longitude FROM waypoints_full "
                "WHERE geocache_id = %Q ORDER BY type ASC, id ASC", id);
    else
        query = sqlite3_mprintf("SELECT id, geocache_id, name, placed, type, "
                "url, summary, description, latitude, longitude FROM waypoints "
                "WHERE geocache_id = %Q ORDER BY type ASC, id ASC", id);
    stmt = ph_database_prepare(database, query, error);
    sqlite3_free(query);
    if (stmt == NULL)
        return FALSE;

    while ((status = ph_database_step(database, stmt, error)) == SQLITE_ROW) {
        PHWaypoint *current = g_new(PHWaypoint, 1);
        ph_waypoint_from_row(current, stmt, full);
        if (current->type == PH_WAYPOINT_TYPE_GEOCACHE) {
            if (gc_waypoint != NULL) {
                g_set_error(error,
                        PH_DATABASE_ERROR, PH_DATABASE_ERROR_INCONSISTENT,
                        _("Multiple primary waypoints for geocache %s"),
                        id);
                status = SQLITE_ERROR;
                break;
            }
            else
                gc_waypoint = current;
        }
        else
            result = g_list_prepend(result, current);
    }

    (void) sqlite3_finalize(stmt);

    if (status == SQLITE_DONE && gc_waypoint == NULL) {
        g_set_error(error, PH_DATABASE_ERROR, PH_DATABASE_ERROR_INCONSISTENT,
                _("No primary waypoint for geocache %s"), id);
        status = SQLITE_ERROR;
    }

    if (status != SQLITE_DONE) {
        ph_waypoints_free(result);
        return FALSE;
    }

    result = g_list_reverse(result);
    *list = g_list_prepend(result, gc_waypoint);
    return TRUE;
}

/* Memory management {{{1 */

/*
 * Free all memory associated with a waypoint.  Safe no-op if called with
 * NULL.
 */
void
ph_waypoint_free(PHWaypoint *waypoint)
{
    if (waypoint == NULL)
        return;

    g_free(waypoint->id);
    g_free(waypoint->geocache_id);
    g_free(waypoint->name);
    g_free(waypoint->url);
    g_free(waypoint->summary);
    g_free(waypoint->description);
    g_free(waypoint);
}

/*
 * Free an entire list of waypoints.
 */
void
ph_waypoints_free(GList *waypoints)
{
    g_list_free_full(waypoints, (GDestroyNotify) ph_waypoint_free);
}

/*
 * Copy a waypoint note into newly allocated memory.
 */
PHWaypointNote *
ph_waypoint_note_copy(const PHWaypointNote *note)
{
    PHWaypointNote *result = g_new(PHWaypointNote, 1);

    result->id = g_strdup(note->id);
    result->custom = note->custom;
    result->new_latitude = note->new_latitude;
    result->new_longitude = note->new_longitude;

    return result;
}

/*
 * Free memory associated with a waypoint note.  No-op for NULL.
 */
void
ph_waypoint_note_free(PHWaypointNote *note)
{
    if (note == NULL)
        return;

    g_free(note->id);
    g_free(note);
}

/* Database storage {{{1 */

/*
 * Store the given waypoint in the database.  Uses an INSERT OR REPLACE
 * statement to avoid duplicates, so the IDs must be unique.  Returns FALSE
 * on error.
 */
gboolean
ph_waypoint_store(const PHWaypoint *waypoint,
                  PHDatabase *database,
                  GError **error)
{
    char *query;
    gboolean success;
    const gchar *gc_id;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    gc_id = (waypoint->geocache_id == NULL)
        ? waypoint->id : waypoint->geocache_id;

    query = sqlite3_mprintf("INSERT OR REPLACE INTO waypoints "
            "(id, geocache_id, name, placed, type, url, summary, description, "
            "latitude, longitude) "
            "VALUES (%Q, %Q, %Q, %ld, %d, %Q, %Q, %Q, %d, %d)",
            waypoint->id, gc_id, waypoint->name, waypoint->placed,
            waypoint->type, waypoint->url, waypoint->summary,
            waypoint->description, waypoint->latitude, waypoint->longitude);
    success = ph_database_exec(database, query, error);
    sqlite3_free(query);

    return success;
}

/*
 * Store a waypoint note using an INSERT OR REPLACE statement, or delete it if
 * it is empty.  Returns FALSE on error.
 */
gboolean
ph_waypoint_note_store(const PHWaypointNote *note,
                       PHDatabase *database,
                       GError **error)
{
    char *query;
    gboolean success;

    if (note->custom) {
        query = sqlite3_mprintf("INSERT OR REPLACE INTO waypoint_notes "
                "(id, new_latitude, new_longitude) VALUES (%Q, %d, %d)",
                note->id, note->new_latitude, note->new_longitude);
    }
    else {
        query = sqlite3_mprintf("DELETE FROM waypoint_notes WHERE id = %Q",
                note->id);
    }
    success = ph_database_exec(database, query, error);
    sqlite3_free(query);

    return success;
}

/* Utility functions {{{1 */

/*
 * Obtain the ID of the geocache a waypoint belongs to.
 */
gchar *
ph_waypoint_get_geocache_id(const gchar *waypoint_id)
{
    const gchar *comma = strchr(waypoint_id, ',');

    if (comma == NULL)
        return g_strdup(waypoint_id);
    else {
        /* GC,ABCDEF -> GCCDEF */
        gchar *result = g_strdup(comma + 1);
        if (result[0] != '\0' && result[1] != '\0') {
            result[0] = waypoint_id[0];
            result[1] = waypoint_id[1];
        }
        return result;
    }
}

/*
 * Check if the given ID belongs to a primary waypoint (i.e., if it represents
 * the header coordinates of a geocache).
 */
gboolean
ph_waypoint_is_primary(const gchar *waypoint_id)
{
    return (strchr(waypoint_id, ',') == NULL);
}

/* Boxed type registration {{{1 */

GType
ph_waypoint_note_get_type()
{
    static GType type = 0;

    if (type == 0)
        type = g_boxed_type_register_static("PHWaypointNote",
                (GBoxedCopyFunc) ph_waypoint_note_copy,
                (GBoxedFreeFunc) ph_waypoint_note_free);

    return type;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
