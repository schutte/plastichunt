/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_MAP_TILE_CACHE_H
#define PH_MAP_TILE_CACHE_H

/* Includes {{{1 */

#include "ph-map-tile.h"

/* Public interface {{{1 */

gchar *ph_map_tile_cache_get_location(const PHMapTile *tile,
                                      const PHMapProvider *provider);
gboolean ph_map_tile_cache_query(const PHMapTile *tile,
                                 const PHMapProvider *provider);
void ph_map_tile_cache_write(const PHMapTile *tile,
                             const PHMapProvider *provider,
                             const gchar *data,
                             gsize length);

void ph_map_tile_cache_restart();

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
