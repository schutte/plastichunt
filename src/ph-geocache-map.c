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

#include "ph-geocache-map.h"
#include "ph-geo.h"
#include "ph-geocache.h"
#include "ph-sprite.h"
#include "ph-sprite-image.h"
#include "ph-waypoint.h"
#include <math.h>
#include <glib/gi18n.h>

/* Signals {{{1 */

/*
 * Signals.
 */
enum {
    PH_GEOCACHE_MAP_SIGNAL_GEOCACHE_SELECTED,
    PH_GEOCACHE_MAP_SIGNAL_GEOCACHE_ACTIVATED,
    PH_GEOCACHE_MAP_SIGNAL_COUNT
};

static guint ph_geocache_map_signals[PH_GEOCACHE_MAP_SIGNAL_COUNT];

/* Private data {{{1 */

#define PH_GEOCACHE_MAP_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), PH_TYPE_GEOCACHE_MAP, \
                                 PHGeocacheMapPrivate))

/*
 * Singly linked list for rectangular regions drawn onto the overlay and the
 * data they represent.
 */
typedef struct _PHGeocacheMapRect PHGeocacheMapRect;
struct _PHGeocacheMapRect {
    gint x1, y1;
    gint x2, y2;
    gint index;
    PHGeocacheMapRect *next;
};

/*
 * Private data.
 */
struct _PHGeocacheMapPrivate {
    PHGeocacheList *geocache_list;  /* list of geocaches to display */
    gulong update_handler;          /* handler ID for the "updated" signal */
    GtkTreeIter list_iter;          /* geocache to be drawn next */
    gboolean iter_valid;            /* does the iterator have a meaning? */
    gint iter_index;                /* index of list item iter points to */
    cairo_surface_t *next_overlay;  /* geocache overlay in preparation and */
    cairo_t *cairo;                 /* associated context */
    guint draw_source_id;           /* ID for the geocache drawing GSource */

    cairo_surface_t *overlay;       /* overlay being displayed and */
    PHMapRegion region;             /* the area it covers */

    PHGeocacheMapRect *locations;   /* lookup list for mouse clicks etc. */

    GtkTreeRowReference *selection; /* currently selected geocache */
    gchar *sel_id;                  /* its ID */
    GList *waypoints;               /* waypoints of the selected geocache */
};

/* Forward declarations {{{1 */

static void ph_geocache_map_class_init(PHGeocacheMapClass *klass);
static void ph_geocache_map_init(PHGeocacheMap *map);
static void ph_geocache_map_dispose(GObject *obj);
static void ph_geocache_map_finalize(GObject *obj);

static gboolean ph_geocache_map_expose(GtkWidget *widget,
                                       GdkEventExpose *expose);
static void ph_geocache_map_viewport_changed(PHMap *parent_map);
static void ph_geocache_map_clicked(PHMap *parent_map, GdkEventButton *event);
static gboolean ph_geocache_map_query_tooltip(
    GtkWidget *widget, gint x, gint y, gboolean keyboard,
    GtkTooltip *tooltip, gpointer data);

static void ph_geocache_map_draw_sprite(
    PHGeocacheMap *map, gint longitude, gint latitude, PHSprite sprite,
    guint value, gboolean faded, gboolean highlighted, gint location_index);
static void ph_geocache_map_get_sprite_position(
    PHGeocacheMap *map, gint longitude, gint latitude,
    gint *x, gint *y, gint *width, gint *height, PHSpriteSize *size);

static void ph_geocache_map_draw(PHGeocacheMap *map);
static gboolean ph_geocache_map_draw_single(gpointer data);
static void ph_geocache_map_draw_geocache(PHGeocacheMap *map, GtkTreeIter *iter,
                                          gboolean faded, gboolean highlighted,
                                          gint index);
static void ph_geocache_map_draw_waypoints(PHGeocacheMap *map);

static GtkTreePath *ph_geocache_map_find_by_coordinates(PHGeocacheMap *map,
                                                        gint x, gint y);
static void ph_geocache_map_clear_locations(PHGeocacheMap *map);

/* Sprite dimensions {{{1 */

static int ph_geocache_sprite_small_width = 0;
static int ph_geocache_sprite_small_height = 0;
static int ph_geocache_sprite_medium_width = 0;
static int ph_geocache_sprite_medium_height = 0;

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHGeocacheMap, ph_geocache_map, PH_TYPE_MAP)

/*
 * Class initialization code.
 */
static void
ph_geocache_map_class_init(PHGeocacheMapClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *gtk_widget_class = GTK_WIDGET_CLASS(klass);
    PHMapClass *ph_map_class = PH_MAP_CLASS(klass);

    g_object_class->dispose = ph_geocache_map_dispose;
    g_object_class->finalize = ph_geocache_map_finalize;

    gtk_widget_class->expose_event = ph_geocache_map_expose;

    ph_map_class->viewport_changed = ph_geocache_map_viewport_changed;
    ph_map_class->clicked = ph_geocache_map_clicked;

    ph_geocache_map_signals[PH_GEOCACHE_MAP_SIGNAL_GEOCACHE_SELECTED] =
        g_signal_new("geocache-selected", PH_TYPE_GEOCACHE_MAP,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET(PHGeocacheMapClass, geocache_selected),
                NULL, NULL,
                g_cclosure_marshal_VOID__BOXED, G_TYPE_NONE,
                1, GTK_TYPE_TREE_PATH);
    ph_geocache_map_signals[PH_GEOCACHE_MAP_SIGNAL_GEOCACHE_ACTIVATED] =
        g_signal_new("geocache-activated", PH_TYPE_GEOCACHE_MAP,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET(PHGeocacheMapClass, geocache_activated),
                NULL, NULL,
                g_cclosure_marshal_VOID__BOXED, G_TYPE_NONE,
                1, GTK_TYPE_TREE_PATH);

    g_type_class_add_private(klass, sizeof(PHGeocacheMapPrivate));

    ph_sprite_get_dimensions(PH_SPRITE_GEOCACHE, PH_SPRITE_SIZE_SMALL,
            &ph_geocache_sprite_small_width,
            &ph_geocache_sprite_small_height);
    ph_sprite_get_dimensions(PH_SPRITE_GEOCACHE, PH_SPRITE_SIZE_MEDIUM,
            &ph_geocache_sprite_medium_width,
            &ph_geocache_sprite_medium_height);
}

/*
 * Instance initialization code.
 */
static void
ph_geocache_map_init(PHGeocacheMap *map)
{
    PHGeocacheMapPrivate *priv = PH_GEOCACHE_MAP_GET_PRIVATE(map);

    map->priv = priv;

    g_object_set(map, "has-tooltip", TRUE, NULL);
    g_signal_connect(map, "query-tooltip",
            G_CALLBACK(ph_geocache_map_query_tooltip), NULL);
}

/*
 * Drop references to other objects.
 */
static void
ph_geocache_map_dispose(GObject *obj)
{
    PHGeocacheMap *map = PH_GEOCACHE_MAP(obj);

    if (map->priv->geocache_list != NULL) {
        g_signal_handler_disconnect(map->priv->geocache_list,
                map->priv->update_handler);
        g_object_unref(map->priv->geocache_list);
        map->priv->geocache_list = NULL;
    }
    if (map->priv->next_overlay != NULL) {
        cairo_surface_destroy(map->priv->next_overlay);
        map->priv->next_overlay = NULL;
    }
    if (map->priv->cairo != NULL) {
        cairo_destroy(map->priv->cairo);
        map->priv->cairo = NULL;
    }
    if (map->priv->overlay != NULL) {
        cairo_surface_destroy(map->priv->overlay);
        map->priv->overlay = NULL;
    }

    if (G_OBJECT_CLASS(ph_geocache_map_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(ph_geocache_map_parent_class)->dispose(obj);
}

/*
 * Instance destruction code.
 */
static void
ph_geocache_map_finalize(GObject *obj)
{
    PHGeocacheMap *map = PH_GEOCACHE_MAP(obj);

    if (map->priv->selection != NULL)
        gtk_tree_row_reference_free(map->priv->selection);
    if (map->priv->sel_id != NULL)
        g_free(map->priv->sel_id);
    if (map->priv->waypoints != NULL)
        ph_waypoints_free(map->priv->waypoints);

    ph_geocache_map_clear_locations(PH_GEOCACHE_MAP(obj));

    if (G_OBJECT_CLASS(ph_geocache_map_parent_class)->finalize != NULL)
        G_OBJECT_CLASS(ph_geocache_map_parent_class)->finalize(obj);
}

/* Event handling {{{1 */

/*
 * Paint the overlay on top of the map.
 */
static gboolean
ph_geocache_map_expose(GtkWidget *widget,
                       GdkEventExpose *expose)
{
    PHGeocacheMap *map = PH_GEOCACHE_MAP(widget);
    cairo_t *cr;
    gint xcenter, ycenter;

    if (GTK_WIDGET_CLASS(ph_geocache_map_parent_class)->expose_event != NULL)
        GTK_WIDGET_CLASS(ph_geocache_map_parent_class)->expose_event(
                widget, expose);

    cr = gdk_cairo_create(widget->window);

    /* geocache overlay */
    if (map->priv->overlay != NULL) {
        PHMapRegion viewport;
        gint tile_size, x, y, w, h;

        ph_map_get_viewport(PH_MAP(widget), &viewport);
        tile_size = ph_map_get_provider(PH_MAP(widget))->tile_size;
        x = (map->priv->region.x1 - viewport.x1) * tile_size;
        y = (map->priv->region.y1 - viewport.y1) * tile_size;
        w = (map->priv->region.x2 - map->priv->region.x1) * tile_size;
        h = (map->priv->region.y2 - map->priv->region.y1) * tile_size;

        cairo_set_source_surface(cr, map->priv->overlay, x, y);
        cairo_rectangle(cr, x, y, w, h);
        cairo_fill(cr);
    }

    /* crosshairs */
    xcenter = widget->allocation.width / 2;
    ycenter = widget->allocation.height / 2;
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
    cairo_set_operator(cr, CAIRO_OPERATOR_DIFFERENCE);
    cairo_set_line_width(cr, 1.0);
    cairo_arc(cr, xcenter, ycenter, 5, 0, 2 * M_PI);
    cairo_move_to(cr, xcenter - 7, ycenter);
    cairo_line_to(cr, xcenter + 7, ycenter);
    cairo_move_to(cr, xcenter, ycenter - 7);
    cairo_line_to(cr, xcenter, ycenter + 7);
    cairo_stroke(cr);

    cairo_destroy(cr);

    return FALSE;
}

/*
 * Request the geocache list for the new overlay after the viewport has been
 * moved.
 */
static void
ph_geocache_map_viewport_changed(PHMap *parent_map)
{
    PHGeocacheMap *map = PH_GEOCACHE_MAP(parent_map);

    if (PH_MAP_CLASS(ph_geocache_map_parent_class)->viewport_changed != NULL)
        PH_MAP_CLASS(ph_geocache_map_parent_class)->viewport_changed(
                parent_map);

    if (map->priv->geocache_list != NULL) {
        PHMapRegion viewport;
        PHGeocacheListRange range;

        ph_map_get_viewport(parent_map, &viewport);
        range.north = PH_GEO_CLAMP_LATITUDE_MINFRAC(PH_GEO_DEG_TO_MINFRAC(
                ph_map_y_to_latitude(parent_map, viewport.y1)));
        range.south = PH_GEO_CLAMP_LATITUDE_MINFRAC(PH_GEO_DEG_TO_MINFRAC(
                ph_map_y_to_latitude(parent_map, viewport.y2)));
        range.west = PH_GEO_CLAMP_LONGITUDE_MINFRAC(PH_GEO_DEG_TO_MINFRAC(
                ph_map_x_to_longitude(parent_map, viewport.x1)));
        range.east = PH_GEO_CLAMP_LONGITUDE_MINFRAC(PH_GEO_DEG_TO_MINFRAC(
                ph_map_x_to_longitude(parent_map, viewport.x2)));

        ph_geocache_list_set_range(map->priv->geocache_list, &range);
    }

    ph_geocache_map_draw(map);
}

/*
 * The user has clicked somewhere.  Change the selection or trigger a
 * "geocache-activated" signal if the existing selection has been clicked again.
 */
static void
ph_geocache_map_clicked(PHMap *parent_map,
                        GdkEventButton *event)
{
    PHGeocacheMap *map = PH_GEOCACHE_MAP(parent_map);
    GtkTreePath *path;

    path = ph_geocache_map_find_by_coordinates(map, event->x, event->y);

    if (path != NULL && map->priv->selection != NULL &&
            gtk_tree_row_reference_valid(map->priv->selection) &&
            gtk_tree_path_compare(path,
                gtk_tree_row_reference_get_path(map->priv->selection)) == 0)
        /* selected geocache clicked again: activate it */
        g_signal_emit(map, ph_geocache_map_signals[
                PH_GEOCACHE_MAP_SIGNAL_GEOCACHE_ACTIVATED], 0,
                path);
    else
        /* selecting a (different) geocache or clicked empty space ü*/
        ph_geocache_map_select(map, path);

    if (path != NULL)
        gtk_tree_path_free(path);
}

/* Tooltip {{{1 */

/*
 * Display a summary tooltip when hovering a geocache icon.
 */
static gboolean
ph_geocache_map_query_tooltip(GtkWidget *widget,
                              gint x,
                              gint y,
                              gboolean keyboard,
                              GtkTooltip *tooltip,
                              gpointer data)
{
    PHGeocacheMap *map = PH_GEOCACHE_MAP(widget);
    GtkTreePath *path;
    GtkTreeIter iter;
    GtkWidget *hbox, *vbox, *label, *image;
    gchar *id, *name, *owner;
    guint size, difficulty, terrain;
    gint latitude, longitude;
    gchar *text;

    path = ph_geocache_map_find_by_coordinates(PH_GEOCACHE_MAP(widget), x, y);
    if (path == NULL)
        return FALSE;

    gtk_tree_model_get_iter(GTK_TREE_MODEL(map->priv->geocache_list),
            &iter, path);
    gtk_tree_model_get(GTK_TREE_MODEL(map->priv->geocache_list), &iter,
            PH_GEOCACHE_LIST_COLUMN_ID, &id,
            PH_GEOCACHE_LIST_COLUMN_NAME, &name,
            PH_GEOCACHE_LIST_COLUMN_OWNER, &owner,
            PH_GEOCACHE_LIST_COLUMN_SIZE, &size,
            PH_GEOCACHE_LIST_COLUMN_DIFFICULTY, &difficulty,
            PH_GEOCACHE_LIST_COLUMN_TERRAIN, &terrain,
            PH_GEOCACHE_LIST_COLUMN_NEW_LATITUDE, &latitude,
            PH_GEOCACHE_LIST_COLUMN_NEW_LONGITUDE, &longitude,
            -1);

    hbox = gtk_hbox_new(FALSE, 10);

    /* left column: textual description */
    vbox = gtk_vbox_new(FALSE, 3);

    /* name */
    label = gtk_label_new(NULL);
    text = g_markup_printf_escaped(
            "<span size='larger' weight='bold'>%s</span>", name);
    gtk_label_set_markup(GTK_LABEL(label), text);
    g_free(text);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    /* ID and owner */
    text = g_strdup_printf("%s by %s", id, owner);
    label = gtk_label_new(text);
    g_free(text);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    /* coordinates */
    text = ph_geo_minfrac_to_string(longitude, latitude);
    label = gtk_label_new(text);
    g_free(text);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    /* right column: images */
    vbox = gtk_vbox_new(TRUE, 3);

    /* container size */
    image = ph_sprite_image_new(PH_SPRITE_SIZE, PH_SPRITE_SIZE_SMALL);
    ph_sprite_image_set_value(PH_SPRITE_IMAGE(image), size);
    gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, 0);

    /* difficulty rating */
    image = ph_sprite_image_new(PH_SPRITE_DIFFICULTY, PH_SPRITE_SIZE_SMALL);
    ph_sprite_image_set_value(PH_SPRITE_IMAGE(image), difficulty);
    gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, 0);

    /* terrain rating */
    image = ph_sprite_image_new(PH_SPRITE_TERRAIN, PH_SPRITE_SIZE_SMALL);
    ph_sprite_image_set_value(PH_SPRITE_IMAGE(image), terrain);
    gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    gtk_widget_show_all(hbox);
    gtk_tooltip_set_custom(tooltip, hbox);

    g_free(id);
    g_free(name);
    g_free(owner);

    gtk_tree_path_free(path);

    return TRUE;
}

/* Overlay drawing helpers {{{1 */

/*
 * Draw a geocache or waypoint sprite at the given coordinates.  If faded is
 * TRUE, make it translucent; if highlighted is TRUE, draw a border.  If
 * location_index is non-negative, also prepend the extents of the sprite to the
 * location list for mouse event lookups.
 */
static void
ph_geocache_map_draw_sprite(PHGeocacheMap *map,
                            gint longitude,
                            gint latitude,
                            PHSprite sprite,
                            guint value,
                            gboolean faded,
                            gboolean highlighted,
                            gint location_index)
{
    gint x = 0, y = 0;
    gint w = 0, h = 0;
    PHSpriteSize s = 0;

    ph_geocache_map_get_sprite_position(map, longitude, latitude,
            &x, &y, &w, &h, &s);

    /* draw the sprite on a half-transparent background */
    cairo_rectangle(map->priv->cairo, x, y, w, h);
    if (highlighted) {
        cairo_set_source_rgba(map->priv->cairo, 1.0, 0.3, 0.3, 0.5);
        cairo_fill_preserve(map->priv->cairo);
        cairo_set_source_rgba(map->priv->cairo, 0.6, 0.0, 0.0, 1.0);
        cairo_set_line_width(map->priv->cairo, 2.0);
        cairo_stroke(map->priv->cairo);
    }
    else {
        cairo_set_source_rgba(map->priv->cairo, 1.0, 1.0, 1.0,
                faded ? 0.2 : 0.5);
        cairo_fill(map->priv->cairo);
    }
    ph_sprite_draw(sprite, s, value, map->priv->cairo,
            faded ? 0.5 : 1.0, x, y, NULL, NULL);

    /* also draw a small black triangle in higher zoom levels */
    if (s == PH_SPRITE_SIZE_MEDIUM) {
        cairo_set_source_rgba(map->priv->cairo, 0.0, 0.0, 0.0,
                faded ? 0.5 : 1.0);
        cairo_move_to(map->priv->cairo, x, y + h);
        cairo_line_to(map->priv->cairo, x + w/5.0, y + h);
        cairo_line_to(map->priv->cairo, x, y + 4.0*h/5.0);
        cairo_close_path(map->priv->cairo);
        cairo_fill(map->priv->cairo);
    }

    if (location_index >= 0) {
        /* prepend to the locations list */
        PHGeocacheMapRect *location = g_new(PHGeocacheMapRect, 1);
        location->x1 = x; location->y1 = y;
        location->x2 = x + w; location->y2 = y + h;
        location->index = location_index;
        location->next = map->priv->locations;
        map->priv->locations = location;
    }
}

/*
 * Calculate the pixel coordinates for a geocache sprite.
 */
static void
ph_geocache_map_get_sprite_position(PHGeocacheMap *map,
                                    gint longitude,
                                    gint latitude,
                                    gint *x,
                                    gint *y,
                                    gint *width,
                                    gint *height,
                                    PHSpriteSize *size)
{
    const PHMapProvider *provider = ph_map_get_provider(PH_MAP(map));
    PHMapRegion viewport;
    PHSpriteSize s;

    ph_map_get_viewport(PH_MAP(map), &viewport);

    *x = (ph_map_longitude_to_x(PH_MAP(map), PH_GEO_MINFRAC_TO_DEG(longitude)) -
            viewport.x1) * provider->tile_size;
    *y = (ph_map_latitude_to_y(PH_MAP(map), PH_GEO_MINFRAC_TO_DEG(latitude)) -
            viewport.y1) * provider->tile_size;

    /* choose a different size for higher zoom levels */
    if (ph_map_get_zoom(PH_MAP(map)) >= provider->zoom_detail) {
        s = PH_SPRITE_SIZE_MEDIUM;
        *width = ph_geocache_sprite_medium_width;
        *height = ph_geocache_sprite_medium_height;
        *y -= *height;
    }
    else {
        s = PH_SPRITE_SIZE_SMALL;
        *width = ph_geocache_sprite_small_width;
        *height = ph_geocache_sprite_small_height;
        *x -= (*width)/2;
        *y -= (*height)/2;
    }

    if (size != NULL)
        *size = s;
}

/* Drawing geocaches and waypoints {{{1 */

/*
 * Begin drawing the geocache overlay.
 */
static void
ph_geocache_map_draw(PHGeocacheMap *map)
{
    GtkWidget *widget = GTK_WIDGET(map);
    GtkTreeModel *model;
    GSource *source;

    if (map->priv->next_overlay != NULL)
        cairo_surface_destroy(map->priv->next_overlay);
    if (map->priv->cairo != NULL)
        cairo_destroy(map->priv->cairo);

    ph_geocache_map_clear_locations(map);

    if (map->priv->geocache_list == NULL) {
        map->priv->next_overlay = NULL;
        map->priv->cairo = NULL;
        return;
    }

    map->priv->next_overlay = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            widget->allocation.width, widget->allocation.height);
    map->priv->cairo = cairo_create(map->priv->next_overlay);

    model = GTK_TREE_MODEL(map->priv->geocache_list);
    map->priv->iter_valid = gtk_tree_model_get_iter_first(model,
            &map->priv->list_iter);
    map->priv->iter_index = 0;

    if (map->priv->draw_source_id != 0) {
        g_source_remove(map->priv->draw_source_id);
        map->priv->draw_source_id = 0;
    }

    source = g_idle_source_new();
    g_source_set_callback(source, ph_geocache_map_draw_single, map, NULL);
    map->priv->draw_source_id = g_source_attach(source, NULL);
    g_source_unref(source);
}

/*
 * Draw the next geocache on the list.
 */
static gboolean
ph_geocache_map_draw_single(gpointer data)
{
    PHGeocacheMap *map = PH_GEOCACHE_MAP(data);
    GtkTreeModel *model = GTK_TREE_MODEL(map->priv->geocache_list);
    gchar *id;

    if (map->priv->iter_valid) {
        /* process the next geocache on the list */
        gtk_tree_model_get(model, &map->priv->list_iter,
                PH_GEOCACHE_LIST_COLUMN_ID, &id,
                -1);

        if (map->priv->sel_id != NULL && strcmp(id, map->priv->sel_id) == 0) {
            /* if this is the currently selected geocache, update the selection
             * if necessary and postpone drawing it */
            GtkTreePath *path = gtk_tree_path_new_from_indices(
                    map->priv->iter_index, -1);

            if (!gtk_tree_row_reference_valid(map->priv->selection)) {
                /* force re-selection */
                g_free(map->priv->sel_id);
                map->priv->sel_id = NULL;
                ph_geocache_map_select(map, path);
            }

            gtk_tree_path_free(path);
        }
        else
            ph_geocache_map_draw_geocache(map, &map->priv->list_iter,
                    map->priv->sel_id != NULL, FALSE,
                    map->priv->iter_index);

        g_free(id);

        map->priv->iter_valid = gtk_tree_model_iter_next(model,
                &map->priv->list_iter);
        ++map->priv->iter_index;

        return TRUE;
    }
    else {
        PHMapRegion viewport;
        ph_map_get_viewport(PH_MAP(map), &viewport);

        /* hit the end of the list: draw the selection, if there is any */
        ph_geocache_map_draw_waypoints(map);

        if (map->priv->selection != NULL &&
                gtk_tree_row_reference_valid(map->priv->selection)) {
            GtkTreePath *path;
            gint *indices;
            GtkTreeIter iter;

            path = gtk_tree_row_reference_get_path(map->priv->selection);
            indices = gtk_tree_path_get_indices(path);
            gtk_tree_model_get_iter(model, &iter, path);

            ph_geocache_map_draw_geocache(map, &iter, FALSE, TRUE, indices[0]);
        }

        /* finally, cause the new surface to be drawn */
        map->priv->region = viewport;
        if (map->priv->cairo != NULL) {
            cairo_destroy(map->priv->cairo);
            map->priv->cairo = NULL;
        }
        if (map->priv->overlay != NULL)
            cairo_surface_destroy(map->priv->overlay);
        map->priv->overlay = map->priv->next_overlay;
        map->priv->next_overlay = NULL;
        ph_map_refresh(PH_MAP(map));

        map->priv->draw_source_id = 0;
        return FALSE;
    }
}

/*
 * Draw the sprite for the given geocache.
 */
static void
ph_geocache_map_draw_geocache(PHGeocacheMap *map,
                              GtkTreeIter *iter,
                              gboolean faded,
                              gboolean highlighted,
                              gint index)
{
    GtkTreeModel *model = GTK_TREE_MODEL(map->priv->geocache_list);
    gint latitude, longitude;
    PHGeocacheType type;
    gboolean found, logged, available, archived, note;

    gtk_tree_model_get(model, iter,
            PH_GEOCACHE_LIST_COLUMN_NEW_LATITUDE, &latitude,
            PH_GEOCACHE_LIST_COLUMN_NEW_LONGITUDE, &longitude,
            PH_GEOCACHE_LIST_COLUMN_TYPE, &type,
            PH_GEOCACHE_LIST_COLUMN_FOUND, &found,
            PH_GEOCACHE_LIST_COLUMN_LOGGED, &logged,
            PH_GEOCACHE_LIST_COLUMN_AVAILABLE, &available,
            PH_GEOCACHE_LIST_COLUMN_ARCHIVED, &archived,
            PH_GEOCACHE_LIST_COLUMN_NOTE, &note,
            -1);

    ph_geocache_map_draw_sprite(map, longitude, latitude,
            PH_SPRITE_GEOCACHE,
            ph_sprite_value_for_geocache_details(type, found, logged,
                available, archived, note),
            faded, highlighted,
            index);
}

/*
 * Draw the waypoints of the selected geocache.
 */
static void
ph_geocache_map_draw_waypoints(PHGeocacheMap *map)
{
    GList *current;

    if (map->priv->waypoints != NULL) {
        /* draw all waypoints but the first (header coordinates) */
        for (current = map->priv->waypoints->next; current != NULL;
                current = current->next) {
            PHWaypoint *waypoint = (PHWaypoint *) current->data;

            ph_geocache_map_draw_sprite(map,
                    waypoint->note.new_longitude, waypoint->note.new_latitude,
                    PH_SPRITE_WAYPOINT, waypoint->type, FALSE, FALSE, -1);
        }
    }
}

/* Coordinate lookup {{{1 */

/*
 * Search the geocache whose icon is displayed at the given location.
 */
static GtkTreePath *
ph_geocache_map_find_by_coordinates(PHGeocacheMap *map,
                                    gint x,
                                    gint y)
{
    PHGeocacheMapRect *cur;
    GtkTreePath *result = NULL;

    for (cur = map->priv->locations; cur != NULL; cur = cur->next) {
        if (x >= cur->x1 && y >= cur->y1 &&
                x <= cur->x2 && y <= cur->y2) {
            result = gtk_tree_path_new_from_indices(cur->index, -1);
            break;
        }
    }

    return result;
}

/*
 * Wipe the list of rectangles used to find geocaches by coordinate.
 */
static void
ph_geocache_map_clear_locations(PHGeocacheMap *map)
{
    while (map->priv->locations != NULL) {
        PHGeocacheMapRect *next = map->priv->locations->next;
        g_free(map->priv->locations);
        map->priv->locations = next;
    }
}

/* List signal handlers {{{1 */

/*
 * Redraw the map whenever the list is updated.
 */
static void
ph_geocache_map_list_updated(PHGeocacheList *list,
                             gpointer data)
{
    PHGeocacheMap *map = PH_GEOCACHE_MAP(data);

    ph_geocache_map_draw(map);
    ph_map_refresh(PH_MAP(map));
}

/* Public interface {{{1 */

/*
 * Create a PHGeocacheMap instance.
 */
GtkWidget *
ph_geocache_map_new()
{
    return g_object_new(PH_TYPE_GEOCACHE_MAP, NULL);
}

/*
 * Display the given geocache list.
 */
void
ph_geocache_map_set_list(PHGeocacheMap *map,
                         PHGeocacheList *geocache_list)
{
    g_return_if_fail(map != NULL && PH_IS_GEOCACHE_MAP(map));

    if (map->priv->geocache_list != NULL) {
        g_signal_handler_disconnect(map->priv->geocache_list,
                map->priv->update_handler);
        g_object_unref(map->priv->geocache_list);
    }

    map->priv->geocache_list = g_object_ref(geocache_list);
    map->priv->update_handler = g_signal_connect(map->priv->geocache_list,
            "updated", G_CALLBACK(ph_geocache_map_list_updated), map);

    ph_geocache_map_draw(map);
    ph_map_refresh(PH_MAP(map));
}

/*
 * Select a geocache: Highlight it and show its waypoints.
 */
void
ph_geocache_map_select(PHGeocacheMap *map,
                       GtkTreePath *path)
{
    GtkTreeIter iter;
    gchar *id = NULL;

    g_return_if_fail(map != NULL && PH_IS_GEOCACHE_MAP(map));

    if (path != NULL) {
        gtk_tree_model_get_iter(GTK_TREE_MODEL(map->priv->geocache_list),
                &iter, path);
        gtk_tree_model_get(GTK_TREE_MODEL(map->priv->geocache_list), &iter,
                PH_GEOCACHE_LIST_COLUMN_ID, &id, -1);
    }

    if (g_strcmp0(map->priv->sel_id, id) != 0) {
        ph_waypoints_free(map->priv->waypoints);
        map->priv->waypoints = NULL;

        if (map->priv->sel_id != NULL) {
            g_free(map->priv->sel_id);
            map->priv->sel_id = NULL;
        }

        if (map->priv->selection != NULL) {
            gtk_tree_row_reference_free(map->priv->selection);
            map->priv->selection = NULL;
        }

        if (path != NULL) {
            GError *error = NULL;
            PHDatabase *database;
            gboolean success;

            database = ph_geocache_list_get_database(map->priv->geocache_list);
            success = ph_waypoints_load_by_geocache_id(&map->priv->waypoints,
                    database, id, TRUE, &error);

            if (success) {
                map->priv->sel_id = g_strdup(id);
                map->priv->selection = gtk_tree_row_reference_new(
                        GTK_TREE_MODEL(map->priv->geocache_list), path);
            }
            else {
                GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(map));
                if (GTK_IS_WINDOW(toplevel)) {
                    GtkWidget *message = gtk_message_dialog_new(
                        GTK_WINDOW(toplevel), GTK_DIALOG_MODAL,
                        GTK_MESSAGE_ERROR, GTK_BUTTONS_CANCEL,
                        _("Cannot retrieve the geocache from the database."));
                    gtk_message_dialog_format_secondary_text(
                        GTK_MESSAGE_DIALOG(message), "%s.",
                        error->message);
                    gtk_dialog_run(GTK_DIALOG(message));
                    gtk_widget_destroy(message);
                }
                g_error_free(error);
            }
        }

        g_signal_emit(map, ph_geocache_map_signals[
                    PH_GEOCACHE_MAP_SIGNAL_GEOCACHE_SELECTED],
                0, path);

        ph_geocache_map_draw(map);
    }

    g_free(id);
}

/*
 * Get the waypoint ID of the currently selected geocache, or NULL if there is
 * no selection.
 */
const gchar *
ph_geocache_map_get_selection(PHGeocacheMap *map)
{
    g_return_val_if_fail(map != NULL && PH_IS_GEOCACHE_MAP(map), NULL);

    return map->priv->sel_id;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
