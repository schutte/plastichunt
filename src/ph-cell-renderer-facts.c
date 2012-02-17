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

#include "ph-cell-renderer-facts.h"
#include <glib/gi18n.h>

/* Properties {{{1 */

/*
 * Properties.
 */
enum {
    PH_CELL_RENDERER_FACTS_PROP_0,
    PH_CELL_RENDERER_FACTS_PROP_SHOW,
    PH_CELL_RENDERER_FACTS_PROP_SPRITE_SIZE,
    PH_CELL_RENDERER_FACTS_PROP_SIZE,
    PH_CELL_RENDERER_FACTS_PROP_DIFFICULTY,
    PH_CELL_RENDERER_FACTS_PROP_TERRAIN
};

/* Private data {{{1 */

#define PH_CELL_RENDERER_FACTS_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), PH_TYPE_CELL_RENDERER_FACTS, \
                                 PHCellRendererFactsPrivate))

/*
 * Private data.
 */
struct _PHCellRendererFactsPrivate {
    guint show;
    PHSpriteSize sprite_size;
    PHGeocacheSize size;
    guint8 difficulty;
    guint8 terrain;
    gint width, height, total_height;
};

/* Forward declarations {{{1 */

static void ph_cell_renderer_facts_class_init(PHCellRendererFactsClass *klass);
static void ph_cell_renderer_facts_init(PHCellRendererFacts *renderer);
static void ph_cell_renderer_facts_get_property(GObject *obj, guint id,
                                                GValue *value,
                                                GParamSpec *spec);
static void ph_cell_renderer_facts_set_property(GObject *obj, guint id,
                                                const GValue *value,
                                                GParamSpec *spec);

static void ph_cell_renderer_facts_get_size(
    GtkCellRenderer *cr, GtkWidget *widget, GdkRectangle *area,
    gint *xoff, gint *yoff, gint *width, gint *height);
static void ph_cell_renderer_facts_calculate_size(
    PHCellRendererFacts *renderer);

static void ph_cell_renderer_facts_render(
    GtkCellRenderer *cr, GdkDrawable *window, GtkWidget *widget,
    GdkRectangle *bg_area, GdkRectangle *cell_area, GdkRectangle *expose_area,
    GtkCellRendererState flags);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHCellRendererFacts, ph_cell_renderer_facts,
              GTK_TYPE_CELL_RENDERER)

/*
 * Class initialization code.
 */
static void
ph_cell_renderer_facts_class_init(PHCellRendererFactsClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GtkCellRendererClass *cr_class = GTK_CELL_RENDERER_CLASS(klass);

    g_object_class->get_property = ph_cell_renderer_facts_get_property;
    g_object_class->set_property = ph_cell_renderer_facts_set_property;

    cr_class->get_size = ph_cell_renderer_facts_get_size;
    cr_class->render = ph_cell_renderer_facts_render;

    g_type_class_add_private(klass, sizeof(PHCellRendererFactsPrivate));

    g_object_class_install_property(g_object_class,
            PH_CELL_RENDERER_FACTS_PROP_SHOW,
            g_param_spec_uint("show",
                "shown facts",
                "bit field specifying which facts to display",
                0, G_MAXUINT8, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(g_object_class,
            PH_CELL_RENDERER_FACTS_PROP_SPRITE_SIZE,
            g_param_spec_uint("sprite-size",
                "PHSpriteSize value",
                "size of the sprites",
                0, PH_SPRITE_SIZE_COUNT - 1, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(g_object_class,
            PH_CELL_RENDERER_FACTS_PROP_SIZE,
            g_param_spec_uint("geocache-size",
                "geocache size",
                "PHGeocacheSize value",
                0, PH_GEOCACHE_SIZE_COUNT - 1, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(g_object_class,
            PH_CELL_RENDERER_FACTS_PROP_DIFFICULTY,
            g_param_spec_uint("geocache-difficulty",
                "geocache difficulty",
                "difficulty rating times 10",
                0, G_MAXUINT8, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(g_object_class,
            PH_CELL_RENDERER_FACTS_PROP_TERRAIN,
            g_param_spec_uint("geocache-terrain",
                "geocache terrain",
                "terrain rating times 10",
                0, G_MAXUINT8, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE));
}

/*
 * Instance initialization code.
 */
static void
ph_cell_renderer_facts_init(PHCellRendererFacts *renderer)
{
    renderer->priv = PH_CELL_RENDERER_FACTS_GET_PRIVATE(renderer);
}

/*
 * Property accessor.
 */
static void
ph_cell_renderer_facts_get_property(GObject *obj,
                                    guint id,
                                    GValue *value,
                                    GParamSpec *spec)
{
    PHCellRendererFacts *renderer = PH_CELL_RENDERER_FACTS(obj);

    switch (id) {
    case PH_CELL_RENDERER_FACTS_PROP_SHOW:
        g_value_set_uint(value, renderer->priv->show);
        break;
    case PH_CELL_RENDERER_FACTS_PROP_SIZE:
        g_value_set_uint(value, renderer->priv->size);
        break;
    case PH_CELL_RENDERER_FACTS_PROP_DIFFICULTY:
        g_value_set_uint(value, renderer->priv->difficulty);
        break;
    case PH_CELL_RENDERER_FACTS_PROP_TERRAIN:
        g_value_set_uint(value, renderer->priv->terrain);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
    }
}

/*
 * Property mutator.
 */
static void
ph_cell_renderer_facts_set_property(GObject *obj,
                                    guint id,
                                    const GValue *value,
                                    GParamSpec *spec)
{
    PHCellRendererFacts *renderer = PH_CELL_RENDERER_FACTS(obj);

    switch (id) {
    case PH_CELL_RENDERER_FACTS_PROP_SHOW:
        renderer->priv->show = g_value_get_uint(value) &
            PH_CELL_RENDERER_FACTS_SHOW_ALL;
        ph_cell_renderer_facts_calculate_size(PH_CELL_RENDERER_FACTS(obj));
        break;
    case PH_CELL_RENDERER_FACTS_PROP_SIZE:
        renderer->priv->size = g_value_get_uint(value);
        ph_cell_renderer_facts_calculate_size(PH_CELL_RENDERER_FACTS(obj));
        break;
    case PH_CELL_RENDERER_FACTS_PROP_DIFFICULTY:
        renderer->priv->difficulty = g_value_get_uint(value);
        break;
    case PH_CELL_RENDERER_FACTS_PROP_TERRAIN:
        renderer->priv->terrain = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
    }
}

/* Size calculation {{{1 */

/*
 * Get the size required to display the cell.
 */
static void
ph_cell_renderer_facts_get_size(GtkCellRenderer *cr,
                                GtkWidget *widget,
                                GdkRectangle *area,
                                gint *xoff,
                                gint *yoff,
                                gint *width,
                                gint *height)
{
    PHCellRendererFacts *renderer = PH_CELL_RENDERER_FACTS(cr);

    if (width != NULL)
        *width = renderer->priv->width;
    if (height != NULL)
        *height = renderer->priv->total_height;
}

/*
 * Determine the pixel size of a single sprite.
 */
static void
ph_cell_renderer_facts_calculate_size(PHCellRendererFacts *renderer)
{
    guint show = renderer->priv->show;

    ph_sprite_get_dimensions(PH_SPRITE_SIZE, renderer->priv->sprite_size,
            &renderer->priv->width, &renderer->priv->height);

    renderer->priv->total_height = 0;
    while (show != 0) {         /* for all 1 bits in show */
        if ((show & 1) != 0)
            renderer->priv->total_height += renderer->priv->height;
        show >>= 1;
    }
}

/* Rendering {{{1 */

/*
 * Render the cell.
 */
static void
ph_cell_renderer_facts_render(GtkCellRenderer *cr,
                              GdkDrawable *window,
                              GtkWidget *widget,
                              GdkRectangle *bg_area,
                              GdkRectangle *cell_area,
                              GdkRectangle *expose_area,
                              GtkCellRendererState flags)
{
    PHCellRendererFacts *renderer = PH_CELL_RENDERER_FACTS(cr);
    gint xoff, yoff;
    cairo_t *cairo;

    xoff = (cell_area->width - renderer->priv->width) / 2;
    yoff = (cell_area->height - renderer->priv->total_height) / 2;

    cairo = gdk_cairo_create(window);

    if (renderer->priv->show & PH_CELL_RENDERER_FACTS_SHOW_SIZE) {
        ph_sprite_draw(PH_SPRITE_SIZE, renderer->priv->sprite_size,
                renderer->priv->size,
                cairo, 1.0, cell_area->x + xoff, cell_area->y + yoff,
                NULL, NULL);
        yoff += renderer->priv->height;
    }
    if (renderer->priv->show & PH_CELL_RENDERER_FACTS_SHOW_DIFFICULTY) {
        ph_sprite_draw(PH_SPRITE_DIFFICULTY, renderer->priv->sprite_size,
                renderer->priv->difficulty,
                cairo, 1.0, cell_area->x + xoff, cell_area->y + yoff,
                NULL, NULL);
        yoff += renderer->priv->height;
    }
    if (renderer->priv->show & PH_CELL_RENDERER_FACTS_SHOW_TERRAIN)
        ph_sprite_draw(PH_SPRITE_TERRAIN, renderer->priv->sprite_size,
                renderer->priv->terrain,
                cairo, 1.0, cell_area->x + xoff, cell_area->y + yoff,
                NULL, NULL);

    cairo_destroy(cairo);
}

/* Public interface {{{1 */

/*
 * Create a new instance.  show determines which facts (size, difficulty
 * and/or terrain) should be shown; for the ORable values see the
 * PH_CELL_RENDERER_FACTS_SHOW_* enum in the header file.
 */
GtkCellRenderer *
ph_cell_renderer_facts_new(guint show,
                           PHSpriteSize sprite_size)
{
    PHCellRendererFacts *renderer = g_object_new(
            PH_TYPE_CELL_RENDERER_FACTS, NULL);

    renderer->priv->show = show & PH_CELL_RENDERER_FACTS_SHOW_ALL;
    renderer->priv->sprite_size = sprite_size;
    ph_cell_renderer_facts_calculate_size(renderer);

    return GTK_CELL_RENDERER(renderer);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
