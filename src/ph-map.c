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

#include "ph-map.h"
#include "ph-map-tile.h"
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* Properties {{{1 */

/*
 * Properties.
 */
enum {
    PH_MAP_PROP_0,
    PH_MAP_PROP_PROVIDER,
    PH_MAP_PROP_PAN_DISTANCE
};

/* Signals {{{1 */

/*
 * Signals.
 */
enum {
    PH_MAP_SIGNAL_VIEWPORT_CHANGED,
    PH_MAP_SIGNAL_CLICKED,
    PH_MAP_SIGNAL_COUNT
};

static guint ph_map_signals[PH_MAP_SIGNAL_COUNT];

/* Private data {{{1 */

#define PH_MAP_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), PH_TYPE_MAP, PHMapPrivate))

/*
 * Private data.  All geographical locations are stored as tile coordinates.
 */
struct _PHMapPrivate {
    cairo_surface_t *surface;       /* cairo surface and */
    cairo_t *cairo;                 /* context for drawing tiles */
    PHMapRegion region;             /* map region covered by surface */
    gint width, height;             /* dimensions of region in device pixels */
    gboolean reload;                /* force surface redraw? */

    PHMapPoint center;              /* current center of the viewport */
    PHMapRegion viewport;           /* extent of the viewport */
    PHMapPoint old_center;          /* center on mouse button press */
    GdkPoint anchor;                /* widget coords on mouse button press */
    guint pan_distance;             /* panning sensitivity in pixels */
    gboolean panned;                /* panned or just clicked? */

    PHMapProvider *provider;        /* map provider configuration */

    PHMapTileQueue *tile_queue;     /* tile download manager */

    guint zoom;                     /* current zoom level */
    glong tile_limit;               /* number of tiles in current zoom level */
    GtkAdjustment *zoom_adjustment; /* zoom adjustment */
    gulong zoom_handler;            /* adjustment value change handler */
};

/* Forward declarations {{{1 */

static void ph_map_class_init(PHMapClass *klass);
static void ph_map_init(PHMap *map);
static void ph_map_dispose(GObject *obj);
static void ph_map_finalize(GObject *obj);
static void ph_map_get_property(GObject *obj, guint id,
                                GValue *value, GParamSpec *spec);
static void ph_map_set_property(GObject *obj, guint id,
                                const GValue *value, GParamSpec *spec);

static void ph_map_realize(GtkWidget *widget);
static void ph_map_size_allocate(GtkWidget *widget, GtkAllocation *alloc);

static gboolean ph_map_expose(GtkWidget *widget, GdkEventExpose *event);

static gboolean ph_map_button_press(GtkWidget *widget, GdkEventButton *event);
static gboolean ph_map_button_release(GtkWidget *widget, GdkEventButton *event);
static gboolean ph_map_motion_notify(GtkWidget *widget, GdkEventMotion *event);
static gboolean ph_map_scroll(GtkWidget *widget, GdkEventScroll *event);

static gboolean ph_map_key_press(GtkWidget *widget, GdkEventKey *event);

static void ph_map_update(PHMap *map);

static void ph_map_paint_tile(PHMap *map, const GdkPixbuf *pixbuf,
                              const PHMapTile *tile);
static void ph_map_recycle_tiles(PHMap *map, cairo_surface_t *old_surface,
                                 const PHMapRegion *old_region);

static void ph_map_received_tile(PHMapTileQueue *queue, const PHMapTile *tile,
                                 GdkPixbuf *pixbuf, PHMap *map);

static void ph_map_move_region(PHMap *map);

static void ph_map_zoom_surface(PHMap *map, gdouble change);
static void ph_map_zoom_changed(GtkAdjustment *zoom_adjustment, gpointer data);
static void ph_map_zoom_in(PHMap *map, const PHMapPoint *point);
static void ph_map_zoom_out(PHMap *map, const PHMapPoint *point);

static gint ph_map_x_to_pixels(PHMap *map, gdouble x);
static gint ph_map_y_to_pixels(PHMap *map, gdouble y);
static gdouble ph_map_x_from_pixels(PHMap *map, gint pixels);
static gdouble ph_map_y_from_pixels(PHMap *map, gint pixels);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHMap, ph_map, GTK_TYPE_WIDGET)

/*
 * Class initialization code.
 */
static void
ph_map_class_init(PHMapClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *gtk_widget_class = (GtkWidgetClass *) klass;

    g_object_class->get_property = ph_map_get_property;
    g_object_class->set_property = ph_map_set_property;
    g_object_class->dispose = ph_map_dispose;
    g_object_class->finalize = ph_map_finalize;

    gtk_widget_class->realize = ph_map_realize;
    gtk_widget_class->expose_event = ph_map_expose;
    gtk_widget_class->size_allocate = ph_map_size_allocate;
    gtk_widget_class->button_press_event = ph_map_button_press;
    gtk_widget_class->button_release_event = ph_map_button_release;
    gtk_widget_class->motion_notify_event = ph_map_motion_notify;
    gtk_widget_class->scroll_event = ph_map_scroll;
    gtk_widget_class->key_press_event = ph_map_key_press;

    ph_map_signals[PH_MAP_SIGNAL_VIEWPORT_CHANGED] =
        g_signal_new("viewport-changed", PH_TYPE_MAP, G_SIGNAL_RUN_FIRST,
                G_STRUCT_OFFSET(PHMapClass, viewport_changed),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
                0);
    ph_map_signals[PH_MAP_SIGNAL_CLICKED] =
        g_signal_new("clicked", PH_TYPE_MAP, G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET(PHMapClass, clicked),
                NULL, NULL,
                g_cclosure_marshal_VOID__BOXED, G_TYPE_NONE,
                1, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

    g_type_class_add_private(klass, sizeof(PHMapPrivate));

    g_object_class_install_property(g_object_class,
            PH_MAP_PROP_PROVIDER,
            g_param_spec_boxed("provider",
                "map provider configuration",
                "information about the tile server",
                PH_TYPE_MAP_PROVIDER,
                G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(g_object_class,
            PH_MAP_PROP_PAN_DISTANCE,
            g_param_spec_uint("pan-distance",
                "panning sensitivity",
                "how many pixels the mouse has to move before panning starts",
                0, G_MAXUINT, 5,
                G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
}

/*
 * Instance initialization code.
 */
static void
ph_map_init(PHMap *map)
{
    PHMapPrivate *priv = PH_MAP_GET_PRIVATE(map);

    map->priv = priv;

    priv->reload = TRUE;
    priv->tile_queue = ph_map_tile_queue_new(5);
    priv->zoom_adjustment = GTK_ADJUSTMENT(
            gtk_adjustment_new(0.0, 0.0, 0.0, 1.0, 1.0, 0.0));
    g_object_ref_sink(priv->zoom_adjustment);
    priv->zoom_handler = g_signal_connect(priv->zoom_adjustment,
            "value-changed", G_CALLBACK(ph_map_zoom_changed), map);

    gtk_widget_set_can_focus(GTK_WIDGET(map), TRUE);
}

/*
 * Drop references to other objects.
 */
static void
ph_map_dispose(GObject *obj)
{
    PHMap *map = PH_MAP(obj);

    if (map->priv->surface != NULL) {
        cairo_surface_destroy(map->priv->surface);
        map->priv->surface = NULL;
    }
    if (map->priv->cairo != NULL) {
        cairo_destroy(map->priv->cairo);
        map->priv->cairo = NULL;
    }
    if (map->priv->zoom_adjustment != NULL) {
        g_object_unref(map->priv->zoom_adjustment);
        map->priv->zoom_adjustment = NULL;
    }
    if (map->priv->tile_queue != NULL) {
        ph_map_tile_queue_unref(map->priv->tile_queue);
        map->priv->tile_queue = NULL;
    }

    if (G_OBJECT_CLASS(ph_map_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(ph_map_parent_class)->dispose(obj);
}

/*
 * Instance destruction code.
 */
static void
ph_map_finalize(GObject *obj)
{
    PHMap *map = PH_MAP(obj);

    if (map->priv->provider != NULL)
        ph_map_provider_free(map->priv->provider);

    if (G_OBJECT_CLASS(ph_map_parent_class)->finalize != NULL)
        G_OBJECT_CLASS(ph_map_parent_class)->finalize(obj);
}

/*
 * Property accessor.
 */
static void
ph_map_get_property(GObject *obj,
                    guint id,
                    GValue *value,
                    GParamSpec *spec)
{
    PHMap *map = PH_MAP(obj);

    switch (id) {
    case PH_MAP_PROP_PROVIDER:
        g_value_set_boxed(value, map->priv->provider);
        break;
    case PH_MAP_PROP_PAN_DISTANCE:
        g_value_set_uint(value, map->priv->pan_distance);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
    }
}

/*
 * Property mutator.
 */
static void
ph_map_set_property(GObject *obj,
                    guint id,
                    const GValue *value,
                    GParamSpec *spec)
{
    PHMap *map = PH_MAP(obj);

    switch (id) {
    case PH_MAP_PROP_PROVIDER:
        ph_map_set_provider(map, (PHMapProvider *) g_value_get_boxed(value));
        break;
    case PH_MAP_PROP_PAN_DISTANCE:
        map->priv->pan_distance = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
    }
}

/* Standard GtkWidget code {{{1 */

/*
 * Create GDK resources.
 */
static void
ph_map_realize(GtkWidget *widget)
{
    GdkWindowAttr attr;

    gtk_widget_set_realized(widget, TRUE);

    attr.x = widget->allocation.x;
    attr.y = widget->allocation.y;
    attr.width = widget->allocation.width;
    attr.height = widget->allocation.height;
    attr.wclass = GDK_INPUT_OUTPUT;
    attr.window_type = GDK_WINDOW_CHILD;
    attr.event_mask = gtk_widget_get_events(widget) |
        GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK | GDK_BUTTON1_MOTION_MASK |
        GDK_KEY_PRESS_MASK;
    attr.visual = gtk_widget_get_visual(widget);
    attr.colormap = gtk_widget_get_colormap(widget);

    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
            &attr, GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP);
    widget->style = gtk_style_attach(widget->style, widget->window);

    gdk_window_set_user_data(widget->window, widget);

    gtk_style_set_background(widget->style, widget->window, GTK_STATE_ACTIVE);
}

/*
 * Resize the underlying GDK window and change the viewport accordingly.
 */
static void
ph_map_size_allocate(GtkWidget *widget,
                     GtkAllocation *alloc)
{
    widget->allocation = *alloc;
    if (gtk_widget_get_realized(widget)) {
        gdk_window_move_resize(widget->window,
                alloc->x, alloc->y,
                alloc->width, alloc->height);
        ph_map_update(PH_MAP(widget));
    }
}

/* Redrawing {{{1 */

/*
 * Actually draw the map onto the GDK window.
 */
static gboolean
ph_map_expose(GtkWidget *widget,
              GdkEventExpose *event)
{
    PHMap *map = PH_MAP(widget);
    cairo_t *cr;
    gint xoff, yoff;

    if (map->priv->surface == NULL || map->priv->provider == NULL)
        return FALSE;

    cr = gdk_cairo_create(widget->window);

    /* align region on viewport */
    xoff = ph_map_x_to_pixels(map, map->priv->region.x1);
    yoff = ph_map_y_to_pixels(map, map->priv->region.y1);

    cairo_set_source_surface(cr, map->priv->surface, xoff, yoff);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
    gdk_cairo_rectangle(cr, &event->area);
    cairo_fill(cr);

    cairo_destroy(cr);

    return FALSE;
}

/* Mouse events {{{1 */

/*
 * Respond to a left button click: Store the current viewport center and the
 * point of the click.
 */
static gboolean
ph_map_button_press(GtkWidget *widget,
                    GdkEventButton *event)
{
    PHMap *map = PH_MAP(widget);

    gtk_widget_grab_focus(widget);

    if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
        map->priv->old_center = map->priv->center;
        map->priv->anchor.x = event->x;
        map->priv->anchor.y = event->y;
        map->priv->panned = FALSE;
    }

    return FALSE;
}

/*
 * Respond to a left button release: Redraw.
 */
static gboolean
ph_map_button_release(GtkWidget *widget,
                      GdkEventButton *event)
{
    PHMap *map = PH_MAP(widget);

    if (event->type == GDK_BUTTON_RELEASE && event->button == 1) {
        if (map->priv->panned)
            ph_map_update(map);
        else
            g_signal_emit(widget, ph_map_signals[PH_MAP_SIGNAL_CLICKED],
                    0, event);
    }

    return FALSE;
}

/*
 * Respond to a mouse movement: Determine the new viewport based on the
 * original center coordinates, the point of the button press event and the
 * current location of the pointer.  Redraw accordingly.
 */
static gboolean
ph_map_motion_notify(GtkWidget *widget,
                     GdkEventMotion *event)
{
    PHMap *map = PH_MAP(widget);
    gint relx, rely;

    if (map->priv->provider == NULL)
        return FALSE;

    if ((event->state & GDK_BUTTON1_MASK) == 0)
        return FALSE;

    relx = event->x - map->priv->anchor.x;
    rely = event->y - map->priv->anchor.y;

    if (map->priv->panned ||
            (guint) abs(relx) >= map->priv->pan_distance ||
            (guint) abs(rely) >= map->priv->pan_distance) {
        map->priv->panned = TRUE;
        map->priv->center.x = map->priv->old_center.x -
            (gdouble) relx / map->priv->provider->tile_size;
        map->priv->center.y = map->priv->old_center.y -
            (gdouble) rely / map->priv->provider->tile_size;
        ph_map_update(map);
    }

    return FALSE;
}

/*
 * Zoom the map on mouse wheel events.
 */
static gboolean
ph_map_scroll(GtkWidget *widget,
              GdkEventScroll *event)
{
    PHMap *map = PH_MAP(widget);
    PHMapPoint point;

    if (map->priv->provider == NULL)
        return FALSE;

    point.x = ph_map_x_from_pixels(map, event->x);
    point.y = ph_map_y_from_pixels(map, event->y);

    if (event->direction == GDK_SCROLL_DOWN)
        ph_map_zoom_in(map, &point);
    else if (event->direction == GDK_SCROLL_UP)
        ph_map_zoom_out(map, &point);

    return FALSE;
}

/* Keyboard events {{{1 */

/*
 * Move and zoom the map on key presses.
 */
static gboolean
ph_map_key_press(GtkWidget *widget,
                 GdkEventKey *event)
{
    PHMap *map = PH_MAP(widget);
    gdouble delta;

    if (map->priv->provider == NULL)
        return FALSE;

    switch (event->keyval) {
    case '+':
        ph_map_zoom_in(map, &map->priv->center);
        return TRUE;
    case '-':
        ph_map_zoom_out(map, &map->priv->center);
        return TRUE;
    case GDK_KEY_Up: case GDK_KEY_Down:
    case GDK_KEY_Left: case GDK_KEY_Right:
        /* move the map ten pixels */
        delta = 10.0 / map->priv->provider->tile_size;
        if (event->keyval == GDK_KEY_Up)
            map->priv->center.y -= delta;
        else if (event->keyval == GDK_KEY_Down)
            map->priv->center.y += delta;
        else if (event->keyval == GDK_KEY_Left)
            map->priv->center.x -= delta;
        else if (event->keyval == GDK_KEY_Right)
            map->priv->center.x += delta;
        ph_map_update(map);
        return TRUE;
    default:
        return FALSE;
    }
}

/* Map update {{{1 */

/*
 * Calculate the viewport from the center and the widget size.  If necessary,
 * move the underlying region.
 */
static void
ph_map_update(PHMap *map)
{
    GtkWidget *widget = GTK_WIDGET(map);
    gdouble width;
    gdouble height;

    if (map->priv->provider == NULL)
        return;

    width = (gdouble) widget->allocation.width /
        map->priv->provider->tile_size;
    height = (gdouble) widget->allocation.height /
        map->priv->provider->tile_size;

    map->priv->tile_limit = 1L << map->priv->zoom;

    /* wrap around if center becomes negative or exceeds tile limit */
    while (map->priv->center.x < 0)
        map->priv->center.x += map->priv->tile_limit;
    while (map->priv->center.x >= map->priv->tile_limit)
        map->priv->center.x -= map->priv->tile_limit;

    while (map->priv->center.y < 0)
        map->priv->center.y += map->priv->tile_limit;
    while (map->priv->center.y >= map->priv->tile_limit)
        map->priv->center.y -= map->priv->tile_limit;

    map->priv->viewport.x1 = map->priv->center.x - width / 2;
    map->priv->viewport.y1 = map->priv->center.y - height / 2;
    map->priv->viewport.x2 = map->priv->viewport.x1 + width;
    map->priv->viewport.y2 = map->priv->viewport.y1 + height;

    if (map->priv->reload ||
            map->priv->viewport.x1 < map->priv->region.x1 ||
            map->priv->viewport.y1 < map->priv->region.y1 ||
            map->priv->viewport.x2 > map->priv->region.x2 ||
            map->priv->viewport.y2 > map->priv->region.y2)
        ph_map_move_region(map);

    g_signal_emit(map, ph_map_signals[PH_MAP_SIGNAL_VIEWPORT_CHANGED], 0);

    ph_map_refresh(map);
}

/* Drawing tiles {{{1 */

/*
 * Copy a tile from a PNG surface to the map surface.
 */
static void
ph_map_paint_tile(PHMap *map,
                  const GdkPixbuf *pixbuf,
                  const PHMapTile *tile)
{
    cairo_t *cr = map->priv->cairo;

    if (cr != NULL) {
        gint xoff = (tile->x - map->priv->region.x1) *
            map->priv->provider->tile_size;
        gint yoff = (tile->y - map->priv->region.y1) *
            map->priv->provider->tile_size;

        gdk_cairo_set_source_pixbuf(cr, pixbuf, xoff, yoff);
        cairo_rectangle(cr, xoff, yoff,
                map->priv->provider->tile_size,
                map->priv->provider->tile_size);
        cairo_fill(cr);
    }
}

/*
 * Copy the overlapping region from the old to the new surface.
 */
static void
ph_map_recycle_tiles(PHMap *map,
                     cairo_surface_t *old_surface,
                     const PHMapRegion *old_region)
{
    PHMapRegion common_region;
    gint old_x, old_y, new_x, new_y;
    gint width, height;
    cairo_t *target = map->priv->cairo;
    gint tile_size = map->priv->provider->tile_size;

    common_region.x1 = MAX(old_region->x1, map->priv->region.x1);
    common_region.y1 = MAX(old_region->y1, map->priv->region.y1);
    common_region.x2 = MIN(old_region->x2, map->priv->region.x2);
    common_region.y2 = MIN(old_region->y2, map->priv->region.y2);

    if (common_region.x2 <= common_region.x1 ||
            common_region.y2 <= common_region.y1)
        /* no overlap - nothing to copy */
        return;

    old_x = (common_region.x1 - old_region->x1) * tile_size;
    old_y = (common_region.y1 - old_region->y1) * tile_size;
    new_x = (common_region.x1 - map->priv->region.x1) * tile_size;
    new_y = (common_region.y1 - map->priv->region.y1) * tile_size;

    width = (common_region.x2 - common_region.x1) * tile_size;
    height = (common_region.y2 - common_region.y1) * tile_size;

    cairo_set_source_surface(target, old_surface, new_x - old_x, new_y - old_y);
    cairo_rectangle(target, new_x, new_y, width, height);
    cairo_fill(target);
}

/* Viewport changes {{{1 */

/*
 * Callback for the reception of tiles via the queue.
 */
static void
ph_map_received_tile(PHMapTileQueue *queue,
                     const PHMapTile *tile,
                     GdkPixbuf *pixbuf,
                     PHMap *map)
{
    ph_map_paint_tile(map, pixbuf, tile);
    ph_map_refresh(map);
}

/*
 * Provide a map region consisting of full tiles covering the whole viewport.
 */
static void
ph_map_move_region(PHMap *map)
{
    PHMapRegion old_region;
    cairo_surface_t *old_surface;
    glong x, y;

    if (map->priv->provider == NULL)
        return;

    old_region = map->priv->region;
    old_surface = map->priv->surface;

    /* make surface big enough to fill entire viewport with tiles */
    map->priv->region.x1 = floor(map->priv->viewport.x1);
    map->priv->region.y1 = floor(map->priv->viewport.y1);
    map->priv->region.x2 = ceil(map->priv->viewport.x2);
    map->priv->region.y2 = ceil(map->priv->viewport.y2);

    if (map->priv->region.x2 >= map->priv->region.x1 + map->priv->tile_limit) {
        map->priv->region.x1 = 0;
        map->priv->region.x2 = map->priv->tile_limit;
    }
    if (map->priv->region.y2 >= map->priv->region.y1 + map->priv->tile_limit) {
        map->priv->region.y1 = 0;
        map->priv->region.y2 = map->priv->tile_limit;
    }

    map->priv->width = (map->priv->region.x2 - map->priv->region.x1) *
        map->priv->provider->tile_size;
    map->priv->height = (map->priv->region.y2 - map->priv->region.y1) *
        map->priv->provider->tile_size;

    map->priv->surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
            map->priv->width, map->priv->height);
    if (map->priv->cairo != NULL)
        cairo_destroy(map->priv->cairo);
    map->priv->cairo = cairo_create(map->priv->surface);

    /* reuse common area from the previous region */
    if (old_surface != NULL)
        ph_map_recycle_tiles(map, old_surface, &old_region);

    /* fill the queue */
    ph_map_tile_queue_begin_add(map->priv->tile_queue,
                                &map->priv->region, map->priv->zoom);

    x = floor(map->priv->region.x1);
    while (x < ceil(map->priv->region.x2)) {
        y = floor(map->priv->region.y1);
        while (y < ceil(map->priv->region.y2)) {
            if (map->priv->reload || old_surface == NULL ||
                    x < old_region.x1 || x >= old_region.x2 ||
                    y < old_region.y1 || y >= old_region.y2)
                ph_map_tile_queue_add(map->priv->tile_queue,
                                      map->priv->zoom, x, y);
            y += 1;
        }
        x += 1;
    }

    ph_map_tile_queue_end_add(map->priv->tile_queue,
                              &map->priv->center,
                              (PHMapTileCallback) ph_map_received_tile,
                              map);
    map->priv->reload = FALSE;

    if (old_surface != NULL)
        cairo_surface_destroy(old_surface);
}

/* Zooming {{{1 */

/*
 * Do a low-quality "preview zoom" by scaling the surface.
 */
static void
ph_map_zoom_surface(PHMap *map,
                    gdouble change)
{
    const GtkAllocation *allocation = &GTK_WIDGET(map)->allocation;
    cairo_surface_t *old_surface = map->priv->surface;
    cairo_t *cr;
    cairo_matrix_t matrix;
    cairo_pattern_t *pattern;
    gint tile_size = map->priv->provider->tile_size;
    PHMapPoint new_center;
    PHMapRegion new_region;
    gint new_width, new_height;
    gdouble old_xc, old_yc;
    gdouble new_xc, new_yc;
    gdouble excess;
    gdouble ratio;

    new_center.x = map->priv->center.x * change;
    new_center.y = map->priv->center.y * change;
    new_region.x1 = map->priv->region.x1 * change;
    new_region.y1 = map->priv->region.y1 * change;
    new_region.x2 = map->priv->region.x2 * change;
    new_region.y2 = map->priv->region.y2 * change;
    new_width = map->priv->width * change;
    new_height = map->priv->height * change;

    /* don't let the zoomed region become too big */
    if (new_width > 2 * allocation->width) {
        excess = ((gdouble) (new_width - 2 * allocation->width)) / tile_size;
        ratio = (new_center.x - new_region.x1) /
            (new_region.x2 - new_region.x1);

        new_region.x1 += ratio * excess;
        new_region.x2 -= (1 - ratio) * excess;
        new_width = 2 * allocation->width;
    }

    if (new_height > 2 * allocation->height) {
        excess = ((gdouble) (new_height - 2 * allocation->height)) / tile_size;
        ratio = (new_center.y - new_region.y1) /
            (new_region.y2 - new_region.y1);

        new_region.y1 += ratio * excess;
        new_region.y2 -= (1 - ratio) * excess;
        new_height = 2 * allocation->height;
    }

    /* calculate the pixel coordinates of the center relative to the
     * edges of the region */
    old_xc = (map->priv->center.x - map->priv->region.x1) * tile_size;
    old_yc = (map->priv->center.y - map->priv->region.y1) * tile_size;
    new_xc = (new_center.x - new_region.x1) * tile_size;
    new_yc = (new_center.y - new_region.y1) * tile_size;

    /* 3. move so that after zooming, (0, 0) is at the old center */
    cairo_matrix_init_translate(&matrix, old_xc, old_yc);
    /* 2. zoom */
    cairo_matrix_scale(&matrix, 1.0 / change, 1.0 / change);
    /* 1. move so that before zooming, the new center is at (0, 0) */
    cairo_matrix_translate(&matrix, -new_xc, -new_yc);

    map->priv->surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
            new_width, new_height);
    cairo_destroy(map->priv->cairo);
    cr = map->priv->cairo = cairo_create(map->priv->surface);

    /* transform and copy the old surface onto the zoomed one */
    pattern = cairo_pattern_create_for_surface(old_surface);
    cairo_pattern_set_matrix(pattern, &matrix);
    cairo_set_source(cr, pattern);
    cairo_paint(cr);
    cairo_pattern_destroy(pattern);

    map->priv->region = new_region;
    map->priv->width = new_width;
    map->priv->height = new_height;
    /* center handled by caller */

    cairo_surface_destroy(old_surface);
    map->priv->reload = TRUE;
}

/*
 * Handle a value change in the zoom adjustment.
 */
static void
ph_map_zoom_changed(GtkAdjustment *zoom_adjustment,
                    gpointer data)
{
    PHMap *map = PH_MAP(data);
    guint new_zoom;

    new_zoom = (guint) gtk_adjustment_get_value(zoom_adjustment);
    if (new_zoom != map->priv->zoom) {
        gdouble change = pow(2, (gint) new_zoom - (gint) map->priv->zoom);

        if (map->priv->cairo != NULL)
            ph_map_zoom_surface(map, change);

        map->priv->center.x *= change;
        map->priv->center.y *= change;
        map->priv->zoom = new_zoom;

        ph_map_update(map);
    }
}

/*
 * Zoom in one step around a point given in tile coordinates.
 */
static void
ph_map_zoom_in(PHMap *map,
               const PHMapPoint *point)
{
    guint zoom = map->priv->zoom;

    if (zoom < (guint) gtk_adjustment_get_upper(map->priv->zoom_adjustment)) {
        map->priv->center.x = (map->priv->center.x + point->x) / 2;
        map->priv->center.y = (map->priv->center.y + point->y) / 2;
        gtk_adjustment_set_value(map->priv->zoom_adjustment, zoom + 1);
    }
}

/*
 * Zoom out one step around a point given in tile coordinates.
 */
static void
ph_map_zoom_out(PHMap *map,
                const PHMapPoint *point)
{
    guint zoom = map->priv->zoom;

    if (zoom > (guint) gtk_adjustment_get_lower(map->priv->zoom_adjustment)) {
        map->priv->center.x += map->priv->center.x - point->x;
        map->priv->center.y += map->priv->center.y - point->y;
        gtk_adjustment_set_value(map->priv->zoom_adjustment, zoom - 1);
    }
}

/* Public interface {{{1 */

/*
 * Create a PHMap instance.
 */
GtkWidget *
ph_map_new()
{
    return g_object_new(PH_TYPE_MAP, NULL);
}

/*
 * Move the map to the desired coordinates in the tile coordinate system.
 */
void
ph_map_set_coordinates(PHMap *map,
                       gdouble x,
                       gdouble y)
{
    g_return_if_fail(map != NULL && PH_IS_MAP(map));

    map->priv->center.x = x;
    map->priv->center.y = y;

    ph_map_update(map);
}

/*
 * Move the map to the desired location specified by geographical coordinates
 * (in degrees).
 */
void
ph_map_set_lonlat(PHMap *map,
                  gdouble longitude,
                  gdouble latitude)
{
    gdouble x, y;

    g_return_if_fail(map != NULL && PH_IS_MAP(map));
    g_return_if_fail(longitude >= -180.0 && longitude <= 180.0);
    g_return_if_fail(latitude >= -90.0 && latitude <= 90.0);

    x = ph_map_longitude_to_x(map, longitude);
    y = ph_map_latitude_to_y(map, latitude);
    ph_map_set_coordinates(map, x, y);
}

/*
 * Move the map to the desired coordinates in the tile coordinate system,
 * specified as a PHMapPoint value.
 */
void
ph_map_set_center(PHMap *map,
                  const PHMapPoint *center)
{
    ph_map_set_coordinates(map, center->x, center->y);
}

/*
 * Obtain the current center coordinates of the map (in tile coordinates).
 */
void
ph_map_get_center(PHMap *map,
                  PHMapPoint *center)
{
    g_return_if_fail(map != NULL && PH_IS_MAP(map));
    g_return_if_fail(center != NULL);

    center->x = map->priv->center.x;
    center->y = map->priv->center.y;
}

/*
 * Obtain the currently visible part of the map (in tile coordinates).
 */
void
ph_map_get_viewport(PHMap *map,
                    PHMapRegion *viewport)
{
    g_return_if_fail(map != NULL && PH_IS_MAP(map));
    g_return_if_fail(viewport != NULL);

    memcpy(viewport, &map->priv->viewport, sizeof(PHMapRegion));
}

/*
 * Get the current zoom level.
 */
guint
ph_map_get_zoom(PHMap *map)
{
    g_return_val_if_fail(map != NULL && PH_IS_MAP(map), 0);

    return map->priv->zoom;
}

/*
 * Get the zoom adjustment.
 */
GtkAdjustment *
ph_map_get_zoom_adjustment(PHMap *map)
{
    g_return_val_if_fail(map != NULL && PH_IS_MAP(map), NULL);

    return map->priv->zoom_adjustment;
}

/*
 * Trigger a redraw of the entire visible map.
 */
void
ph_map_refresh(PHMap *map)
{
    GtkWidget *widget;

    g_return_if_fail(map != NULL && PH_IS_MAP(map));

    widget = GTK_WIDGET(map);
    if (gtk_widget_get_realized(widget))
        gdk_window_invalidate_rect(widget->window, NULL, FALSE);
}

/* Coordinate conversions {{{1 */

/*
 * Convert a longitude to the tile coordinate system, using the current zoom
 * level.
 */
gdouble
ph_map_longitude_to_x(PHMap *map,
                      gdouble longitude)
{
    g_return_val_if_fail(map != NULL && PH_IS_MAP(map), 0.0);

    return ((longitude + 180) / 360.0) * map->priv->tile_limit;
}

/*
 * Convert a latitude to the tile coordinate system, using the current zoom
 * level.
 */
gdouble
ph_map_latitude_to_y(PHMap *map,
                     gdouble latitude)
{
    gdouble rad;

    g_return_val_if_fail(map != NULL && PH_IS_MAP(map), 0.0);

    rad = latitude * G_PI / 180;
    return (1 - (log(tan(rad) + (1 / cos(rad))) / G_PI)) / 2 *
        map->priv->tile_limit;
}

/*
 * Convert a tile coordinate into a longitude, using the current zoom level.
 */
gdouble
ph_map_x_to_longitude(PHMap *map,
                      gdouble x)
{
    g_return_val_if_fail(map != NULL && PH_IS_MAP(map), 0.0);

    return (x / map->priv->tile_limit * 360.0 - 180.0);
}

/*
 * Convert a tile coordinate into a latitude, using the current zoom level.
 */
gdouble
ph_map_y_to_latitude(PHMap *map,
                     gdouble y)
{
    gdouble rad;

    g_return_val_if_fail(map != NULL && PH_IS_MAP(map), 0.0);

    rad = atan(sinh(M_PI * (1 - 2 * y / map->priv->tile_limit)));
    return (rad * 180.0 / M_PI);
}

/*
 * Convert a horizontal tile coordinate to screen pixels.
 */
static gint
ph_map_x_to_pixels(PHMap *map,
                   gdouble x)
{
    return (x - map->priv->viewport.x1) * map->priv->provider->tile_size;
}

/*
 * Convert a vertical tile coordinate to screen pixels.
 */
static gint
ph_map_y_to_pixels(PHMap *map,
                   gdouble y)
{
    return (y - map->priv->viewport.y1) * map->priv->provider->tile_size;
}

/*
 * Convert screen pixels to a horizontal tile coordinate.
 */
static gdouble
ph_map_x_from_pixels(PHMap *map,
                     gint pixels)
{
    return ((gdouble) pixels) / map->priv->provider->tile_size +
        map->priv->viewport.x1;
}

/*
 * Convert screen pixels to a vertical tile coordinate.
 */
static gdouble
ph_map_y_from_pixels(PHMap *map,
                     gint pixels)
{
    return ((gdouble) pixels) / map->priv->provider->tile_size +
        map->priv->viewport.y1;
}

/* Configuration {{{1 */

/*
 * Change to another map provider.  Triggers a redraw of the entire map.
 */
void
ph_map_set_provider(PHMap *map,
                    const PHMapProvider *provider)
{
    if (map->priv->provider != NULL) {
        ph_map_provider_free(map->priv->provider);
        map->priv->provider = NULL;
    }

    if (provider != NULL) {
        map->priv->provider = ph_map_provider_copy(provider);
        ph_map_tile_queue_set_provider(map->priv->tile_queue, provider);

        g_object_set(map->priv->zoom_adjustment,
                "lower", (gdouble) provider->zoom_min,
                "upper", (gdouble) provider->zoom_max,
                NULL);

        if (map->priv->zoom < provider->zoom_min)
            gtk_adjustment_set_value(map->priv->zoom_adjustment,
                    provider->zoom_min);
        else if (map->priv->zoom > provider->zoom_max)
            gtk_adjustment_set_value(map->priv->zoom_adjustment,
                    provider->zoom_max);

        map->priv->reload = TRUE;

        ph_map_update(map);
    }
}

/*
 * Retrieve the current map provider configuration.
 */
PHMapProvider *
ph_map_get_provider(PHMap *map)
{
    g_return_val_if_fail(map != NULL && PH_IS_MAP(map), NULL);

    return map->priv->provider;
}

/* PHMapPoint functions {{{1 */

/*
 * GBoxedCopyFunc for map points.
 */
PHMapPoint *
ph_map_point_copy(const PHMapPoint *point)
{
    return (PHMapPoint *) g_memdup(point, sizeof(PHMapPoint));
}

/*
 * GBoxedFreeFunc for map points.
 */
void
ph_map_point_free(PHMapPoint *point)
{
    g_free(point);
}

/*
 * Register PHMapPoint with the GLib type system.
 */
GType
ph_map_point_get_type()
{
    static GType type = 0;

    if (type == 0)
        type = g_boxed_type_register_static("PHMapPoint",
                (GBoxedCopyFunc) ph_map_point_copy,
                (GBoxedFreeFunc) ph_map_point_free);

    return type;
}

/* PHMapRegion functions {{{1 */

/*
 * GBoxedCopyFunc for map regions.
 */
PHMapRegion *
ph_map_region_copy(const PHMapRegion *region)
{
    return (PHMapRegion *) g_memdup(region, sizeof(PHMapRegion));
}

/*
 * GBoxedFreeFunc for map regions.
 */
void
ph_map_region_free(PHMapRegion *region)
{
    g_free(region);
}

/*
 * Register PHMapRegion with the GLib type system.
 */
GType
ph_map_region_get_type()
{
    static GType type = 0;

    if (type == 0)
        type = g_boxed_type_register_static("PHMapRegion",
                (GBoxedCopyFunc) ph_map_region_copy,
                (GBoxedFreeFunc) ph_map_region_free);

    return type;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
