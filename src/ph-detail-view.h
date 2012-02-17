/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_DETAIL_VIEW_H
#define PH_DETAIL_VIEW_H

/* Includes {{{1 */

#include <gtk/gtk.h>
#include "ph-geocache.h"
#include "ph-waypoint.h"
#include "ph-log.h"
#include "ph-trackable.h"

/* GObject boilerplate {{{1 */

#define PH_TYPE_DETAIL_VIEW \
    (ph_detail_view_get_type())
#define PH_DETAIL_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_DETAIL_VIEW, PHDetailView))
#define PH_IS_DETAIL_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_DETAIL_VIEW))
#define PH_DETAIL_VIEW_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST((cls), PH_TYPE_DETAIL_VIEW, PHDetailViewClass))
#define PH_IS_DETAIL_VIEW_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE((cls), PH_TYPE_DETAIL_VIEW))
#define PH_DETAIL_VIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_DETAIL_VIEW, PHDetailViewClass))

GType ph_detail_view_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHDetailView PHDetailView;
typedef struct _PHDetailViewClass PHDetailViewClass;
typedef struct _PHDetailViewPrivate PHDetailViewPrivate;

struct _PHDetailView {
    GtkVBox parent;
    PHDetailViewPrivate *priv;
};

struct _PHDetailViewClass {
    GtkVBoxClass parent_class;

    void (*closed)(PHDetailView *view);
};

/* Public interface {{{1 */

GtkWidget *ph_detail_view_new(PHDatabase *database,
                              const gchar *geocache_id,
                              GError **error);

GtkWidget *ph_detail_view_get_label(PHDetailView *view);
const gchar *ph_detail_view_get_geocache_id(PHDetailView *view);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
