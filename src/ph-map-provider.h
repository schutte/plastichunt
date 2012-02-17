/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_MAP_PROVIDER_H
#define PH_MAP_PROVIDER_H

/* Includes {{{1 */

#include <gtk/gtk.h>

/* Data structure {{{1 */

/*
 * Information that is necessary to download and display map tiles.
 */
typedef struct _PHMapProvider {
    gchar *name;                /* user-supplied name */
    gboolean predefined;        /* is this one of the default providers? */
    gchar *url;                 /* URL with placeholders */
    guint tile_size;            /* tile size in pixels */
    guint zoom_min;             /* minimum and */
    guint zoom_max;             /* maximum zoom level */
    guint zoom_detail;          /* lowest zoom level considered detailed */
} PHMapProvider;

/*
 * Equivalent columns for the map provider list store used by the configuration
 * framework.
 */
enum {
    PH_MAP_PROVIDER_COLUMN_NAME,
    PH_MAP_PROVIDER_COLUMN_PREDEFINED,
    PH_MAP_PROVIDER_COLUMN_URL,
    PH_MAP_PROVIDER_COLUMN_TILE_SIZE,
    PH_MAP_PROVIDER_COLUMN_ZOOM_MIN,
    PH_MAP_PROVIDER_COLUMN_ZOOM_MAX,
    PH_MAP_PROVIDER_COLUMN_ZOOM_DETAIL,
    PH_MAP_PROVIDER_COLUMN_COUNT
};

#define PH_TYPE_MAP_PROVIDER (ph_map_provider_get_type())

GType ph_map_provider_get_type();

/* Public interface {{{1 */

PHMapProvider *ph_map_provider_new_from_list(GtkListStore *store,
                                             GtkTreeIter *iter);
void ph_map_provider_to_list(const PHMapProvider *provider,
                             GtkListStore *store,
                             GtkTreeIter *iter);
void ph_map_provider_to_list_single(const PHMapProvider *provider,
                                    GtkListStore *store,
                                    GtkTreeIter *iter,
                                    gint column);

PHMapProvider *ph_map_provider_copy(const PHMapProvider *provider);
void ph_map_provider_free(PHMapProvider *provider);

gchar *ph_map_provider_get_tile_url(const PHMapProvider *provider,
                                    guint zoom,
                                    glong x,
                                    glong y);
gchar *ph_map_provider_get_cache_dir(const PHMapProvider *provider);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
