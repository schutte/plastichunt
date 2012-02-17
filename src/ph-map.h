/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_MAP_H
#define PH_MAP_H

/* Includes {{{1 */

#include "ph-map-provider.h"
#include <gtk/gtk.h>
#include <cairo.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_MAP (ph_map_get_type())
#define PH_MAP(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_MAP, PHMap))
#define PH_MAP_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), PH_TYPE_MAP, PHMapClass))
#define PH_IS_MAP(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_MAP))
#define PH_IS_MAP_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), PH_TYPE_MAP))
#define PH_MAP_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_MAP, PHMapClass))

GType ph_map_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHMap PHMap;
typedef struct _PHMapClass PHMapClass;
typedef struct _PHMapPrivate PHMapPrivate;

struct _PHMap {
    GtkWidget widget;
    PHMapPrivate *priv;
};

struct _PHMapClass {
    GtkWidgetClass parent_class;

    void (*viewport_changed)(PHMap *map);
    void (*clicked)(PHMap *map, GdkEventButton *event);
};

/* Boxed types {{{1 */

#define PH_TYPE_MAP_POINT (ph_map_point_get_type())
#define PH_TYPE_MAP_REGION (ph_map_region_get_type())

typedef struct _PHMapPoint PHMapPoint;
typedef struct _PHMapRegion PHMapRegion;

/*
 * Point in tile coordinates.
 */
struct _PHMapPoint {
    gdouble x, y;
};

GType ph_map_point_get_type();
PHMapPoint *ph_map_point_copy(const PHMapPoint *point);
void ph_map_point_free(PHMapPoint *point);

/*
 * Region in tile coordinates.
 */
struct _PHMapRegion {
    gdouble x1, y1;
    gdouble x2, y2;
};

GType ph_map_region_get_type();
PHMapRegion *ph_map_region_copy(const PHMapRegion *region);
void ph_map_region_free(PHMapRegion *region);

/* Public interface {{{1 */

GtkWidget *ph_map_new();

void ph_map_set_provider(PHMap *map,
                         const PHMapProvider *provider);
PHMapProvider *ph_map_get_provider(PHMap *map);

void ph_map_set_coordinates(PHMap *map,
                            gdouble x,
                            gdouble y);
void ph_map_set_lonlat(PHMap *map,
                       gdouble longitude,
                       gdouble latitude);
void ph_map_set_center(PHMap *map,
                       const PHMapPoint *center);

void ph_map_get_center(PHMap *map,
                       PHMapPoint *center);
void ph_map_get_viewport(PHMap *map,
                         PHMapRegion *viewport);
guint ph_map_get_zoom(PHMap *map);
GtkAdjustment *ph_map_get_zoom_adjustment(PHMap *map);

gdouble ph_map_longitude_to_x(PHMap *map,
                              gdouble longitude);
gdouble ph_map_latitude_to_y(PHMap *map,
                             gdouble latitude);
gdouble ph_map_x_to_longitude(PHMap *map,
                              gdouble x);
gdouble ph_map_y_to_latitude(PHMap *map,
                             gdouble y);

void ph_map_refresh(PHMap *map);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
