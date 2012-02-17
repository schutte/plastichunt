/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_GEOCACHE_MAP_H
#define PH_GEOCACHE_MAP_H

/* Includes {{{1 */

#include "ph-map.h"
#include "ph-geocache-list.h"

/* GObject boilerplate {{{1 */

#define PH_TYPE_GEOCACHE_MAP (ph_geocache_map_get_type())
#define PH_GEOCACHE_MAP(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_GEOCACHE_MAP, PHGeocacheMap))
#define PH_GEOCACHE_MAP_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), PH_TYPE_GEOCACHE_MAP, PHGeocacheMapClass))
#define PH_IS_GEOCACHE_MAP(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_GEOCACHE_MAP))
#define PH_IS_GEOCACHE_MAP_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), PH_TYPE_GEOCACHE_MAP))
#define PH_GEOCACHE_MAP_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_GEOCACHE_MAP, PHGeocacheMapClass))

GType ph_geocache_map_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHGeocacheMap PHGeocacheMap;
typedef struct _PHGeocacheMapClass PHGeocacheMapClass;
typedef struct _PHGeocacheMapPrivate PHGeocacheMapPrivate;

struct _PHGeocacheMap {
    PHMap parent;
    PHGeocacheMapPrivate *priv;
};

struct _PHGeocacheMapClass {
    PHMapClass parent_class;

    void (*geocache_selected)(PHGeocacheMap *map, GtkTreePath *path);
    void (*geocache_activated)(PHGeocacheMap *map, GtkTreePath *path);
};

/* Public interface {{{1 */

GtkWidget *ph_geocache_map_new();

void ph_geocache_map_set_list(PHGeocacheMap *map,
                              PHGeocacheList *geocache_list);

void ph_geocache_map_select(PHGeocacheMap *map,
                            GtkTreePath *path);
const gchar *ph_geocache_map_get_selection(PHGeocacheMap *map);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
