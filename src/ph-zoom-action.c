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

#include "ph-zoom-action.h"
#include "ph-zoom-tool-item.h"

/* Properties {{{1 */

enum {
    PH_ZOOM_ACTION_PROP_0,
    PH_ZOOM_ACTION_PROP_ADJUSTMENT
};

/* Private data {{{1 */

#define PH_ZOOM_ACTION_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), PH_TYPE_ZOOM_ACTION, \
                                 PHZoomActionPrivate))

struct _PHZoomActionPrivate {
    GtkAdjustment *adjustment;              /* underlying zoom adjustment */
};

/* Forward declarations {{{1 */

static void ph_zoom_action_class_init(PHZoomActionClass *cls);
static void ph_zoom_action_init(PHZoomAction *action);
static void ph_zoom_action_dispose(GObject *object);
static void ph_zoom_action_set_property(GObject *object, guint id,
                                        const GValue *value, GParamSpec *param);
static void ph_zoom_action_get_property(GObject *object, guint id,
                                        GValue *value, GParamSpec *param);

static void ph_zoom_action_connect_proxy(GtkAction *parent_action,
                                         GtkWidget *proxy);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHZoomAction, ph_zoom_action, GTK_TYPE_ACTION)

/*
 * Class initialization code.
 */
static void
ph_zoom_action_class_init(PHZoomActionClass *cls)
{
    GObjectClass *g_obj_cls = G_OBJECT_CLASS(cls);
    GtkActionClass *action_cls = GTK_ACTION_CLASS(cls);

    g_obj_cls->dispose = ph_zoom_action_dispose;
    g_obj_cls->set_property = ph_zoom_action_set_property;
    g_obj_cls->get_property = ph_zoom_action_get_property;

    action_cls->toolbar_item_type = PH_TYPE_ZOOM_TOOL_ITEM;
    action_cls->connect_proxy = ph_zoom_action_connect_proxy;

    g_object_class_install_property(g_obj_cls, PH_ZOOM_ACTION_PROP_ADJUSTMENT,
            g_param_spec_object("adjustment", "zoom adjustment",
                "underlying adjustment associated with the map",
                GTK_TYPE_ADJUSTMENT,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_type_class_add_private(cls, sizeof(PHZoomActionPrivate));
}

/*
 * Instance initialization code.
 */
static void
ph_zoom_action_init(PHZoomAction *action)
{
    PHZoomActionPrivate *priv = PH_ZOOM_ACTION_GET_PRIVATE(action);

    action->priv = priv;
}

/*
 * Drop references to other objects.
 */
static void
ph_zoom_action_dispose(GObject *object)
{
    PHZoomAction *action = PH_ZOOM_ACTION(object);

    if (action->priv->adjustment != NULL) {
        g_object_unref(action->priv->adjustment);
        action->priv->adjustment = NULL;
    }

    if (G_OBJECT_CLASS(ph_zoom_action_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(ph_zoom_action_parent_class)->dispose(object);
}

/*
 * Property mutator.
 */
static void
ph_zoom_action_set_property(GObject *object,
                            guint id,
                            const GValue *value,
                            GParamSpec *param)
{
    PHZoomAction *action = PH_ZOOM_ACTION(object);

    switch (id) {
    case PH_ZOOM_ACTION_PROP_ADJUSTMENT:
        ph_zoom_action_set_adjustment(action,
                GTK_ADJUSTMENT(g_value_get_object(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, param);
    }
}

/*
 * Property accessor.
 */
static void
ph_zoom_action_get_property(GObject *object,
                            guint id,
                            GValue *value,
                            GParamSpec *param)
{
}

/* Action implementation {{{1 */

/*
 * Connect the zoom slider widget to the adjustment.
 */
static void
ph_zoom_action_connect_proxy(GtkAction *parent_action,
                             GtkWidget *proxy)
{
    PHZoomAction *action = PH_ZOOM_ACTION(parent_action);
    PHZoomToolItem *item = PH_ZOOM_TOOL_ITEM(proxy);

    ph_zoom_tool_item_set_adjustment(item, action->priv->adjustment);

    if (GTK_ACTION_CLASS(ph_zoom_action_parent_class)->connect_proxy != NULL)
        GTK_ACTION_CLASS(ph_zoom_action_parent_class)->connect_proxy(
                parent_action, proxy);
}

/* Public interface {{{1 */

/*
 * Create a new zoom action instance with the specified name.
 */
GtkAction *
ph_zoom_action_new(const gchar *name,
                   GtkAdjustment *adjustment)
{
    g_return_val_if_fail(name != NULL, NULL);
    g_return_val_if_fail(adjustment == NULL || GTK_IS_ADJUSTMENT(adjustment),
            NULL);

    return GTK_ACTION(g_object_new(PH_TYPE_ZOOM_ACTION,
                "name", name,
                "adjustment", adjustment,
                NULL));
}

/*
 * Set the zoom adjustment on the action and all its proxies.
 */
void
ph_zoom_action_set_adjustment(PHZoomAction *action,
                              GtkAdjustment *adjustment)
{
    GSList *proxies;

    g_return_if_fail(action != NULL && PH_IS_ZOOM_ACTION(action));
    g_return_if_fail(adjustment == NULL || GTK_IS_ADJUSTMENT(adjustment));

    if (action->priv->adjustment == adjustment)
        return;

    if (action->priv->adjustment != NULL)
        g_object_unref(action->priv->adjustment);

    action->priv->adjustment = adjustment;
    if (action->priv->adjustment != NULL)
        g_object_ref(action->priv->adjustment);

    proxies = gtk_action_get_proxies(GTK_ACTION(action));
    while (proxies != NULL) {
        PHZoomToolItem *item = PH_ZOOM_TOOL_ITEM(proxies->data);
        ph_zoom_tool_item_set_adjustment(item, action->priv->adjustment);
        proxies = proxies->next;
    }
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
