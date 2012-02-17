/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

/* Includes {{{1 */

#include "ph-config.h"
#include "ph-map-provider.h"

/* Forward declarations {{{1 */

static gchar *ph_map_provider_get_quadkey(gint zoom, glong x, glong y,
                                          const gchar *quadrants);

/* Conversion {{{1 */

/*
 * Create a new map provider information structure based on the information in
 * the list store.
 */
PHMapProvider *
ph_map_provider_new_from_list(GtkListStore *store,
                              GtkTreeIter *iter)
{
    PHMapProvider *result = g_new(PHMapProvider, 1);

    g_return_val_if_fail(store != NULL && GTK_IS_LIST_STORE(store), NULL);
    g_return_val_if_fail(iter != NULL, NULL);

    gtk_tree_model_get(GTK_TREE_MODEL(store), iter,
            PH_MAP_PROVIDER_COLUMN_NAME, &result->name,
            PH_MAP_PROVIDER_COLUMN_PREDEFINED, &result->predefined,
            PH_MAP_PROVIDER_COLUMN_URL, &result->url,
            PH_MAP_PROVIDER_COLUMN_TILE_SIZE, &result->tile_size,
            PH_MAP_PROVIDER_COLUMN_ZOOM_MIN, &result->zoom_min,
            PH_MAP_PROVIDER_COLUMN_ZOOM_MAX, &result->zoom_max,
            PH_MAP_PROVIDER_COLUMN_ZOOM_DETAIL, &result->zoom_detail,
            -1);

    return result;
}

/*
 * Fill a list store row with the values from the map provider structure.
 */
void
ph_map_provider_to_list(const PHMapProvider *provider,
                        GtkListStore *store,
                        GtkTreeIter *iter)
{
    g_return_if_fail(provider != NULL);
    g_return_if_fail(store != NULL && GTK_IS_LIST_STORE(store));
    g_return_if_fail(iter != NULL);

    gtk_list_store_set(store, iter,
            PH_MAP_PROVIDER_COLUMN_NAME, provider->name,
            PH_MAP_PROVIDER_COLUMN_PREDEFINED, provider->predefined,
            PH_MAP_PROVIDER_COLUMN_URL, provider->url,
            PH_MAP_PROVIDER_COLUMN_TILE_SIZE, provider->tile_size,
            PH_MAP_PROVIDER_COLUMN_ZOOM_MIN, provider->zoom_min,
            PH_MAP_PROVIDER_COLUMN_ZOOM_MAX, provider->zoom_max,
            PH_MAP_PROVIDER_COLUMN_ZOOM_DETAIL, provider->zoom_detail,
            -1);
}

/*
 * Fill a single column in a list store row with the matching value from the map
 * provider structure.
 */
void
ph_map_provider_to_list_single(const PHMapProvider *provider,
                               GtkListStore *store,
                               GtkTreeIter *iter,
                               gint column)
{
    g_return_if_fail(provider != NULL);
    g_return_if_fail(store != NULL && GTK_IS_LIST_STORE(store));
    g_return_if_fail(iter != NULL);
    g_return_if_fail(column >= 0 && column < PH_MAP_PROVIDER_COLUMN_COUNT);

    switch (column) {
    case PH_MAP_PROVIDER_COLUMN_NAME:
        gtk_list_store_set(store, iter, column, provider->name, -1);
        break;
    case PH_MAP_PROVIDER_COLUMN_PREDEFINED:
        gtk_list_store_set(store, iter, column, provider->predefined, -1);
        break;
    case PH_MAP_PROVIDER_COLUMN_URL:
        gtk_list_store_set(store, iter, column, provider->url, -1);
        break;
    case PH_MAP_PROVIDER_COLUMN_TILE_SIZE:
        gtk_list_store_set(store, iter, column, provider->tile_size, -1);
        break;
    case PH_MAP_PROVIDER_COLUMN_ZOOM_MIN:
        gtk_list_store_set(store, iter, column, provider->zoom_min, -1);
        break;
    case PH_MAP_PROVIDER_COLUMN_ZOOM_MAX:
        gtk_list_store_set(store, iter, column, provider->zoom_max, -1);
        break;
    case PH_MAP_PROVIDER_COLUMN_ZOOM_DETAIL:
        gtk_list_store_set(store, iter, column, provider->zoom_detail, -1);
        break;
    }
}

/* Memory management {{{1 */

/*
 * Create a deep copy of a map provider.
 */
PHMapProvider *
ph_map_provider_copy(const PHMapProvider *provider)
{
    PHMapProvider *result = g_memdup(provider, sizeof(*provider));

    result->name = g_strdup(provider->name);
    result->url = g_strdup(provider->url);

    return result;
}

/*
 * Free all memory associated with a provider information struct.
 */
void
ph_map_provider_free(PHMapProvider *provider)
{
    if (provider == NULL)
        return;

    g_free(provider->name);
    g_free(provider->url);
    g_free(provider);
}

/* Tile URL handling {{{1 */

/*
 * Fill in the tile URL placeholders to get the location of an actual tile.
 */
gchar *
ph_map_provider_get_tile_url(const PHMapProvider *provider,
                             guint zoom,
                             glong x,
                             glong y)
{
    gchar *input = provider->url;
    GString *result = g_string_new(NULL);
    gchar *tmp;
    gint length;

    while (*input != '\0') {
        if (*input == '$') {
            ++input;
            switch (*input) {
            case 'x':
                /* $x: horizontal tile coordinate */
                g_string_append_printf(result, "%ld", x);
                break;
            case 'y':
                /* $y: vertical tile coordinate */
                g_string_append_printf(result, "%ld", y);
                break;
            case 'z':
                /* $z: zoom level */
                g_string_append_printf(result, "%d", zoom);
                break;
            case '[':
                /* $[abc]: pick one of a, b, c at random */
                tmp = ++input;
                length = 0;

                while (*input != '\0' && *input != ']') {
                    ++length;
                    ++input;
                }
                if (length > 0) {
                    gint choice = g_random_int_range(0, length);
                    g_string_append_c(result, *(tmp + choice));
                }
                break;
            case 'q':
                /* $q: quadkey using 0123 */
                tmp = ph_map_provider_get_quadkey(zoom, x, y, "0123");
                g_string_append(result, tmp);
                g_free(tmp);
                break;
            default:
                g_string_append_c(result, *input);
            }
        }
        else
            g_string_append_c(result, *input);
        ++input;
    }

    return g_string_free(result, FALSE);
}

/*
 * Get the quadrant key for the specified tile, using quadrants[0] for NW, [1]
 * for NE, [2] for SW, [3] for SE.
 */
static gchar *
ph_map_provider_get_quadkey(gint zoom,
                            glong x,
                            glong y,
                            const gchar *quadrants)
{
    gchar *result = g_new(gchar, zoom + 1);

    result[zoom] = '\0';
    while (--zoom >= 0) {
        result[zoom] = quadrants[2 * (y % 2) + (x % 2)];
        x /= 2;
        y /= 2;
    }

    return result;
}

/* Caching {{{1 */

/*
 * Get the cache location for a provider.
 */
gchar *
ph_map_provider_get_cache_dir(const PHMapProvider *provider)
{
    GValue root = { 0 };
    gchar *escaped_name;
    gchar *result;

    g_value_init(&root, G_TYPE_STRING);
    ph_config_get_tile_cache_location(&root);

    escaped_name = g_uri_escape_string(provider->name, " ", TRUE);
    result = g_build_filename(g_value_get_string(&root), escaped_name, NULL);
    g_free(escaped_name);

    g_value_unset(&root);

    return result;
}

/* Boxed type registration {{{1 */

GType
ph_map_provider_get_type() {
    static GType type = 0;

    if (type == 0)
        type = g_boxed_type_register_static("PHMapProvider",
                (GBoxedCopyFunc) ph_map_provider_copy,
                (GBoxedFreeFunc) ph_map_provider_free);

    return type;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
