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

#include "ph-sprite-image.h"

/* Properties {{{1 */

/*
 * Properties.
 */
enum {
    PH_SPRITE_IMAGE_PROP_0,
    PH_SPRITE_IMAGE_PROP_SPRITE,
    PH_SPRITE_IMAGE_PROP_SIZE,
    PH_SPRITE_IMAGE_PROP_VALUE
};

/* Private data {{{1 */

#define PH_SPRITE_IMAGE_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), PH_TYPE_SPRITE_IMAGE, \
                                 PHSpriteImagePrivate))

/*
 * Implementation details.
 */
struct _PHSpriteImagePrivate {
    PHSprite sprite;
    PHSpriteSize size;
    guint value;
};

/* Forward declarations {{{1 */

static void ph_sprite_image_class_init(PHSpriteImageClass *cls);
static void ph_sprite_image_init(PHSpriteImage *image);
static void ph_sprite_image_get_property(GObject *obj, guint id,
                                         GValue *value, GParamSpec *spec);
static void ph_sprite_image_set_property(GObject *obj, guint id,
                                         const GValue *value, GParamSpec *spec);

static gboolean ph_sprite_image_expose(GtkWidget *widget,
                                       GdkEventExpose *event);

static void ph_sprite_image_set_size_request(PHSpriteImage *image);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHSpriteImage, ph_sprite_image, GTK_TYPE_WIDGET)

/*
 * Class initialization code.
 */
static void
ph_sprite_image_class_init(PHSpriteImageClass *cls)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(cls);
    GtkWidgetClass *gtk_widget_class = GTK_WIDGET_CLASS(cls);

    g_object_class->get_property = ph_sprite_image_get_property;
    g_object_class->set_property = ph_sprite_image_set_property;

    gtk_widget_class->expose_event = ph_sprite_image_expose;

    g_type_class_add_private(cls, sizeof(PHSpriteImagePrivate));

    g_object_class_install_property(g_object_class,
            PH_SPRITE_IMAGE_PROP_SPRITE,
            g_param_spec_uint("sprite",
                "sprite type",
                "type of PHSprite to display",
                0, PH_SPRITE_COUNT - 1, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(g_object_class,
            PH_SPRITE_IMAGE_PROP_SIZE,
            g_param_spec_uint("size",
                "sprite size",
                "size constant from PHSpriteSize",
                0, PH_SPRITE_SIZE_COUNT - 1, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(g_object_class,
            PH_SPRITE_IMAGE_PROP_VALUE,
            g_param_spec_uint("value",
                "sprite value",
                "number of the PHSprite part to render",
                0, G_MAXUINT, 0,
                G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
}

/*
 * Instance initialization code.
 */
static void
ph_sprite_image_init(PHSpriteImage *image)
{
    PHSpriteImagePrivate *priv = PH_SPRITE_IMAGE_GET_PRIVATE(image);
    GtkWidget *widget = GTK_WIDGET(image);

    image->priv = priv;

    gtk_widget_set_has_window(widget, FALSE);
}

/*
 * Property accessor.
 */
static void
ph_sprite_image_get_property(GObject *obj,
                             guint id,
                             GValue *value,
                             GParamSpec *spec)
{
    PHSpriteImage *image = PH_SPRITE_IMAGE(obj);

    switch (id) {
    case PH_SPRITE_IMAGE_PROP_SPRITE:
        g_value_set_uint(value, image->priv->sprite);
        break;
    case PH_SPRITE_IMAGE_PROP_SIZE:
        g_value_set_uint(value, image->priv->size);
        break;
    case PH_SPRITE_IMAGE_PROP_VALUE:
        g_value_set_uint(value, image->priv->value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
    }
}

/*
 * Property mutator.
 */
static void
ph_sprite_image_set_property(GObject *obj,
                             guint id,
                             const GValue *value,
                             GParamSpec *spec)
{
    PHSpriteImage *image = PH_SPRITE_IMAGE(obj);

    switch (id) {
    case PH_SPRITE_IMAGE_PROP_SPRITE:
        ph_sprite_image_set_sprite(image, g_value_get_uint(value));
        break;
    case PH_SPRITE_IMAGE_PROP_SIZE:
        ph_sprite_image_set_size(image, g_value_get_uint(value));
        break;
    case PH_SPRITE_IMAGE_PROP_VALUE:
        ph_sprite_image_set_value(image, g_value_get_uint(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
    }
}

/* Drawing {{{1 */

/*
 * Draw the part of the sprite specified by value at the given size.
 */
static gboolean
ph_sprite_image_expose(GtkWidget *widget,
                       GdkEventExpose *event)
{
    PHSpriteImage *image = PH_SPRITE_IMAGE(widget);
    cairo_t *cr = gdk_cairo_create(widget->window);
    gint width, height;
    gint xoff, yoff;

    /* center the image */
    ph_sprite_get_dimensions(image->priv->sprite, image->priv->size,
            &width, &height);
    xoff = (widget->allocation.width - width) / 2;
    yoff = (widget->allocation.height - height) / 2;

    ph_sprite_draw(image->priv->sprite, image->priv->size, image->priv->value,
            cr, 1.0, widget->allocation.x + xoff, widget->allocation.y + yoff,
            NULL, NULL);

    cairo_destroy(cr);

    return FALSE;
}

/* Size request {{{1 */

/*
 * Set the size requisition of this widget according to the type and size of
 * the sprite to be displayed.
 */
static void
ph_sprite_image_set_size_request(PHSpriteImage *image)
{
    GtkWidget *widget = GTK_WIDGET(image);

    ph_sprite_get_dimensions(image->priv->sprite, image->priv->size,
            &widget->requisition.width, &widget->requisition.height);
    gtk_widget_queue_resize(widget);
}

/* Public interface {{{1 */

/*
 * Create a new PHSpriteImage instance.  PHSpriteImage is used to render
 * the icons provided by PHSprite, like GtkImage does for GDK pixmaps.
 */
GtkWidget *
ph_sprite_image_new(PHSprite sprite,
                    PHSpriteSize size)
{
    return GTK_WIDGET(g_object_new(PH_TYPE_SPRITE_IMAGE,
                "sprite", sprite,
                "size", size,
                NULL));
}

/*
 * Get the type of sprite to be displayed.
 */
PHSprite
ph_sprite_image_get_sprite(PHSpriteImage *image)
{
    g_return_val_if_fail(image != NULL && PH_IS_SPRITE_IMAGE(image), 0);
    return image->priv->sprite;
}

/*
 * Set the sprite type (geocache type, size, etc.) to be displayed.  This
 * might change the size request of the widget.
 */
void
ph_sprite_image_set_sprite(PHSpriteImage *image,
                           PHSprite sprite)
{
    g_return_if_fail(image != NULL && PH_IS_SPRITE_IMAGE(image));

    image->priv->sprite = sprite;
    ph_sprite_image_set_size_request(image);
    gtk_widget_queue_draw(GTK_WIDGET(image));
}

/*
 * Get the size of the sprite.
 */
PHSpriteSize
ph_sprite_image_get_size(PHSpriteImage *image)
{
    g_return_val_if_fail(image != NULL && PH_IS_SPRITE_IMAGE(image), 0);
    return image->priv->size;
}

/*
 * Set the size the sprite will be rendered at.  This is likely to change the
 * size request of the widget.
 */
void
ph_sprite_image_set_size(PHSpriteImage *image,
                         PHSpriteSize size)
{
    g_return_if_fail(image != NULL && PH_IS_SPRITE_IMAGE(image));

    image->priv->size = size;
    ph_sprite_image_set_size_request(image);
    gtk_widget_queue_draw(GTK_WIDGET(image));
}

/*
 * Get the value of the currently displayed part of the sprite.
 */
guint
ph_sprite_image_get_value(PHSpriteImage *image)
{
    g_return_val_if_fail(image != NULL && PH_IS_SPRITE_IMAGE(image), 0);
    return image->priv->value;
}

/*
 * Pick the part of the sprite to display.
 */
void
ph_sprite_image_set_value(PHSpriteImage *image,
                          guint value)
{
    g_return_if_fail(image != NULL && PH_IS_SPRITE_IMAGE(image));

    image->priv->value = value;
    gtk_widget_queue_draw(GTK_WIDGET(image));
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
