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

#include "ph-zoom-tool-item.h"

/* Private data {{{1 */

#define PH_ZOOM_TOOL_ITEM_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), PH_TYPE_ZOOM_TOOL_ITEM, \
                                 PHZoomToolItemPrivate))

struct _PHZoomToolItemPrivate {
    GtkWidget *scale;                       /* child slider widget */
};

/* Forward declarations {{{1 */

static void ph_zoom_tool_item_class_init(PHZoomToolItemClass *cls);
static void ph_zoom_tool_item_init(PHZoomToolItem *item);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHZoomToolItem, ph_zoom_tool_item, GTK_TYPE_TOOL_ITEM)

/*
 * Class initialization code.
 */
static void
ph_zoom_tool_item_class_init(PHZoomToolItemClass *cls)
{
    g_type_class_add_private(cls, sizeof(PHZoomToolItemPrivate));
}

/*
 * Instance initialization code.
 */
static void
ph_zoom_tool_item_init(PHZoomToolItem *item)
{
    PHZoomToolItemPrivate *priv = PH_ZOOM_TOOL_ITEM_GET_PRIVATE(item);

    item->priv = priv;

    item->priv->scale = gtk_hscale_new(NULL);
    gtk_scale_set_draw_value(GTK_SCALE(item->priv->scale), FALSE);
    gtk_scale_set_digits(GTK_SCALE(item->priv->scale), 0);
    gtk_widget_set_size_request(item->priv->scale, 70, -1);
    gtk_widget_set_can_focus(item->priv->scale, FALSE);

    gtk_container_add(GTK_CONTAINER(item), item->priv->scale);
}

/* Public interface {{{1 */

/*
 * Create a new zoom slider tool item.
 */
GtkToolItem *
ph_zoom_tool_item_new()
{
    return GTK_TOOL_ITEM(g_object_new(PH_TYPE_ZOOM_TOOL_ITEM, NULL));
}

/*
 * Set the zoom adjustment on the slider widget.
 */
void
ph_zoom_tool_item_set_adjustment(PHZoomToolItem *item,
                                 GtkAdjustment *adjustment)
{
    g_return_if_fail(item != NULL && PH_IS_ZOOM_TOOL_ITEM(item));
    g_return_if_fail(adjustment == NULL || GTK_IS_ADJUSTMENT(adjustment));

    gtk_range_set_adjustment(GTK_RANGE(item->priv->scale), adjustment);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
