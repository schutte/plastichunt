/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_MAP_TILE_H
#define PH_MAP_TILE_H

/* Includes {{{1 */

#include "ph-map.h"
#include "ph-map-provider.h"
#include <glib.h>

/* Data types {{{1 */

/*
 * Coordinates of a map tile.
 */
typedef struct _PHMapTile {
    guint zoom;
    glong x, y;
    gboolean cached;
} PHMapTile;

/*
 * Download queue.
 */
typedef struct _PHMapTileQueue PHMapTileQueue;

/*
 * Callback for received map tiles.
 */
typedef void (*PHMapTileCallback)(PHMapTileQueue *queue,
                                  const PHMapTile *tile,
                                  GdkPixbuf *pixbuf,
                                  gpointer user_data);

/* Public interface {{{1 */

PHMapTileQueue *ph_map_tile_queue_new();
PHMapTileQueue *ph_map_tile_queue_ref(PHMapTileQueue *queue);
void ph_map_tile_queue_unref(PHMapTileQueue *queue);

void ph_map_tile_queue_set_provider(PHMapTileQueue *queue,
                                    const PHMapProvider *provider);

void ph_map_tile_queue_begin_add(PHMapTileQueue *queue,
                                 const PHMapRegion *region,
                                 guint zoom);
void ph_map_tile_queue_add(PHMapTileQueue *queue,
                           guint zoom,
                           glong x, glong y);
void ph_map_tile_queue_end_add(PHMapTileQueue *queue,
                               const PHMapPoint *center,
                               PHMapTileCallback callback,
                               gpointer user_data);

void ph_map_tile_wrap(PHMapTile *dest,
                      const PHMapTile *source);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
