/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_WAYPOINT_EDITOR_H
#define PH_WAYPOINT_EDITOR_H

/* Includes {{{1 */

#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_WAYPOINT_EDITOR \
    (ph_waypoint_editor_get_type())
#define PH_WAYPOINT_EDITOR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_WAYPOINT_EDITOR, PHWaypointEditor))
#define PH_IS_WAYPOINT_EDITOR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_WAYPOINT_EDITOR))
#define PH_WAYPOINT_EDITOR_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST((cls), PH_TYPE_WAYPOINT_EDITOR, PHWaypointEditorClass))
#define PH_IS_WAYPOINT_EDITOR_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE((cls), PH_TYPE_WAYPOINT_EDITOR))
#define PH_WAYPOINT_EDITOR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_WAYPOINT_EDITOR, PHWaypointEditorClass))

GType ph_waypoint_editor_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHWaypointEditor PHWaypointEditor;
typedef struct _PHWaypointEditorClass PHWaypointEditorClass;
typedef struct _PHWaypointEditorPrivate PHWaypointEditorPrivate;

struct _PHWaypointEditor {
    GtkTable parent;
    PHWaypointEditorPrivate *priv;
};

struct _PHWaypointEditorClass {
    GtkTableClass parent_class;

    void (*changed)(PHWaypointEditor *editor);
};

/* Public interface {{{1 */

GtkWidget *ph_waypoint_editor_new();

void ph_waypoint_editor_start(PHWaypointEditor *editor,
                              gdouble latitude,
                              gdouble longitude);
void ph_waypoint_editor_end(PHWaypointEditor *editor,
                            gdouble *latitude,
                            gdouble *longitude);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
