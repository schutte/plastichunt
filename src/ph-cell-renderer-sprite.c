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

#include "ph-cell-renderer-sprite.h"
#include "ph-sprite.h"
#include <glib/gi18n.h>

/* Properties {{{1 */

/*
 * Properties.
 */
enum {
    PH_CELL_RENDERER_SPRITE_PROP_0,
    PH_CELL_RENDERER_SPRITE_PROP_SPRITE,
    PH_CELL_RENDERER_SPRITE_PROP_SIZE,
    PH_CELL_RENDERER_SPRITE_PROP_VALUE
};

/* Private data {{{1 */

#define PH_CELL_RENDERER_SPRITE_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), PH_TYPE_CELL_RENDERER_SPRITE, \
				 PHCellRendererSpritePrivate))

/*
 * Private data.
 */
struct _PHCellRendererSpritePrivate {
    PHSprite sprite;
    PHSpriteSize size;
    guint value;
    gint width, height;
};

/* Forward declarations {{{1 */

static void ph_cell_renderer_sprite_class_init(
    PHCellRendererSpriteClass *klass);
static void ph_cell_renderer_sprite_init(PHCellRendererSprite *renderer);
static void ph_cell_renderer_sprite_get_property(
    GObject *obj, guint id, GValue *value, GParamSpec *spec);
static void ph_cell_renderer_sprite_set_property(
    GObject *obj, guint id, const GValue *value, GParamSpec *spec);

static void ph_cell_renderer_sprite_get_size(
    GtkCellRenderer *cr, GtkWidget *widget, GdkRectangle *area,
    gint *xoff, gint *yoff, gint *width, gint *height);
static void ph_cell_renderer_sprite_calculate_size(
    PHCellRendererSprite *renderer);

static void ph_cell_renderer_sprite_render(
    GtkCellRenderer *cr, GdkDrawable *window, GtkWidget *widget,
    GdkRectangle *bg_area, GdkRectangle *cell_area, GdkRectangle *expose_area,
    GtkCellRendererState flags);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHCellRendererSprite, ph_cell_renderer_sprite,
              GTK_TYPE_CELL_RENDERER)

/*
 * Class initialization code.
 */
static void
ph_cell_renderer_sprite_class_init(PHCellRendererSpriteClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GtkCellRendererClass *cr_class = GTK_CELL_RENDERER_CLASS(klass);

    g_object_class->get_property = ph_cell_renderer_sprite_get_property;
    g_object_class->set_property = ph_cell_renderer_sprite_set_property;

    cr_class->get_size = ph_cell_renderer_sprite_get_size;
    cr_class->render = ph_cell_renderer_sprite_render;

    g_type_class_add_private(klass, sizeof(PHCellRendererSpritePrivate));

    g_object_class_install_property(g_object_class,
            PH_CELL_RENDERER_SPRITE_PROP_SPRITE,
            g_param_spec_uint("sprite",
                "sprite type",
                "type of PHSprite to display",
                0, PH_SPRITE_COUNT - 1, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(g_object_class,
            PH_CELL_RENDERER_SPRITE_PROP_SIZE,
            g_param_spec_uint("size",
                "sprite size",
                "size constant from PHSpriteSize",
                0, PH_SPRITE_SIZE_COUNT - 1, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(g_object_class,
            PH_CELL_RENDERER_SPRITE_PROP_VALUE,
            g_param_spec_uint("value",
                "sprite value",
                "number of the PHSprite part to render",
                0, G_MAXUINT, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE));
}

/*
 * Instance initialization code.
 */
static void
ph_cell_renderer_sprite_init(PHCellRendererSprite *renderer)
{
    renderer->priv = PH_CELL_RENDERER_SPRITE_GET_PRIVATE(renderer);
}

/*
 * Property accessor.
 */
static void
ph_cell_renderer_sprite_get_property(GObject *obj,
                                     guint id,
                                     GValue *value,
                                     GParamSpec *spec)
{
    PHCellRendererSprite *renderer = PH_CELL_RENDERER_SPRITE(obj);

    switch (id) {
    case PH_CELL_RENDERER_SPRITE_PROP_SPRITE:
        g_value_set_uint(value, renderer->priv->sprite);
        break;
    case PH_CELL_RENDERER_SPRITE_PROP_SIZE:
        g_value_set_uint(value, renderer->priv->size);
        break;
    case PH_CELL_RENDERER_SPRITE_PROP_VALUE:
        g_value_set_uint(value, renderer->priv->value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
    }
}

/*
 * Property mutator.
 */
static void
ph_cell_renderer_sprite_set_property(GObject *obj,
                                     guint id,
                                     const GValue *value,
                                     GParamSpec *spec)
{
    PHCellRendererSprite *renderer = PH_CELL_RENDERER_SPRITE(obj);

    switch (id) {
    case PH_CELL_RENDERER_SPRITE_PROP_SPRITE:
        renderer->priv->sprite = g_value_get_uint(value);
        ph_cell_renderer_sprite_calculate_size(PH_CELL_RENDERER_SPRITE(obj));
        break;
    case PH_CELL_RENDERER_SPRITE_PROP_SIZE:
        renderer->priv->size = g_value_get_uint(value);
        ph_cell_renderer_sprite_calculate_size(PH_CELL_RENDERER_SPRITE(obj));
        break;
    case PH_CELL_RENDERER_SPRITE_PROP_VALUE:
        renderer->priv->value = g_value_get_uint(value);
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
ph_cell_renderer_sprite_get_size(GtkCellRenderer *cr,
                                 GtkWidget *widget,
                                 GdkRectangle *area,
                                 gint *xoff,
                                 gint *yoff,
                                 gint *width,
                                 gint *height)
{
    PHCellRendererSprite *renderer = PH_CELL_RENDERER_SPRITE(cr);

    if (width != NULL)
        *width = renderer->priv->width;
    if (height != NULL)
        *height = renderer->priv->height;
}

/*
 * Update the stored dimensions of the sprite.
 */
static void
ph_cell_renderer_sprite_calculate_size(PHCellRendererSprite *renderer)
{
    ph_sprite_get_dimensions(renderer->priv->sprite, renderer->priv->size,
            &renderer->priv->width, &renderer->priv->height);
}

/* Rendering {{{1 */

/*
 * Render the cell.
 */
static void
ph_cell_renderer_sprite_render(GtkCellRenderer *cr,
                               GdkDrawable *window,
                               GtkWidget *widget,
                               GdkRectangle *bg_area,
                               GdkRectangle *cell_area,
                               GdkRectangle *expose_area,
                               GtkCellRendererState flags)
{
    PHCellRendererSprite *renderer = PH_CELL_RENDERER_SPRITE(cr);
    gint xoff, yoff;
    cairo_t *cairo;

    xoff = (cell_area->width - renderer->priv->width) / 2;
    yoff = (cell_area->height - renderer->priv->height) / 2;

    cairo = gdk_cairo_create(window);
    ph_sprite_draw(renderer->priv->sprite, renderer->priv->size,
            renderer->priv->value, cairo, 1.0,
            cell_area->x + xoff, cell_area->y + yoff,
            NULL, NULL);
    cairo_destroy(cairo);
}

/* Public interface {{{1 */

/*
 * Create a new instance.
 */
GtkCellRenderer *
ph_cell_renderer_sprite_new(PHSprite sprite,
                            PHSpriteSize size)
{
    PHCellRendererSprite *renderer = g_object_new(
            PH_TYPE_CELL_RENDERER_SPRITE, NULL);

    renderer->priv->sprite = sprite;
    renderer->priv->size = size;
    ph_cell_renderer_sprite_calculate_size(renderer);

    return GTK_CELL_RENDERER(renderer);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
