/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_ZOOM_ACTION_H
#define PH_ZOOM_ACTION_H

/* Includes {{{1 */

#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_ZOOM_ACTION (ph_zoom_action_get_type())
#define PH_ZOOM_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_ZOOM_ACTION, PHZoomAction))
#define PH_ZOOM_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), PH_TYPE_ZOOM_ACTION, PHZoomActionClass))
#define PH_IS_ZOOM_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_ZOOM_ACTION))
#define PH_IS_ZOOM_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), PH_TYPE_ZOOM_ACTION))
#define PH_ZOOM_ACTION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_ZOOM_ACTION, PHZoomActionClass))

GType ph_zoom_action_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHZoomAction PHZoomAction;
typedef struct _PHZoomActionClass PHZoomActionClass;
typedef struct _PHZoomActionPrivate PHZoomActionPrivate;

struct _PHZoomAction {
    GtkAction parent;
    PHZoomActionPrivate *priv;
};

struct _PHZoomActionClass {
    GtkActionClass parent_class;
};

/* Public interface {{{1 */

GtkAction *ph_zoom_action_new(const gchar *name,
                              GtkAdjustment *adjustment);
void ph_zoom_action_set_adjustment(PHZoomAction *action,
                                   GtkAdjustment *adjustment);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
