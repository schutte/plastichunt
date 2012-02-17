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
#include "ph-util.h"
#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

/* Data structure {{{1 */

/*
 * The application configuration data structure.  This is only used for the
 * static "ph_config" variable.
 */
typedef struct _PHConfig {
    gchar *path;                    /* path to the configuration file */
    GKeyFile *key_file;             /* parsed key-value store */

    GtkListStore *map_providers;    /* map tile sources */
} PHConfig;

/*
 * The global application configuration instance.
 */
static PHConfig *ph_config = NULL;

/* Forward declarations {{{1 */

static void ph_config_set_value(const gchar *group, const gchar *key,
                                const GValue *value);

static void ph_config_read_map_provider(GtkListStore *list, const gchar *group);
gchar *ph_config_get_map_provider_group(const gchar *name);
const gchar *ph_config_get_map_provider_key(gint column);

/* Input and output {{{1 */

/*
 * Open the application configuration file "plastichunt/application-config" in
 * the XDG user configuration directory.
 *
 * On I/O problems, FALSE will be returned and error will be set accordingly.
 */
gboolean
ph_config_init(GError **error)
{
    gchar *filename;
    GKeyFile *key_file = g_key_file_new();
    gboolean success;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (ph_config != NULL)
        /* already initialized */
        return TRUE;

    filename = g_build_filename(g_get_user_config_dir(), g_get_prgname(),
            "application-config", NULL);

    if (g_file_test(filename, G_FILE_TEST_EXISTS))
        success = g_key_file_load_from_file(key_file, filename,
                G_KEY_FILE_KEEP_COMMENTS, error);
    else
        success = TRUE;

    if (success) {
        ph_config = g_new0(PHConfig, 1);
        ph_config->path = filename;
        ph_config->key_file = key_file;
        return TRUE;
    }
    else {
        g_free(filename);
        g_key_file_free(key_file);
        return FALSE;
    }
}

/*
 * Write the application configuration to the same file it was read from.
 *
 * On I/O problems, FALSE will be returned and error will be set accordingly.
 */
gboolean
ph_config_save(GError **error)
{
    gchar *dirname;
    gsize length;
    gboolean success;
    gboolean empty;
    gchar **groups, **current_group;

    g_return_val_if_fail(ph_config != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    /* check if the key file is empty */
    empty = TRUE;
    groups = g_key_file_get_groups(ph_config->key_file, NULL);
    for (current_group = groups; *current_group != NULL; ++current_group) {
        gsize length;

        g_strfreev(g_key_file_get_keys(ph_config->key_file, *current_group,
                    &length, NULL));

        if (length > 0) {
            empty = FALSE;
            break;
        }
    }

    /* first create the directory, if necessary */
    dirname = g_path_get_dirname(ph_config->path);
    success = (g_mkdir_with_parents(dirname, 0755) == 0);
    if (!success)
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                "Could not create the configuration directory `%s': %s.",
                dirname, g_strerror(errno));
    g_free(dirname);

    /* then either write the file or delete it */
    if (success) {
        if (!empty) {
            gchar *data;

            data = g_key_file_to_data(ph_config->key_file, &length, NULL);
            success = g_file_set_contents(ph_config->path, data, length, error);
            g_free(data);
        }
        else if (g_file_test(ph_config->path, G_FILE_TEST_EXISTS)) {
            success = (g_unlink(ph_config->path) == 0);
            if (!success)
                g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                        "Could not remove empty configuration file `%s': %s.",
                        ph_config->path, g_strerror(errno));
        }
    }

    return success;
}

/* Utility functions {{{1 */

/*
 * Set a configuration setting to the content of a GValue.
 */
static void
ph_config_set_value(const gchar *group,
                    const gchar *key,
                    const GValue *value)
{
    switch (G_VALUE_TYPE(value)) {
    case G_TYPE_UINT:
        g_key_file_set_uint64(ph_config->key_file, group, key,
                g_value_get_uint(value));
        break;
    case G_TYPE_STRING:
        g_key_file_set_string(ph_config->key_file, group, key,
                g_value_get_string(value));
        break;
    default:
        g_return_if_reached();
    }
}

/* Browser {{{1 */

/*
 * Retrieve the command name of the preferred browser.
 */
void
ph_config_get_browser(GValue *value)
{
    gchar *browser;

    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(G_VALUE_HOLDS_STRING(value));

    browser = g_key_file_get_string(ph_config->key_file,
            "Programs", "browser", NULL);
    if (browser == NULL)
        g_value_set_static_string(value, "xdg-open");
    else
        g_value_take_string(value, browser);
}

/*
 * Set the command name of the preferred browser.  If value is NULL, the default
 * setting is restored.
 */
void
ph_config_set_browser(const GValue *value)
{
    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(value == NULL || G_VALUE_HOLDS_STRING(value));

    if (value != NULL)
        g_key_file_set_string(ph_config->key_file,
                "Programs", "browser", g_value_get_string(value));
    else
        (void) g_key_file_remove_key(ph_config->key_file,
                "Programs", "browser", NULL);
}

/* Map providers {{{1 */

#define PH_CONFIG_PREFIX_MAP_PROVIDER "Map provider "
#define PH_CONFIG_PREFIX_MAP_PROVIDER_LENGTH 13

/*
 * Default tile servers.
 */
static const PHMapProvider ph_config_default_map_providers[] = {
    { "OpenStreetMap", TRUE,
        "http://$[abc].tile.openstreetmap.org/$z/$x/$y.png",
        256, 0, 18, 14 },
    { "OpenCycleMap", TRUE,
        "http://$[abc].tile.opencyclemap.org/cycle/$z/$x/$y.png",
        256, 0, 17, 14 },
    { "Bing Road", TRUE,
        "http://a$[0123].ortho.tiles.virtualearth.net/tiles/r$q?g=0",
        256, 1, 20, 14 },
    { "Bing Aerial", TRUE,
        "http://a$[0123].ortho.tiles.virtualearth.net/tiles/a$q?g=0",
        256, 1, 20, 14 },
    { "Bing Hybrid", TRUE,
        "http://a$[0123].ortho.tiles.virtualearth.net/tiles/h$q?g=0",
        256, 1, 20, 14 },
};

/*
 * Interpret a [Map provider ...] section from the application configuration
 * file and add it to the given list store.  The settings for default providers,
 * such as OpenStreetMap, are automatically completed.
 */
static void
ph_config_read_map_provider(GtkListStore *list,
                            const gchar *group)
{
    const gchar *name = group + PH_CONFIG_PREFIX_MAP_PROVIDER_LENGTH;
    GtkTreeIter iter;
    PHMapProvider *provider = NULL;
    gsize i;

    gtk_list_store_append(list, &iter);

    for (i = 0; i < G_N_ELEMENTS(ph_config_default_map_providers); ++i) {
        const PHMapProvider *def = &ph_config_default_map_providers[i];
        if (strcmp(def->name, name) == 0)
            provider = ph_map_provider_copy(def);
    }

    if (provider == NULL) {
        /* not one of the default providers */
        provider = g_new0(PHMapProvider, 1);
        provider->name = g_strdup(name);
    }

    if (g_key_file_has_key(ph_config->key_file, group, "url", NULL)) {
        g_free(provider->url);
        provider->url = g_key_file_get_string(ph_config->key_file,
                group, "url", NULL);
    }

    if (g_key_file_has_key(ph_config->key_file, group, "tile-size", NULL))
        provider->tile_size = (guint) g_key_file_get_uint64(ph_config->key_file,
                group, "tile-size", NULL);
    if (g_key_file_has_key(ph_config->key_file, group, "zoom-min", NULL))
        provider->zoom_min = (guint) g_key_file_get_uint64(ph_config->key_file,
                group, "zoom-min", NULL);
    if (g_key_file_has_key(ph_config->key_file, group, "zoom-max", NULL))
        provider->zoom_max = (guint) g_key_file_get_uint64(ph_config->key_file,
                group, "zoom-max", NULL);
    if (g_key_file_has_key(ph_config->key_file, group, "zoom-detail", NULL))
        provider->zoom_detail = (guint) g_key_file_get_uint64(
                ph_config->key_file, group, "zoom-detail", NULL);

    ph_map_provider_to_list(provider, list, &iter);

    ph_map_provider_free(provider);
}

/*
 * Allocate a new string with the key file group name for a map provider with
 * the given name.
 */
gchar *
ph_config_get_map_provider_group(const gchar *name)
{
    return g_strconcat(PH_CONFIG_PREFIX_MAP_PROVIDER, name, NULL);
}

/*
 * Get the configuration key for a column in the map provider list store.  The
 * resulting string is owned by this function and must not be modified or freed.
 */
const gchar *
ph_config_get_map_provider_key(gint column)
{
    switch (column) {
    case PH_MAP_PROVIDER_COLUMN_URL:
        return "url";
    case PH_MAP_PROVIDER_COLUMN_TILE_SIZE:
        return "tile-size";
    case PH_MAP_PROVIDER_COLUMN_ZOOM_MIN:
        return "zoom-min";
    case PH_MAP_PROVIDER_COLUMN_ZOOM_MAX:
        return "zoom-max";
    case PH_MAP_PROVIDER_COLUMN_ZOOM_DETAIL:
        return "zoom-detail";
    default:
        g_return_val_if_reached(NULL);
    }
}

/*
 * Retrieve a list store holding the names, URLs and zoom levels of all
 * configured map providers.  There is no corresponding setter function.
 * Instead, the configuration framework automatically handles changes to the
 * list store.
 */
GtkListStore *
ph_config_get_map_providers()
{
    g_return_val_if_fail(ph_config != NULL, NULL);

    if (ph_config->map_providers == NULL) {
        gchar **groups;
        gsize length;
        gsize i;
        GTree *done;

        ph_config->map_providers = gtk_list_store_new(
                PH_MAP_PROVIDER_COLUMN_COUNT,
                G_TYPE_STRING, G_TYPE_BOOLEAN,          /* name, predefined */
                G_TYPE_STRING, G_TYPE_UINT,             /* url, tile size */
                G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT); /* zoom levels */

        done = g_tree_new_full(ph_util_strcmp_full, NULL, g_free, NULL);

        for (i = 0; i < G_N_ELEMENTS(ph_config_default_map_providers); ++i) {
            const PHMapProvider *provider = &ph_config_default_map_providers[i];
            gchar *group = ph_config_get_map_provider_group(provider->name);

            ph_config_read_map_provider(ph_config->map_providers, group);
            g_tree_insert(done, group, GINT_TO_POINTER(1));
        }

        groups = g_key_file_get_groups(ph_config->key_file, &length);

        for (i = 0; i < length; ++i) {
            if (strncmp(PH_CONFIG_PREFIX_MAP_PROVIDER, groups[i],
                        PH_CONFIG_PREFIX_MAP_PROVIDER_LENGTH) == 0 &&
                    g_tree_lookup(done, groups[i]) == NULL)
                ph_config_read_map_provider(ph_config->map_providers,
                        groups[i]);
        }

        g_strfreev(groups);
        g_tree_destroy(done);
    }

    return ph_config->map_providers;
}

/*
 * Set a column in the map provider table to a new value and mirror the changes
 * in the key file.  If value is NULL, restore the default value.
 */
void
ph_config_set_map_provider(GtkTreeIter *iter,
                           guint row,
                           gint column,
                           const GValue *value)
{
    gchar *name;
    gchar *group;
    const gchar *key;

    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(iter != NULL);
    g_return_if_fail(column >= 0 && column < PH_MAP_PROVIDER_COLUMN_COUNT);

    if (ph_config->map_providers == NULL)
        (void) ph_config_get_map_providers();

    g_return_if_fail(ph_config->map_providers != NULL);

    gtk_tree_model_get(GTK_TREE_MODEL(ph_config->map_providers), iter,
            PH_MAP_PROVIDER_COLUMN_NAME, &name, -1);
    group = ph_config_get_map_provider_group(name);
    key = ph_config_get_map_provider_key(column);

    if (value == NULL && row < G_N_ELEMENTS(ph_config_default_map_providers)) {
        /* restore default value for built-in provider */
        ph_map_provider_to_list_single(&ph_config_default_map_providers[row],
                ph_config->map_providers, iter, column);
        (void) g_key_file_remove_key(ph_config->key_file, group, key, NULL);
    }
    else {
        GValue target = { 0 };
        GType target_type;

        target_type = gtk_tree_model_get_column_type(
                GTK_TREE_MODEL(ph_config->map_providers), column);
        g_value_init(&target, target_type);

        if (value == NULL) {
            /* restore default value for another provider */
            switch (column) {
            case PH_MAP_PROVIDER_COLUMN_URL:
                g_value_set_static_string(&target, "");
                break;
            case PH_MAP_PROVIDER_COLUMN_TILE_SIZE:
                g_value_set_uint(&target, 256);
                break;
            case PH_MAP_PROVIDER_COLUMN_ZOOM_MIN:
                g_value_set_uint(&target, 0);
                break;
            case PH_MAP_PROVIDER_COLUMN_ZOOM_MAX:
                g_value_set_uint(&target, 18);
                break;
            case PH_MAP_PROVIDER_COLUMN_ZOOM_DETAIL:
                g_value_set_uint(&target, 14);
                break;
            default:
                g_warn_if_reached();
            }
        }
        else
            /* actually set to a new value */
            g_value_transform(value, &target);

        gtk_list_store_set_value(ph_config->map_providers, iter,
                column, &target);
        ph_config_set_value(group, key, &target);
        g_value_unset(&target);
    }

    g_free(name);
    g_free(group);
}

/*
 * Append a new map provider with initial values to the list store.  The return
 * value is the index of the new row, or -1 if the row could not be appended
 * because there already is a provider of the same name.
 */
gint
ph_config_add_map_provider(const gchar *name)
{
    GtkTreeIter iter;
    gboolean iter_valid;
    gboolean found;
    guint row;
    gint i;

    g_return_val_if_fail(ph_config != NULL, -1);
    g_return_val_if_fail(name != NULL, -1);

    if (ph_config->map_providers == NULL)
        (void) ph_config_get_map_providers();

    g_return_val_if_fail(ph_config->map_providers != NULL, -1);

    iter_valid = gtk_tree_model_get_iter_first(
            GTK_TREE_MODEL(ph_config->map_providers), &iter);
    found = FALSE;
    while (iter_valid && !found) {
        gchar *existing_name;
        gtk_tree_model_get(GTK_TREE_MODEL(ph_config->map_providers), &iter,
                PH_MAP_PROVIDER_COLUMN_NAME, &existing_name, -1);
        found = (strcmp(name, existing_name) == 0);
        g_free(existing_name);
        iter_valid = gtk_tree_model_iter_next(
                GTK_TREE_MODEL(ph_config->map_providers), &iter);
    }

    if (found)
        return -1;

    gtk_list_store_append(ph_config->map_providers, &iter);
    gtk_list_store_set(ph_config->map_providers, &iter,
            PH_MAP_PROVIDER_COLUMN_NAME, name, -1);

    row = gtk_tree_model_iter_n_children(
            GTK_TREE_MODEL(ph_config->map_providers), NULL) - 1;
    for (i = PH_MAP_PROVIDER_COLUMN_PREDEFINED + 1;
            i < PH_MAP_PROVIDER_COLUMN_COUNT; ++i)
        ph_config_set_map_provider(&iter, row, i, NULL);

    return row;
}

/*
 * Remove a map provider from the list store.  Returns a new row number (e.g.,
 * to replace combobox selections) if the provider was removed, or -1 if it is a
 * built-in default provider.
 */
gint
ph_config_remove_map_provider(GtkTreeIter *iter,
                              guint row)
{
    gchar *name;
    gchar *group;
    gboolean predefined;
    gint result = -1;

    g_return_val_if_fail(ph_config != NULL, FALSE);
    g_return_val_if_fail(iter != NULL, FALSE);

    if (ph_config->map_providers == NULL)
        (void) ph_config_get_map_providers();

    g_return_val_if_fail(ph_config->map_providers != NULL, FALSE);

    gtk_tree_model_get(GTK_TREE_MODEL(ph_config->map_providers), iter,
            PH_MAP_PROVIDER_COLUMN_NAME, &name,
            PH_MAP_PROVIDER_COLUMN_PREDEFINED, &predefined,
            -1);

    if (!predefined) {
        gtk_list_store_remove(ph_config->map_providers, iter);

        group = ph_config_get_map_provider_group(name);
        (void) g_key_file_remove_group(ph_config->key_file, group, NULL);
        g_free(group);

        if (row >= (guint) gtk_tree_model_iter_n_children(
                GTK_TREE_MODEL(ph_config->map_providers), NULL))
            result = row - 1;
        else
            result = row;
    }

    g_free(name);

    return result;
}

/* Map tile cache {{{1 */

/*
 * Does the user want tiles to be stored in the filesystem to reduce network
 * queries?  This is a boolean value.
 */
void
ph_config_get_tile_cache_enabled(GValue *value)
{
    gboolean enabled;
    GError *error = NULL;

    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(G_VALUE_HOLDS_BOOLEAN(value));

    enabled = g_key_file_get_boolean(ph_config->key_file,
                                     "tile-cache", "enabled",
                                     &error);
    if (error != NULL) {
        enabled = TRUE;
        g_error_free(error);
    }

    g_value_set_boolean(value, enabled);
}

/*
 * Enable or disable reading from and writing to the tile cache.
 */
void
ph_config_set_tile_cache_enabled(const GValue *value)
{
    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(value == NULL || G_VALUE_HOLDS_BOOLEAN(value));

    if (value != NULL)
        g_key_file_set_boolean(ph_config->key_file, "tile-cache", "enabled",
                               g_value_get_boolean(value));
    else
        g_key_file_remove_key(ph_config->key_file, "tile-cache", "enabled",
                              NULL);
}

/*
 * Obtain the name of the directory (a string) where cached tiles should be
 * stored.
 */
void
ph_config_get_tile_cache_location(GValue *value)
{
    gchar *location;
    static gchar *default_location = NULL;

    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(G_VALUE_HOLDS_STRING(value));

    location = g_key_file_get_string(ph_config->key_file,
                                     "tile-cache", "location",
                                     NULL);

    if (location == NULL) {
        if (default_location == NULL)
            default_location = g_build_filename(g_get_user_cache_dir(),
                                                g_get_prgname(),
                                                "map-tiles",
                                                NULL);
        g_value_set_static_string(value, default_location);
    }
    else
        g_value_take_string(value, location);
}

/*
 * Set a different directory to use for the tile cache.
 */
void
ph_config_set_tile_cache_location(const GValue *value)
{
    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(value == NULL || G_VALUE_HOLDS_STRING(value));

    if (value != NULL)
        g_key_file_set_string(ph_config->key_file, "tile-cache", "location",
                              g_value_get_string(value));
    else
        g_key_file_remove_key(ph_config->key_file, "tile-cache", "location",
                              NULL);
}

/*
 * Get the number of days after which a cached map tile is considered too old
 * and has to be downloaded again.  This is an unsigned integer value.
 */
void
ph_config_get_max_tile_age(GValue *value)
{
    guint max_tile_age;

    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(G_VALUE_HOLDS_UINT(value));

    max_tile_age = g_key_file_get_uint64(ph_config->key_file,
                                         "tile-cache", "max-age",
                                         NULL);

    g_value_set_uint(value, (max_tile_age != 0) ? max_tile_age : 7);
}

/*
 * Set the maximum acceptable age of a cached tile.
 */
void
ph_config_set_max_tile_age(const GValue *value)
{
    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(value == NULL || G_VALUE_HOLDS_UINT(value));

    if (value != NULL)
        g_key_file_set_uint64(ph_config->key_file, "tile-cache", "max-age",
                              g_value_get_uint(value));
    else
        g_key_file_remove_key(ph_config->key_file, "tile-cache", "max-age",
                              NULL);
}

/*
 * Get the maximum total size, in megabytes, of the tile cache directory.  This
 * is an unsigned integer value.
 */
void
ph_config_get_max_tile_cache_size(GValue *value)
{
    guint max_size;

    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(G_VALUE_HOLDS_UINT(value));

    max_size = g_key_file_get_uint64(ph_config->key_file,
                                     "tile-cache", "max-size",
                                     NULL);

    g_value_set_uint(value, (max_size != 0) ? max_size : 100);
}

/*
 * Set a new maximum total size for the map tile cache.
 */
void
ph_config_set_max_tile_cache_size(const GValue *value)
{
    g_return_if_fail(ph_config != NULL);
    g_return_if_fail(value == NULL || G_VALUE_HOLDS_UINT(value));

    if (value != NULL)
        g_key_file_set_uint64(ph_config->key_file, "tile-cache", "max-size",
                              g_value_get_uint(value));
    else
        g_key_file_remove_key(ph_config->key_file, "tile-cache", "max-size",
                              NULL);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
