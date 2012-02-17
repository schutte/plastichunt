/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_DATABASE_H
#define PH_DATABASE_H

/* Includes {{{1 */

#include <glib-object.h>
#include <glib.h>
#include <sqlite3.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_DATABASE (ph_database_get_type())
#define PH_DATABASE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
            PH_TYPE_DATABASE, PHDatabase))
#define PH_IS_DATABASE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
            PH_TYPE_DATABASE))
#define PH_DATABASE_CLASS(cls) (G_TYPE_CHECK_CLASS_CAST((cls), \
            PH_TYPE_DATABASE, PHDatabaseClass))
#define PH_IS_DATABASE_CLASS(cls) (G_TYPE_CHECK_CLASS_TYPE((cls), \
            PH_TYPE_DATABASE))
#define PH_DATABASE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            PH_TYPE_DATABASE, PHDatabaseClass))

GType ph_database_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHDatabase PHDatabase;
typedef struct _PHDatabaseClass PHDatabaseClass;
typedef struct _PHDatabasePrivate PHDatabasePrivate;

struct _PHDatabase {
    GObject parent;
    PHDatabasePrivate *priv;
};

struct _PHDatabaseClass {
    GObjectClass parent_class;

    /* signals */
    void (*geocache_updated)(PHDatabase *database, gchar *id);
    void (*bulk_updated)(PHDatabase *database);
};

/* Tables and views {{{1 */

typedef enum _PHDatabaseTable {
    PH_DATABASE_TABLE_GEOCACHES = 0x01,
    PH_DATABASE_TABLE_GEOCACHE_NOTES = 0x02,
    PH_DATABASE_TABLE_GEOCACHES_FULL = 0x04,
    PH_DATABASE_TABLE_WAYPOINTS = 0x08,
    PH_DATABASE_TABLE_WAYPOINT_NOTES = 0x10,
    PH_DATABASE_TABLE_WAYPOINTS_FULL = 0x20,
    PH_DATABASE_TABLE_LOGS = 0x40,
    PH_DATABASE_TABLE_TRACKABLES = 0x80
} PHDatabaseTable;

/* Public interface {{{1 */

PHDatabase *ph_database_new(const gchar *filename,
                            gboolean create,
                            GError **error);
const gchar *ph_database_get_filename(PHDatabase *database);

gboolean ph_database_begin(PHDatabase *database,
                           GError **error);
gboolean ph_database_commit(PHDatabase *database,
                            GError **error);
gboolean ph_database_commit_notify(PHDatabase *database,
                                   GError **error);
gboolean ph_database_rollback(PHDatabase *database,
                              GError **error);

void ph_database_notify_geocache_update(PHDatabase *database,
                                        const gchar *id);
void ph_database_notify_bulk_update(PHDatabase *database);

sqlite3_stmt *ph_database_prepare(PHDatabase *database,
                                  const gchar *query,
                                  GError **error);
gint ph_database_step(PHDatabase *database,
                      sqlite3_stmt *stmt,
                      GError **error);
gboolean ph_database_exec(PHDatabase *database,
                          const gchar *query,
                          GError **error);

const gchar *ph_database_table_name(PHDatabaseTable table);

/* Error reporting {{{1 */

gboolean ph_database_error(PHDatabase *database,
                           GError **error);

#define PH_DATABASE_ERROR ph_database_error_quark()
GQuark ph_database_error_quark();
typedef enum {
    PH_DATABASE_ERROR_OPEN,
    PH_DATABASE_ERROR_SQL,
    PH_DATABASE_ERROR_STEP,
    PH_DATABASE_ERROR_SCHEMA,
    PH_DATABASE_ERROR_INCONSISTENT,
    PH_DATABASE_ERROR_FAILED
} PHDatabaseError;

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
