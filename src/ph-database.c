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

#include "ph-database.h"
#include <glib/gi18n.h>

/* Signals {{{1 */

enum {
    PH_DATABASE_SIGNAL_GEOCACHE_UPDATED,
    PH_DATABASE_SIGNAL_BULK_UPDATED,
    PH_DATABASE_SIGNAL_COUNT
};

static guint ph_database_signals[PH_DATABASE_SIGNAL_COUNT];

/* Private data {{{1 */

#define PH_DATABASE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
            PH_TYPE_DATABASE, PHDatabasePrivate))

typedef struct _PHDatabasePrivate {
    gchar *filename;                /* path to the database file */
    sqlite3 *connection;            /* SQLite handle */
} PHDatabasePrivate;

/* Forward declarations {{{1 */

static void ph_database_class_init(PHDatabaseClass *cls);
static void ph_database_init(PHDatabase *database);
static void ph_database_finalize(GObject *obj);

static gint ph_database_get_version(PHDatabase *database, GError **error);
static gboolean ph_database_create(PHDatabase *database, GError **error);
static gboolean ph_database_setup(PHDatabase *database, GError **error);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHDatabase, ph_database, G_TYPE_OBJECT)

/*
 * Class initialization code.
 */
static void
ph_database_class_init(PHDatabaseClass *cls)
{
    GObjectClass *g_obj_cls = G_OBJECT_CLASS(cls);

    g_obj_cls->finalize = ph_database_finalize;

    ph_database_signals[PH_DATABASE_SIGNAL_GEOCACHE_UPDATED] =
        g_signal_new("geocache-updated", PH_TYPE_DATABASE,
                G_SIGNAL_RUN_FIRST,
                G_STRUCT_OFFSET(PHDatabaseClass, geocache_updated),
                NULL, NULL,
                g_cclosure_marshal_VOID__STRING,
                G_TYPE_NONE, 1, G_TYPE_STRING);
    ph_database_signals[PH_DATABASE_SIGNAL_BULK_UPDATED] =
        g_signal_new("bulk-updated", PH_TYPE_DATABASE,
                G_SIGNAL_RUN_FIRST,
                G_STRUCT_OFFSET(PHDatabaseClass, bulk_updated),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

    g_type_class_add_private(cls, sizeof(PHDatabasePrivate));
}

/*
 * Instance initialization code.
 */
static void
ph_database_init(PHDatabase *database)
{
    PHDatabasePrivate *priv = PH_DATABASE_GET_PRIVATE(database);

    database->priv = priv;
}

/*
 * Instance destruction code.
 */
static void
ph_database_finalize(GObject *obj)
{
    PHDatabase *database = PH_DATABASE(obj);

    g_message("Closing database `%s'.", database->priv->filename);

    if (database->priv->filename != NULL)
        g_free(database->priv->filename);
    if (database->priv->connection != NULL)
        sqlite3_close(database->priv->connection);

    if (G_OBJECT_CLASS(ph_database_parent_class)->finalize != NULL)
        G_OBJECT_CLASS(ph_database_parent_class)->finalize(obj);
}

/* Instance creation {{{1 */

/*
 * Open an SQLite database.  If create is set, establish a new one if needed.
 * An empty database will be populated with the schema needed by plastichunt.
 * Returns NULL on error.
 */
PHDatabase *
ph_database_new(const gchar *filename,
                gboolean create,
                GError **error)
{
    PHDatabase *result;
    sqlite3 *connection;
    gint flags = SQLITE_OPEN_READWRITE;
    int rc;

    g_return_val_if_fail(filename != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (create)
        flags |= SQLITE_OPEN_CREATE;

    rc = sqlite3_open_v2(filename, &connection, flags, NULL);
    if (rc == SQLITE_OK) {
        result = g_object_new(PH_TYPE_DATABASE, NULL);
        result->priv->filename = g_strdup(filename);
        result->priv->connection = connection;
        if (ph_database_setup(result, error)) {
            g_message("Opened database `%s'.", filename);
            return result;
        }
        else {
            g_object_unref(result);
            return NULL;
        }
    }
    else {
        g_set_error(error, PH_DATABASE_ERROR, PH_DATABASE_ERROR_OPEN,
                _("Could not open database `%s': %s"), filename,
                sqlite3_errmsg(connection));
        sqlite3_close(connection);
        return NULL;
    }
}

/* Accessors {{{1 */

/*
 * Get the path to the database file.  The returned string is owned by
 * PHDatabase and must not be modified or freed.
 */
const gchar *
ph_database_get_filename(PHDatabase *database)
{
    g_return_val_if_fail(database != NULL && PH_IS_DATABASE(database), NULL);

    return database->priv->filename;
}

/* Schema version handling {{{1 */

#define PH_DATABASE_CURRENT_VERSION 1

/*
 * Read the schema version from the db_info table.  If the database is empty,
 * create db_info and return 0, which is the placeholder value for "no schema
 * present".  Returns -1 on error.
 */
static gint
ph_database_get_version(PHDatabase *database,
                        GError **error)
{
    sqlite3_stmt *stmt;
    gboolean success;
    gint rc, result = -1;

    g_return_val_if_fail(database != NULL && PH_IS_DATABASE(database), -1);
    g_return_val_if_fail(error == NULL || *error == NULL, -1);

    stmt = ph_database_prepare(database, "SELECT schema_version FROM db_info",
            NULL);
    if (stmt == NULL) {
        /* assume that the db_info table does not exist, create it */
        success = ph_database_exec(database,
                "CREATE TABLE db_info (schema_version INTEGER)", error);
        if (!success)
            return -1;
        success = ph_database_exec(database,
                "INSERT INTO db_info VALUES (0)", error);
        return success ? 0 : -1;
    }

    rc = ph_database_step(database, stmt, error);
    if (rc == SQLITE_DONE)
        g_set_error(error, PH_DATABASE_ERROR, PH_DATABASE_ERROR_SCHEMA,
                _("Missing db_info row in `%s'"), database->priv->filename);
    else if (rc == SQLITE_ROW)
        result = sqlite3_column_int(stmt, 0);
    (void) sqlite3_finalize(stmt);

    return result;
}

/* Schema creation {{{1 */

/*
 * Prepare the database used by plastichunt by creating the necessary tables
 * and indices.  Returns FALSE on error.
 */
static gboolean
ph_database_create(PHDatabase *database,
                   GError **error)
{
    gchar *queries[] = {
        "CREATE TABLE geocaches (id TEXT PRIMARY KEY, name TEXT, creator TEXT, "
            "owner TEXT, type TINYINT, size TINYINT, difficulty TINYINT, "
            "terrain TINYINT, attributes TEXT, summary_html BOOLEAN, "
            "summary TEXT, description_html BOOLEAN, description TEXT, "
            "hint TEXT, logged BOOLEAN, archived BOOLEAN, available BOOLEAN)",
        "CREATE TABLE geocache_notes (id TEXT PRIMARY KEY, "
            "found BOOLEAN, note TEXT)",
        "CREATE VIEW geocaches_full AS SELECT geocaches.*, "
            "geocache_notes.found, geocache_notes.note FROM geocaches "
            "LEFT JOIN geocache_notes USING (id)",
        "CREATE TABLE waypoints (id TEXT PRIMARY KEY, geocache_id TEXT, "
            "name TEXT, placed INTEGER, type TINYINT, url TEXT, summary TEXT, "
            "description TEXT, latitude INTEGER, longitude INTEGER)",
        "CREATE TABLE waypoint_notes (id TEXT PRIMARY KEY, "
            "new_latitude INTEGER, new_longitude INTEGER)",
        "CREATE VIEW waypoints_full AS SELECT waypoints.*, "
            "waypoint_notes.new_latitude, waypoint_notes.new_longitude "
            "FROM waypoints LEFT JOIN waypoint_notes USING (id)",
        "CREATE INDEX waypoints_by_geocache ON waypoints (geocache_id)",
        "CREATE TABLE logs (id INTEGER, geocache_id TEXT, type TINYINT, "
            "logger TEXT, logged INTEGER, details TEXT, "
            "PRIMARY KEY (id, geocache_id))",
        "CREATE TABLE trackables (id TEXT PRIMARY KEY, name TEXT, "
            "geocache_id TEXT)",
        "CREATE INDEX trackables_by_geocache ON trackables (geocache_id)",
        NULL
    }, **query;
    gboolean success;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    for (query = &queries[0]; *query != NULL; ++query) {
        success = ph_database_exec(database, *query, error);
        if (!success)
            return FALSE;
    }

    return ph_database_exec(database,
            "UPDATE db_info SET schema_version = "
            G_STRINGIFY(PH_DATABASE_CURRENT_VERSION),
            error);
}

/*
 * Check whether the database is empty.  If so, let ph_database_create()
 * prepare the schema.  Returns FALSE on error.
 */
static gboolean
ph_database_setup(PHDatabase *database,
                  GError **error)
{
    gint version;
    gboolean success = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (!ph_database_begin(database, error))
        return FALSE;
    version = ph_database_get_version(database, error);
    if (version == 0)
        success = ph_database_create(database, error);
    else if (version == PH_DATABASE_CURRENT_VERSION)
        success = TRUE;
    else if (version != -1)
        g_set_error(error, PH_DATABASE_ERROR, PH_DATABASE_ERROR_SCHEMA,
                _("Unknown database schema version in `%s': %d (highest "
                    "supported is %d)"), database->priv->filename,
                version, PH_DATABASE_CURRENT_VERSION);

    if (success)
        success = ph_database_commit(database, error);
    else
        (void) ph_database_rollback(database, NULL);

    return success;
}

/* Transactions {{{1 */

/*
 * Start an SQLite transaction.  Returns FALSE on error.
 */
gboolean
ph_database_begin(PHDatabase *database,
                  GError **error)
{
    g_return_val_if_fail(database != NULL && PH_IS_DATABASE(database), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    return ph_database_exec(database, "BEGIN", error);
}

/*
 * End an SQLite transaction and commit changes.  Returns FALSE on error.
 */
gboolean
ph_database_commit(PHDatabase *database,
                   GError **error)
{
    g_return_val_if_fail(database != NULL && PH_IS_DATABASE(database), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    return ph_database_exec(database, "COMMIT", error);
}

/*
 * Like ph_database_commit(), but also emit the "bulk-updated" signal on
 * successful completion.
 */
gboolean
ph_database_commit_notify(PHDatabase *database,
                          GError **error)
{
    gboolean success = ph_database_commit(database, error);

    if (success)
        ph_database_notify_bulk_update(database);

    return success;
}

/*
 * End an SQLite transaction and undo changes.  Returns FALSE on error.
 */
gboolean
ph_database_rollback(PHDatabase *database,
                     GError **error)
{
    g_return_val_if_fail(database != NULL && PH_IS_DATABASE(database), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    return ph_database_exec(database, "ROLLBACK", error);
}

/* Prepared statements {{{1 */

/*
 * Prepare an SQLite statement.  Returns NULL on error.
 */
sqlite3_stmt *
ph_database_prepare(PHDatabase *database,
                    const gchar *query,
                    GError **error)
{
    sqlite3_stmt *result;
    int rc;

    g_return_val_if_fail(database != NULL && PH_IS_DATABASE(database), NULL);
    g_return_val_if_fail(query != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    g_debug("Preparing SQL query: %s", query);

    rc = sqlite3_prepare_v2(database->priv->connection, query,
            -1, &result, NULL);
    if (rc != SQLITE_OK)
        g_set_error(error, PH_DATABASE_ERROR, PH_DATABASE_ERROR_SQL,
                _("Could not prepare SQL statement `%s': %s"), query,
                sqlite3_errmsg(database->priv->connection));
    return result;
}

/*
 * Move to the next row in the result set.  Returns SQLITE_DONE if no more
 * data is available, SQLITE_ROW if there is, and the appropriate SQLite
 * result code on error.
 */
gint
ph_database_step(PHDatabase *database,
                 sqlite3_stmt *stmt,
                 GError **error)
{
    int rc;

    g_return_val_if_fail(database != NULL && PH_IS_DATABASE(database),
            SQLITE_ERROR);
    g_return_val_if_fail(stmt != NULL, SQLITE_ERROR);
    g_return_val_if_fail(error == NULL || *error == NULL, SQLITE_ERROR);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW)
        g_set_error(error, PH_DATABASE_ERROR, PH_DATABASE_ERROR_STEP,
                _("Could not get next row in result set: %s"),
                sqlite3_errmsg(database->priv->connection));
    return rc;
}

/* Statement execution {{{1 */

/*
 * Execute an SQL statement without obtaining a result set.  This is useful
 * for queries like INSERT, UPDATE or DELETE.  Returns FALSE on error.
 */
gboolean
ph_database_exec(PHDatabase *database,
                 const gchar *query,
                 GError **error)
{
    int rc;
    gchar *errmsg;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (query == NULL) {
        g_set_error_literal(error, PH_DATABASE_ERROR, PH_DATABASE_ERROR_SQL,
                _("Trying to execute a NULL query (out of memory?)"));
    }

    g_debug("Executing SQL statement: %s", query);

    rc = sqlite3_exec(database->priv->connection, query, NULL, NULL, &errmsg);
    if (rc == SQLITE_OK)
        return TRUE;
    else {
        g_set_error(error, PH_DATABASE_ERROR, PH_DATABASE_ERROR_SQL,
                _("SQL statement `%s' failed: %s"), query, errmsg);
        sqlite3_free(errmsg);
        return FALSE;
    }
}

/* Update notification {{{1 */

/*
 * Emit the "geocache-updated" signal for the geocache with the given ID.
 */
void
ph_database_notify_geocache_update(PHDatabase *database,
                                   const gchar *id)
{
    g_return_if_fail(database != NULL && PH_IS_DATABASE(database));
    g_return_if_fail(id != NULL);

    g_signal_emit(database,
            ph_database_signals[PH_DATABASE_SIGNAL_GEOCACHE_UPDATED],
            0, id);
}

/*
 * Emit the "bulk-updated" signal to announce potentially big changes to the
 * database.
 */
void
ph_database_notify_bulk_update(PHDatabase *database)
{
    g_return_if_fail(database != NULL && PH_IS_DATABASE(database));

    g_signal_emit(database,
            ph_database_signals[PH_DATABASE_SIGNAL_BULK_UPDATED],
            0);
}

/* Table names {{{1 */

/*
 * Names of the default tables in ascending order of PHDatabaseTable values.
 */
static const gchar *ph_database_table_names[] = {
    "geocaches", "geocache_notes", "geocaches_full",
    "waypoints", "waypoint_notes", "waypoints_full",
    "logs", "trackables"
};

/*
 * Get the name of a certain table.
 */
const gchar *
ph_database_table_name(PHDatabaseTable table)
{
    gint index = 0;

    while ((table & 1) == 0) {
        table >>= 1;
        ++index;
    }

    return ph_database_table_names[index];
}

/* Error reporting {{{1 */

/*
 * If there was an error on database, copy the error message to the GError
 * parameter and return FALSE.  Otherwise, do nothing and return TRUE.
 */
gboolean
ph_database_error(PHDatabase *database,
                  GError **error)
{
    g_return_val_if_fail(database != NULL && PH_IS_DATABASE(database), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (sqlite3_errcode(database->priv->connection) == SQLITE_OK)
        return TRUE;
    else {
        g_set_error(error, PH_DATABASE_ERROR, PH_DATABASE_ERROR_FAILED,
                _("SQLite error: %s"),
                sqlite3_errmsg(database->priv->connection));
        return FALSE;
    }
}

/*
 * Unique identifier for database errors.
 */
GQuark
ph_database_error_quark()
{
    return g_quark_from_static_string("ph-database-error");
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
