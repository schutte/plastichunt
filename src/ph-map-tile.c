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

#include "ph-map-tile.h"
#include "ph-map-tile-cache.h"
#include <libsoup/soup.h>
#include <math.h>
#include <string.h>

/* Data structures {{{1 */

struct _PHMapTileQueue {
    guint refs;                     /* reference count */

    PHMapProvider *provider;        /* tile server metadata */
    GArray *array;                  /* the queue itself */
    SoupSession *session;           /* session object for HTTP requests */
    guint free_connections;         /* available parallel connections */

    PHMapPoint *center;             /* map center for distance-based sorting */
    PHMapRegion *region;            /* region to load tiles for */
    guint zoom;                     /* active zoom level */

    guint source;                   /* ID of the idle source */
    PHMapTileCallback callback;     /* callback for received tiles */
    gpointer user_data;             /* custom data for the callback */
};

/*
 * Data for the soup callback function.
 */
typedef struct _PHMapTileReceivedData {
    PHMapTileQueue *queue;
    PHMapTile tile;
} PHMapTileReceivedData;

/* Forward declarations {{{1 */

static void ph_map_tile_queue_start(PHMapTileQueue *queue,
                                    PHMapTileCallback callback,
                                    gpointer user_data);
static void ph_map_tile_queue_add_source(PHMapTileQueue *queue);
static void ph_map_tile_queue_stop(PHMapTileQueue *queue);

static gboolean ph_map_tile_queue_run(PHMapTileQueue *queue);
static void ph_map_tile_queue_received_tile(SoupSession *session,
                                            SoupMessage *message,
                                            PHMapTileReceivedData *data);
static void ph_map_tile_queue_invoke_callback(PHMapTileQueue *queue,
                                              const PHMapTile *tile,
                                              GdkPixbuf *pixbuf);

static gint ph_map_tile_sort_cb(const PHMapTile *a, const PHMapTile *b,
                                PHMapTileQueue *queue);

/* Memory management {{{1 */

/*
 * Create a new, empty tile queue allowing the given number of parallel
 * connections.  The initial reference count is 1.
 */
PHMapTileQueue *
ph_map_tile_queue_new(guint connections)
{
    PHMapTileQueue *result = g_new0(PHMapTileQueue, 1);

    result->refs = 1;

    result->array = g_array_new(FALSE, FALSE, sizeof(PHMapTile));
    result->session = soup_session_async_new_with_options(
            SOUP_SESSION_MAX_CONNS, connections,
            SOUP_SESSION_MAX_CONNS_PER_HOST, connections,
            NULL);
    result->free_connections = connections;

    return result;
}

/*
 * Increase the reference count of a tile queue.
 */
PHMapTileQueue *
ph_map_tile_queue_ref(PHMapTileQueue *queue)
{
    g_return_val_if_fail(queue != NULL, NULL);

    ++queue->refs;

    return queue;
}

/*
 * Decrease the reference count of a tile queue and free it once it has dropped
 * to zero.
 */
void
ph_map_tile_queue_unref(PHMapTileQueue *queue)
{
    g_return_if_fail(queue != NULL);

    if (--queue->refs == 0) {
        ph_map_tile_queue_stop(queue);

        ph_map_provider_free(queue->provider);
        g_array_free(queue->array, TRUE);
        g_object_unref(queue->session);
        ph_map_point_free(queue->center);
        ph_map_region_free(queue->region);

        g_free(queue);
    }
}

/* Configuration {{{1 */

/*
 * Set the map provider to request tiles from.  This also removes the idle
 * source, clears the queue and aborts all HTTP requests, so the tiles to be
 * displayed have to be added again.
 */
void
ph_map_tile_queue_set_provider(PHMapTileQueue *queue,
                               const PHMapProvider *provider)
{
    g_return_if_fail(queue != NULL);
    g_return_if_fail(provider != NULL);

    ph_map_tile_queue_stop(queue);

    ph_map_provider_free(queue->provider);
    queue->provider = ph_map_provider_copy(provider);

    if (queue->array->len > 0)
        g_array_remove_range(queue->array, 0, queue->array->len);

    soup_session_abort(queue->session);
}

/* Queue manipulation {{{1 */

/*
 * Prepare the queue for adding tiles within the given region.  This removes the
 * idle source and drops all queued tiles which are out of bounds or on a
 * different zoom level.
 */
void
ph_map_tile_queue_begin_add(PHMapTileQueue *queue,
                            const PHMapRegion *region,
                            guint zoom)
{
    guint i;

    g_return_if_fail(queue != NULL);
    g_return_if_fail(region != NULL);

    ph_map_region_free(queue->region);
    queue->region = ph_map_region_copy(region);
    queue->zoom = zoom;

    ph_map_tile_queue_stop(queue);

    i = 0;
    while (i < queue->array->len) {
        PHMapTile *tile = &g_array_index(queue->array, PHMapTile, i);
        if (tile->x < region->x1 || tile->x > region->x2 ||
                tile->y < region->y1 || tile->y > region->y2 ||
                tile->zoom != zoom)
            /* we can safely destroy the order here since
             * ph_map_tile_queue_end() will sort the array anyway */
            g_array_remove_index_fast(queue->array, i);
        else
            ++i;
    }
}

/*
 * Add a tile to the queue.  Only use this between calls to
 * ph_map_tile_queue_begin_add() and ph_map_tile_queue_end_add().
 */
void
ph_map_tile_queue_add(PHMapTileQueue *queue,
                      guint zoom,
                      glong x,
                      glong y)
{
    PHMapTile tile = { zoom, x, y };

    g_return_if_fail(queue != NULL);

    if (queue->provider == NULL)
        return;

    tile.cached = ph_map_tile_cache_query(&tile, queue->provider);
    g_array_append_val(queue->array, tile);
}

/*
 * Finish adding to the queue and sort all tiles according to their distance
 * from the center.  Then start the idle queue.
 */
void
ph_map_tile_queue_end_add(PHMapTileQueue *queue,
                          const PHMapPoint *center,
                          PHMapTileCallback callback,
                          gpointer user_data)
{
    g_return_if_fail(queue != NULL);
    g_return_if_fail(center != NULL);

    if (queue->provider == NULL)
        return;

    ph_map_point_free(queue->center);
    queue->center = ph_map_point_copy(center);

    g_array_sort_with_data(queue->array,
                           (GCompareDataFunc) ph_map_tile_sort_cb,
                           queue);

    ph_map_tile_queue_start(queue, callback, user_data);
}

/* Running the queue {{{1 */

/*
 * Configure the callback for received tiles and start running the idle source
 * which processes the queue.
 */
static void
ph_map_tile_queue_start(PHMapTileQueue *queue,
                        PHMapTileCallback callback,
                        gpointer user_data)
{
    queue->callback = callback;
    queue->user_data = user_data;

    ph_map_tile_queue_add_source(queue);
}

/*
 * Start the idle source which processes the queue (if it is not already
 * running).
 */
static void
ph_map_tile_queue_add_source(PHMapTileQueue *queue)
{
    if (queue->source != 0)
        return;

    queue->source = g_idle_add((GSourceFunc) ph_map_tile_queue_run,
                               ph_map_tile_queue_ref(queue));
}

/*
 * Remove the idle source which processes the queue.
 */
static void
ph_map_tile_queue_stop(PHMapTileQueue *queue)
{
    if (queue->source == 0)
        return;

    (void) g_source_remove(queue->source);
    ph_map_tile_queue_unref(queue);
    queue->source = 0;
}

/*
 * Process the tile at the head (last array index) of the queue.
 *
 * If the queue is empty or if the maximum number of HTTP connections is
 * reached, this function will return with a FALSE value, which causes the
 * source to be removed.  Hence, the source is re-added in the soup session
 * callback if necessary.
 */
static gboolean
ph_map_tile_queue_run(PHMapTileQueue *queue)
{
    PHMapTile logical_tile;
    PHMapTile physical_tile;
    gchar *cache;

    if (queue->array->len == 0 || queue->free_connections == 0) {
        queue->source = 0;
        ph_map_tile_queue_unref(queue);
        return FALSE;
    }

    /* logical tile coordinates: not necessarily in the actual
     * coordinate range of the map */
    logical_tile = g_array_index(queue->array, PHMapTile,
                                 queue->array->len - 1);
    g_array_remove_index_fast(queue->array, queue->array->len - 1);

    /* physical tile coordinates: within [0..tile_limit) */
    ph_map_tile_wrap(&physical_tile, &logical_tile);

    g_debug("Requesting tile: x = %ld, y = %ld, zoom = %d",
            physical_tile.x, physical_tile.y, physical_tile.zoom);

    cache = ph_map_tile_cache_get_location(&physical_tile, queue->provider);

    if (cache == NULL)
        physical_tile.cached = FALSE;

    if (physical_tile.cached) {
        /* immediately load the tile from the disk cache */
        GdkPixbuf *pixbuf;
        guint tile_size = queue->provider->tile_size;

        pixbuf = gdk_pixbuf_new_from_file_at_size(cache, tile_size, tile_size,
                                                  NULL);
        if (pixbuf != NULL) {
            ph_map_tile_queue_invoke_callback(queue, &logical_tile, pixbuf);
            g_object_unref(pixbuf);
        }
        else
            /* on error, forcibly download the tile again */
            physical_tile.cached = FALSE;
    }

    g_free(cache);

    if (!physical_tile.cached) {
        /* start loading it via HTTP */
        gchar *url = ph_map_provider_get_tile_url(queue->provider,
                                                  physical_tile.zoom,
                                                  physical_tile.x,
                                                  physical_tile.y);
        PHMapTileReceivedData *data = g_new(PHMapTileReceivedData, 1);
        SoupSessionCallback callback =
            (SoupSessionCallback) ph_map_tile_queue_received_tile;

        data->tile = logical_tile;
        data->queue = ph_map_tile_queue_ref(queue);

        --queue->free_connections;
        soup_session_queue_message(queue->session,
                                   soup_message_new("GET", url),
                                   callback,
                                   data);
        g_debug("Fetching tile from %s", url);

        g_free(url);
    }

    return TRUE;
}

/*
 * Callback function for completed (or failed) HTTP tile downloads.
 */
static void
ph_map_tile_queue_received_tile(SoupSession *session,
                                SoupMessage *message,
                                PHMapTileReceivedData *data)
{
    GdkPixbuf *pixbuf = NULL;

    if (message->status_code == SOUP_STATUS_OK) {
        GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
        guchar *content = (guchar *) message->response_body->data;
        gsize length = message->response_body->length;

        gdk_pixbuf_loader_set_size(loader,
                                   data->queue->provider->tile_size,
                                   data->queue->provider->tile_size);

        if (gdk_pixbuf_loader_write(loader, content, length, NULL) &&
            gdk_pixbuf_loader_close(loader, NULL))
            pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    }

    if (pixbuf != NULL) {
        /* write the raw data stream to the cache file */
        ph_map_tile_cache_write(&data->tile, data->queue->provider,
                                message->response_body->data,
                                message->response_body->length);

        /* then pass it on to the callback */
        ph_map_tile_queue_invoke_callback(data->queue, &data->tile, pixbuf);

        g_object_unref(pixbuf);
    }

    ++data->queue->free_connections;
    ph_map_tile_queue_add_source(data->queue);
    ph_map_tile_queue_unref(data->queue);
    g_free(data);
}

/*
 * Invoke the callback function for the given tile if it is still matches the
 * desired region and zoom level.
 */
static void
ph_map_tile_queue_invoke_callback(PHMapTileQueue *queue,
                                  const PHMapTile *tile,
                                  GdkPixbuf *pixbuf)
{
    if (tile->x >= floor(queue->region->x1) &&
            tile->x <= ceil(queue->region->x2) &&
            tile->y >= floor(queue->region->y1) &&
            tile->y <= ceil(queue->region->y2) &&
            tile->zoom == queue->zoom) {
        g_debug("Received relevant tile: x = %ld, y = %ld, zoom = %d",
                tile->x, tile->y, tile->zoom);
        queue->callback(queue, tile, pixbuf, queue->user_data);
    }
    else
        g_debug("Discarding tile: x = %ld, y = %ld, zoom = %d",
                tile->x, tile->y, tile->zoom);
}

/* Tile utilities {{{1 */

/*
 * Sorting callback to order map tiles by descending distance from a central
 * point.
 */
static gint
ph_map_tile_sort_cb(const PHMapTile *a,
                    const PHMapTile *b,
                    PHMapTileQueue *queue)
{
    gdouble a_distance = sqrt(pow(a->x - queue->center->x, 2) +
                              pow(a->y - queue->center->y, 2));
    gdouble b_distance = sqrt(pow(b->x - queue->center->x, 2) +
                              pow(b->y - queue->center->y, 2));

    return b_distance - a_distance;
}

/*
 * Tiles may have coordinates which are negative or exceed the tile limit.  This
 * function converts these to the [0..tile_limit) range.
 */
void
ph_map_tile_wrap(PHMapTile *dest,
                 const PHMapTile *source)
{
    glong tile_limit = 1L << source->zoom;

    memcpy(dest, source, sizeof(PHMapTile));

    while (dest->x < 0)
        dest->x += tile_limit;
    while (dest->x >= tile_limit)
        dest->x -= tile_limit;

    while (dest->y < 0)
        dest->y += tile_limit;
    while (dest->y >= tile_limit)
        dest->y -= tile_limit;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
