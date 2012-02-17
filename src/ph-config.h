/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_CONFIG_H
#define PH_CONFIG_H

/* Includes {{{1 */

#include "ph-geocache.h"
#include <gtk/gtk.h>

/* Input and output {{{1 */

gboolean ph_config_init(GError **error);
gboolean ph_config_save(GError **error);

/* Accessors {{{1 */

void ph_config_get_browser(GValue *value);
void ph_config_set_browser(const GValue *value);

GtkListStore *ph_config_get_map_providers();
void ph_config_set_map_provider(GtkTreeIter *iter,
                                guint row, gint column,
                                const GValue *value);
gint ph_config_add_map_provider(const gchar *name);
gboolean ph_config_remove_map_provider(GtkTreeIter *iter,
                                       guint row);

void ph_config_get_tile_cache_enabled(GValue *value);
void ph_config_set_tile_cache_enabled(const GValue *value);
void ph_config_get_tile_cache_location(GValue *value);
void ph_config_set_tile_cache_location(const GValue *value);
void ph_config_get_max_tile_age(GValue *value);
void ph_config_set_max_tile_age(const GValue *value);
void ph_config_get_max_tile_cache_size(GValue *value);
void ph_config_set_max_tile_cache_size(const GValue *value);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
