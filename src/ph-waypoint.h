/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_WAYPOINT_H
#define PH_WAYPOINT_H

/* Includes {{{1 */

#include "ph-database.h"
#include <glib.h>
#include <glib-object.h>

/* Waypoint types {{{1 */

/*
 * Waypoint types.
 */
typedef enum _PHWaypointType {
    PH_WAYPOINT_TYPE_UNKNOWN,
    PH_WAYPOINT_TYPE_GEOCACHE,
    PH_WAYPOINT_TYPE_TRAILHEAD,
    PH_WAYPOINT_TYPE_REFERENCE,
    PH_WAYPOINT_TYPE_QUESTION,
    PH_WAYPOINT_TYPE_STAGE,
    PH_WAYPOINT_TYPE_FINAL,
    PH_WAYPOINT_TYPE_PARKING,
    PH_WAYPOINT_TYPE_COUNT
} PHWaypointType;

/* Data structures {{{1 */

/*
 * Mutable waypoint information as stored in the "waypoint_notes" table.
 */
typedef struct _PHWaypointNote {
    gchar *id;                  /* waypoint ID or NULL if note wasn't loaded */
    gboolean custom;            /* custom coordinates present? */
    gint new_latitude;          /* custom latitude */
    gint new_longitude;         /* custom longitude */
} PHWaypointNote;

#define PH_TYPE_WAYPOINT_NOTE (ph_waypoint_note_get_type())
GType ph_waypoint_note_get_type();

/*
 * Representation of a row in the "waypoints" table.
 */
typedef struct _PHWaypoint {
    gchar *id;                  /* waypoint code, such as GC123 */
    gchar *geocache_id;         /* parent geocache */
    gchar *name;                /* human-readable waypoint name */
    gchar *url;                 /* location of the listing */
    glong placed;               /* creation date */
    gchar *summary;             /* short description */
    gchar *description;         /* long description */
    PHWaypointType type;        /* purpose of the waypoint */
    gint latitude, longitude;   /* geographic coordinates */
    PHWaypointNote note;
} PHWaypoint;

/* Public interface {{{1 */

gboolean ph_waypoints_load_by_geocache_id(GList **list,
                                          PHDatabase *database,
                                          const gchar *id,
                                          gboolean full,
                                          GError **error);
gboolean ph_waypoint_store(const PHWaypoint *waypoint,
                           PHDatabase *database,
                           GError **error);
void ph_waypoint_free(PHWaypoint *waypoint);
void ph_waypoints_free(GList *list);

PHWaypointNote *ph_waypoint_note_copy(const PHWaypointNote *note);
gboolean ph_waypoint_note_store(const PHWaypointNote *note,
                                PHDatabase *database,
                                GError **error);
void ph_waypoint_note_free(PHWaypointNote *note);

gchar *ph_waypoint_get_geocache_id(const gchar *waypoint_id);
gboolean ph_waypoint_is_primary(const gchar *waypoint_id);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
