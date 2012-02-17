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
#include "ph-map-tile-cache.h"
#include <gio/gio.h>
#include <sys/stat.h>

/* Data structures {{{1 */

/*
 * Entry in the list of oldest files in the tile cache.
 */
typedef struct _PHMapTileCacheEntry {
    time_t mtime;                       /* when the file was last written */
    const gchar *parent;                /* cache subdir for the provider */
    gchar *filename;                    /* relative path to the tile */
    gsize disk_usage;                   /* size of the file when scanned */
} PHMapTileCacheEntry;

/*
 * Node in the binary tree which gets constructed when scanning the cache
 * directory for its oldest files.
 */
typedef struct _PHMapTileCacheNode PHMapTileCacheNode;
struct _PHMapTileCacheNode {
    PHMapTileCacheEntry *data;
    PHMapTileCacheNode *left, *right;
};

/*
 * Main tile cache data structure.
 */
typedef struct _PHMapTileCache {
    gboolean scanning;                  /* full scan in progress? */
    gsize total;                        /* estimated total disk usage */
    GSList *oldest;                     /* sorted list of oldest file */
    GStringChunk *directories;          /* known cache subdirectories */
    PHMapTileCacheNode *scan_result;    /* tree with possible old files */
    time_t newest;                      /* age of most recent file in tree */
    gint tree_size;                     /* number of tree nodes */
} PHMapTileCache;

/* Forward declarations {{{1 */

static gboolean ph_map_tile_cache_scan(gpointer data);
static gboolean ph_map_tile_cache_scan_single(GQueue *dir_stack);
static void ph_map_tile_cache_scan_handle_file(const gchar *path,
                                               GFileInfo *file_info);
static void ph_map_tile_cache_tree_to_list(PHMapTileCacheNode **node);
static gboolean ph_map_tile_cache_cleanup_single(gpointer data);
static void ph_map_tile_cache_cleanup();
static void ph_map_tile_cache_entry_free(PHMapTileCacheEntry *entry);
static void ph_map_tile_cache_update(const gchar *path, gboolean added);

/* Reading and writing the cache {{{1 */

/*
 * Get the full path to the cache file for a map tile.  Returns NULL if caching
 * is disabled.
 */
gchar *
ph_map_tile_cache_get_location(const PHMapTile *tile,
                               const PHMapProvider *provider)
{
    GValue enabled = { 0 };
    gchar *result = NULL;

    g_value_init(&enabled, G_TYPE_BOOLEAN);
    ph_config_get_tile_cache_enabled(&enabled);

    if (g_value_get_boolean(&enabled)) {
        PHMapTile physical_tile;
        gchar *cache_dir;

        ph_map_tile_wrap(&physical_tile, tile);

        cache_dir = ph_map_provider_get_cache_dir(provider);
        result = g_strdup_printf("%s/%d/%ld/%ld",
                                 cache_dir, physical_tile.zoom,
                                 physical_tile.x, physical_tile.y);
        g_free(cache_dir);
    }

    g_value_unset(&enabled);

    return result;
}

/*
 * Check if the specified tile from a given provider already exists in the
 * filesystem cache and is not too old to be considered outdated.
 */
gboolean
ph_map_tile_cache_query(const PHMapTile *tile,
                        const PHMapProvider *provider)
{
    gchar *path = ph_map_tile_cache_get_location(tile, provider);
    GValue value = { 0 };
    guint max_age;
    gboolean result = FALSE;
    struct stat st;
    time_t now = time(NULL);

    g_value_init(&value, G_TYPE_UINT);
    ph_config_get_max_tile_age(&value);
    max_age = g_value_get_uint(&value) * 24 * 60 * 60;

    if (path != NULL && stat(path, &st) == 0)
        result = (S_ISREG(st.st_mode) &&
                  ((guint) now - st.st_mtime) < max_age);

    g_free(path);
    g_value_unset(&value);

    return result;
}

/*
 * Write the raw image data for a tile to its cache file.
 */
void
ph_map_tile_cache_write(const PHMapTile *tile,
                        const PHMapProvider *provider,
                        const gchar *data,
                        gsize length)
{
    gchar *path = ph_map_tile_cache_get_location(tile, provider);
    gchar *dirname;

    if (path == NULL)
        /* tile cache disabled */
        return;

    dirname = g_path_get_dirname(path);

    if (!g_file_test(dirname, G_FILE_TEST_EXISTS))
        (void) g_mkdir_with_parents(dirname, 0755);

    ph_map_tile_cache_update(path, FALSE);
    (void) g_file_set_contents(path, data, length, NULL);
    ph_map_tile_cache_update(path, TRUE);

    g_free(path);
    g_free(dirname);
}

/* Scanning the tile cache {{{1 */

/*
 * All the information needed to keep the cache below its required maximum size.
 */
static PHMapTileCache *ph_map_tile_cache = NULL;

#define PH_MAP_TILE_CACHE_FILE_ATTRS \
    "standard::type,standard::name,standard::allocated-size,time::modified"

/*
 * Number of oldest files to store.
 */
#define PH_MAP_TILE_CACHE_NUM_OLDEST 1024

/*
 * Compute the total disk usage of the files in the tile cache directory while
 * creating a list of the oldest files, so they can be removed quickly if the
 * maximum cache size is exceeded.  This function will automatically trigger
 * such a cleanup run if necessary.
 *
 * The return value will be the integral value of the data parameter (so this
 * can be used as a GLib source function).
 */
static gboolean
ph_map_tile_cache_scan(gpointer data)
{
    GValue root = { 0 };
    GQueue *dir_stack;
    GFile *root_file;
    GFileEnumerator *root_enum;

    if (ph_map_tile_cache == NULL || ph_map_tile_cache->scanning)
        return GPOINTER_TO_INT(data);

    g_value_init(&root, G_TYPE_STRING);
    ph_config_get_tile_cache_location(&root);

    root_file = g_file_new_for_path(g_value_get_string(&root));
    root_enum = g_file_enumerate_children(root_file,
                                          PH_MAP_TILE_CACHE_FILE_ATTRS,
                                          0, NULL, NULL);

    if (root_enum != NULL) {
        g_debug("Starting map tile cache scan");

        dir_stack = g_queue_new();
        g_queue_push_tail(dir_stack, root_enum);
        ph_map_tile_cache->scanning = TRUE;
        ph_map_tile_cache->total = 0;
        ph_map_tile_cache->scan_result = NULL;
        ph_map_tile_cache->newest = 0;
        ph_map_tile_cache->tree_size = 0;
        g_slist_free_full(ph_map_tile_cache->oldest,
                          (GDestroyNotify) ph_map_tile_cache_entry_free);
        ph_map_tile_cache->oldest = NULL;

        g_idle_add((GSourceFunc) ph_map_tile_cache_scan_single, dir_stack);
    }

    g_object_unref(root_file);
    g_value_unset(&root);

    return GPOINTER_TO_INT(data);
}

/*
 * Recursively look at some files from the tile cache directory, add their disk
 * usage to the total and possibly add them to the list of oldest files.
 */
static gboolean
ph_map_tile_cache_scan_single(GQueue *dir_stack)
{
    gint limit = 16;
    GFileEnumerator *tail = NULL;

    if (ph_map_tile_cache == NULL)
        return FALSE;

    do {
        GFileInfo *file_info = NULL;

        tail = (GFileEnumerator *) g_queue_peek_tail(dir_stack);

        if (tail != NULL) {
            file_info = g_file_enumerator_next_file(tail, NULL, NULL);

            if (file_info == NULL) {
                /* ascend */
                g_debug("Tile cache subdirectory exhausted; going up");

                g_file_enumerator_close(tail, NULL, NULL);
                g_queue_pop_tail(dir_stack);
                g_object_unref(tail);
            }
        }

        if (file_info != NULL) {
            gchar *parent_path =
                g_file_get_path(g_file_enumerator_get_container(tail));
            gchar *path = g_build_filename(parent_path,
                                           g_file_info_get_name(file_info),
                                           NULL);
            g_free(parent_path);

            if (g_file_info_get_file_type(file_info) == G_FILE_TYPE_DIRECTORY) {
                /* descend */
                g_debug("Scanning tile cache subdirectory: %s", path);

                GFile *file = g_file_new_for_path(path);
                GFileEnumerator *new_tail = g_file_enumerate_children(
                    file, PH_MAP_TILE_CACHE_FILE_ATTRS, 0, NULL, NULL);
                if (new_tail != NULL)
                    g_queue_push_tail(dir_stack, new_tail);
                g_object_unref(file);
            }

            else
                /* actual file found */
                ph_map_tile_cache_scan_handle_file(path, file_info);

            g_object_unref(file_info);
            g_free(path);
        }
    } while (limit > 0 && tail != NULL);

    if (tail != NULL)
        return TRUE;
    else {
        g_message("Tile cache scan complete: %d files found",
                  ph_map_tile_cache->tree_size);

        ph_map_tile_cache_tree_to_list(&ph_map_tile_cache->scan_result);

        ph_map_tile_cache->newest = 0;
        ph_map_tile_cache->tree_size = 0;
        ph_map_tile_cache->scanning = FALSE;

        ph_map_tile_cache_cleanup();

        return FALSE;
    }
}

/*
 * Deal with a single regular file in the cache directory.
 */
static void
ph_map_tile_cache_scan_handle_file(const gchar *path,
                                   GFileInfo *file_info)
{
    PHMapTileCacheEntry entry = { 0 };
    GTimeVal mtime;
    static GRegex *path_regex = NULL;
    gboolean match;
    GMatchInfo *match_info = NULL;

    if (path_regex == NULL)
        path_regex = g_regex_new(
            "^(.*)[/\\\\](\\d+[/\\\\]\\d+[/\\\\]\\d+)$", 0, 0, NULL);

    match = g_regex_match(path_regex, path, 0, &match_info);

    if (match) {
        gchar *temp;

        temp = g_match_info_fetch(match_info, 1);
        entry.parent = g_string_chunk_insert_const(
            ph_map_tile_cache->directories, temp);
        g_free(temp);

        entry.filename = g_match_info_fetch(match_info, 2);
    }

    g_match_info_free(match_info);

    if (!match) {
        /* not a cached tile */
        g_debug("Found unknown file in tile cache: %s", path);
        return;
    }

    g_file_info_get_modification_time(file_info, &mtime);
    entry.mtime = mtime.tv_sec;
    entry.disk_usage = g_file_info_get_attribute_uint64(
        file_info, "standard::allocated-size");

    ph_map_tile_cache->total += entry.disk_usage;

    if (ph_map_tile_cache->tree_size < PH_MAP_TILE_CACHE_NUM_OLDEST ||
        entry.mtime < ph_map_tile_cache->newest) {
        /* old file candidate, insert it into the tree */
        PHMapTileCacheNode **node = &ph_map_tile_cache->scan_result;

        g_debug("Adding old tile cache file candidate: %s", entry.filename);

        while ((*node) != NULL) {
            if (entry.mtime < (*node)->data->mtime)
                node = &(*node)->left;
            else
                node = &(*node)->right;
        }

        *node = g_new0(PHMapTileCacheNode, 1);
        (*node)->data = g_memdup(&entry, sizeof(entry));

        ++ph_map_tile_cache->tree_size;
        if (entry.mtime > ph_map_tile_cache->newest)
            ph_map_tile_cache->newest = entry.mtime;
    }
    else {
        g_debug("Not adding old tile cache file candidate: %s", entry.filename);
        g_free(entry.filename);
    }

    if (ph_map_tile_cache->tree_size > PH_MAP_TILE_CACHE_NUM_OLDEST) {
        /* remove the newest file from the tree */
        PHMapTileCacheNode **node = &ph_map_tile_cache->scan_result;
        PHMapTileCacheNode *new_newest = NULL;
        PHMapTileCacheNode *old_node;

        while ((*node)->right != NULL) {    /* locate rightmost node */
            new_newest = *node;
            node = &(*node)->right;
        }

        old_node = *node;

        g_debug("Removing old tile cache file candidate: %s",
                old_node->data->filename);

        *node = old_node->left;             /* replace by left child */
        --ph_map_tile_cache->tree_size;

        g_free(old_node->data->filename);
        g_free(old_node->data);
        g_free(old_node);

        /* determine mtime of the new newest node */
        while (new_newest != NULL && new_newest->right != NULL)
            new_newest = new_newest->right;
        if (new_newest != NULL)
            ph_map_tile_cache->newest = new_newest->data->mtime;
    }
}

/*
 * Do a reverse (right before left) in-order traversal of the scan result tree
 * to build the singly-linked list of oldest files.
 */
static void
ph_map_tile_cache_tree_to_list(PHMapTileCacheNode **node)
{
    if (*node == NULL)
        return;

    ph_map_tile_cache_tree_to_list(&(*node)->right);
    ph_map_tile_cache->oldest = g_slist_prepend(ph_map_tile_cache->oldest,
                                                (*node)->data);
    ph_map_tile_cache_tree_to_list(&(*node)->left);

    g_free(*node);
    *node = NULL;
}

/* Removing old cached tiles {{{1 */

/*
 * Remove just as many of the oldest files in the tile cache as is needed to
 * reduce the cache size to or below the user-specified limit.
 */
static void
ph_map_tile_cache_cleanup()
{
    GValue value = { 0 };
    guint64 max_total;

    if (ph_map_tile_cache == NULL || ph_map_tile_cache->scanning)
        return;

    g_value_init(&value, G_TYPE_UINT);
    ph_config_get_max_tile_cache_size(&value);
    max_total = g_value_get_uint(&value) * 1024 * 1024;

    if (ph_map_tile_cache->total > max_total) {
        g_idle_add(ph_map_tile_cache_cleanup_single,
                   g_memdup(&max_total, sizeof(max_total)));
    }

    g_value_unset(&value);
}

/*
 * If the estimated total size exceeds the guint64 pointed to by data, remove
 * the oldest file from the list and return TRUE.  Otherwise free data and
 * return FALSE.
 */
static gboolean
ph_map_tile_cache_cleanup_single(gpointer data)
{
    guint64 max_total = *(guint64 *) data;
    PHMapTileCacheEntry *entry;
    gchar *path;
    GFile *file;
    GFileInfo *file_info;
    GSList *head;

    if (ph_map_tile_cache == NULL || ph_map_tile_cache->scanning ||
        ph_map_tile_cache->total <= max_total) {
        g_free(data);
        return FALSE;
    }

    head = ph_map_tile_cache->oldest;

    if (head == NULL) {
        /* no more oldest files: scan again */
        g_free(data);
        g_idle_add(ph_map_tile_cache_scan, GINT_TO_POINTER(FALSE));
        return FALSE;
    }

    entry = (PHMapTileCacheEntry *) head->data;
    path = g_build_filename(entry->parent, entry->filename, NULL);
    file = g_file_new_for_path(path);
    file_info = g_file_query_info(file, PH_MAP_TILE_CACHE_FILE_ATTRS,
                                  0, NULL, NULL);

    if (file_info != NULL) {
        /* has the file been updated? */
        GTimeVal mtime;
        gsize disk_usage;

        g_file_info_get_modification_time(file_info, &mtime);
        disk_usage = g_file_info_get_attribute_uint64(
            file_info, "standard::allocated-size");

        if (mtime.tv_sec != entry->mtime || disk_usage != entry->disk_usage) {
            /* yes, do not remove it, but update total */
            ph_map_tile_cache->total += disk_usage;
            g_debug("Not removing %s from tile cache (maybe updated)",
                    entry->filename);
        }
        else {
            /* still there */
            (void) g_file_delete(file, NULL, NULL);
            g_debug("Removed from tile cache: %s", entry->filename);
        }

        g_object_unref(file_info);
    }
    else
        /* on error, assume that the file has been deleted in the meantime */
        g_debug("Not removing %s from tile cache (maybe already removed)",
                entry->filename);

    g_object_unref(file);
    g_free(path);

    ph_map_tile_cache->total -= entry->disk_usage;
    ph_map_tile_cache_entry_free(entry);

    ph_map_tile_cache->oldest = head->next;
    g_slist_free1(head);

    return TRUE;
}

/*
 * Release all memory associated with an entry in the old file list.
 */
static void
ph_map_tile_cache_entry_free(PHMapTileCacheEntry *entry)
{
    if (entry == NULL)
        return;

    g_free(entry->filename);
    g_free(entry);
}

/* Tile cache cleanup control {{{1 */

/*
 * Turn the tile cache cleanup on/off or trigger a re-scan (e.g. when the
 * location or the maximum size has been changed).
 */
void
ph_map_tile_cache_restart()
{
    GValue value = { 0 };
    gboolean enabled;
    static guint scan_source = 0;

    g_value_init(&value, G_TYPE_BOOLEAN);
    ph_config_get_tile_cache_enabled(&value);
    enabled = g_value_get_boolean(&value);
    g_value_unset(&value);

    if (enabled) {
        if (ph_map_tile_cache == NULL) {
            ph_map_tile_cache = g_new0(PHMapTileCache, 1);
            ph_map_tile_cache->directories = g_string_chunk_new(128);
        }

        if (scan_source == 0) {
            scan_source = g_timeout_add_seconds(    /* every ten minutes, */
                600, ph_map_tile_cache_scan,
                GINT_TO_POINTER(TRUE));             /* forever */
            g_message("Started periodical tile cache scan");
        }

        g_idle_add(ph_map_tile_cache_scan,
                   GINT_TO_POINTER(FALSE));         /* once */
    }

    else {
        if (ph_map_tile_cache != NULL) {
            if (ph_map_tile_cache->scanning)
                ph_map_tile_cache_tree_to_list(&ph_map_tile_cache->scan_result);
            g_slist_free_full(ph_map_tile_cache->oldest,
                              (GDestroyNotify) ph_map_tile_cache_entry_free);
            g_string_chunk_free(ph_map_tile_cache->directories);
            g_free(ph_map_tile_cache);
            ph_map_tile_cache = NULL;
        }

        if (scan_source != 0) {
            g_source_remove(scan_source);
            scan_source = 0;
            g_message("Stopped periodical tile cache scan");
        }
    }
}

/*
 * React to the creation (added == TRUE) or removal of a tile file from the
 * cache, including the start of a cache cleanup run if it becomes necessary.
 */
static void
ph_map_tile_cache_update(const gchar *path,
                         gboolean added)
{
    GFile *file;
    GFileInfo *file_info;

    if (ph_map_tile_cache == NULL || ph_map_tile_cache->scanning)
        return;

    file = g_file_new_for_path(path);
    file_info = g_file_query_info(file, "standard::allocated-size",
                                  0, NULL, NULL);

    if (file_info != NULL) {
        guint64 disk_usage = g_file_info_get_attribute_uint64(
            file_info, "standard::allocated-size");
        if (added) {
            ph_map_tile_cache->total += disk_usage;
            ph_map_tile_cache_cleanup();
        }
        else
            ph_map_tile_cache->total -= disk_usage;

        g_object_unref(file_info);
    }

    g_object_unref(file);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
