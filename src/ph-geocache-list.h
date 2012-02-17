/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_GEOCACHE_LIST_H
#define PH_GEOCACHE_LIST_H

/* Includes {{{1 */

#include "ph-query.h"
#include "ph-geocache.h"
#include "ph-database.h"
#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_GEOCACHE_LIST (ph_geocache_list_get_type())
#define PH_GEOCACHE_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_GEOCACHE_LIST, PHGeocacheList))
#define PH_IS_GEOCACHE_LIST(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_GEOCACHE_LIST))
#define PH_GEOCACHE_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), PH_TYPE_GEOCACHE_LIST, \
                             PHGeocacheListClass))
#define PH_IS_GEOCACHE_LIST_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), PH_TYPE_GEOCACHE_LIST))
#define PH_GEOCACHE_LIST_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_GEOCACHE_LIST, \
                               PHGeocacheListClass))

GType ph_geocache_list_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHGeocacheList PHGeocacheList;
typedef struct _PHGeocacheListClass PHGeocacheListClass;
typedef struct _PHGeocacheListPrivate PHGeocacheListPrivate;

struct _PHGeocacheList {
    GObject parent;
    PHGeocacheListPrivate *priv;
};

struct _PHGeocacheListClass {
    GObjectClass parent_class;

    /* signals */
    void (*updated)(PHGeocacheList *list);
};

/* List structure {{{1 */

/*
 * Columns for the geocache list entries.
 */
enum {
    PH_GEOCACHE_LIST_COLUMN_ID,
    PH_GEOCACHE_LIST_COLUMN_NAME,
    PH_GEOCACHE_LIST_COLUMN_OWNER,
    PH_GEOCACHE_LIST_COLUMN_TYPE,
    PH_GEOCACHE_LIST_COLUMN_SIZE,
    PH_GEOCACHE_LIST_COLUMN_DIFFICULTY,
    PH_GEOCACHE_LIST_COLUMN_TERRAIN,
    PH_GEOCACHE_LIST_COLUMN_LOGGED,
    PH_GEOCACHE_LIST_COLUMN_AVAILABLE,
    PH_GEOCACHE_LIST_COLUMN_ARCHIVED,
    PH_GEOCACHE_LIST_COLUMN_LATITUDE,
    PH_GEOCACHE_LIST_COLUMN_LONGITUDE,
    PH_GEOCACHE_LIST_COLUMN_NOTE,
    PH_GEOCACHE_LIST_COLUMN_FOUND,
    PH_GEOCACHE_LIST_COLUMN_NEW_COORDINATES,
    PH_GEOCACHE_LIST_COLUMN_NEW_LATITUDE,
    PH_GEOCACHE_LIST_COLUMN_NEW_LONGITUDE,
    PH_GEOCACHE_LIST_COLUMN_COUNT
};

/*
 * Record in the internal list.
 */
typedef struct _PHGeocacheListEntry {
    gchar *id;
    gchar *name;
    gchar *owner;
    PHGeocacheType type;
    PHGeocacheSize size;
    guint8 difficulty;
    guint8 terrain;
    gboolean logged;
    gboolean available;
    gboolean archived;
    gint latitude;
    gint longitude;
    gboolean found;
    gboolean note;
    gboolean new_coordinates;
    gint new_latitude;
    gint new_longitude;
} PHGeocacheListEntry;

/* Geographical area {{{1 */

/*
 * Area of interest (geographical coordinates specified in 1/1000s of
 * minutes).
 */
typedef struct _PHGeocacheListRange {
    gint south;
    gint north;
    gint west;
    gint east;
} PHGeocacheListRange;

/* Public interface {{{1 */

PHGeocacheList *ph_geocache_list_new();

void ph_geocache_list_set_database(PHGeocacheList *list,
                                   PHDatabase *database);
PHDatabase *ph_geocache_list_get_database(PHGeocacheList *list);

void ph_geocache_list_set_range(PHGeocacheList *list,
                                const PHGeocacheListRange *range);
void ph_geocache_list_set_global_range(PHGeocacheList *list);
gboolean ph_geocache_list_set_query(PHGeocacheList *list,
                                    const gchar *query,
                                    GError **error);

GtkTreePath *ph_geocache_list_find_by_id(PHGeocacheList *list,
                                         const gchar *geocache_id);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
