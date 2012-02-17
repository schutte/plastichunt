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
#include "ph-map-provider-action.h"
#include "ph-map-provider-tool-item.h"

/* Signals {{{1 */

enum {
    PH_MAP_PROVIDER_ACTION_SIGNAL_CHANGED,
    PH_MAP_PROVIDER_ACTION_SIGNAL_COUNT
};

static guint ph_map_provider_action_signals[
    PH_MAP_PROVIDER_ACTION_SIGNAL_COUNT];

/* Private data {{{1 */

#define PH_MAP_PROVIDER_ACTION_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), PH_TYPE_MAP_PROVIDER_ACTION, \
                                 PHMapProviderActionPrivate))

struct _PHMapProviderActionPrivate {
    gint selected_index;                /* index of the chosen provider */
    gulong list_handlers[2];            /* map provider modification handlers */
};

/* Forward declarations {{{1 */

static void ph_map_provider_action_class_init(PHMapProviderActionClass *cls);
static void ph_map_provider_action_init(PHMapProviderAction *action);
static void ph_map_provider_action_dispose(GObject *object);

static void ph_map_provider_action_connect_proxy(GtkAction *parent_action,
                                                 GtkWidget *proxy);

static void ph_map_provider_action_selected(GtkWidget *widget, gint item,
                                            gpointer data);
static void ph_map_provider_action_row_changed(
    GtkTreeModel *tree_model, GtkTreePath *path,
    GtkTreeIter *iter, gpointer data);
static void ph_map_provider_action_row_deleted(
    GtkTreeModel *tree_model, GtkTreePath *path, gpointer data);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHMapProviderAction, ph_map_provider_action, GTK_TYPE_ACTION)

/*
 * Class initialization code.
 */
static void
ph_map_provider_action_class_init(PHMapProviderActionClass *cls)
{
    GObjectClass *g_obj_cls = G_OBJECT_CLASS(cls);
    GtkActionClass *action_cls = GTK_ACTION_CLASS(cls);

    g_obj_cls->dispose = ph_map_provider_action_dispose;

    action_cls->toolbar_item_type = PH_TYPE_MAP_PROVIDER_TOOL_ITEM;
    action_cls->connect_proxy = ph_map_provider_action_connect_proxy;

    ph_map_provider_action_signals[PH_MAP_PROVIDER_ACTION_SIGNAL_CHANGED] =
        g_signal_new("changed", PH_TYPE_MAP_PROVIDER_ACTION,
                G_SIGNAL_RUN_FIRST,
                G_STRUCT_OFFSET(PHMapProviderActionClass, changed),
                NULL, NULL,
                g_cclosure_marshal_VOID__BOXED,
                G_TYPE_NONE, 1, GTK_TYPE_TREE_PATH);

    g_type_class_add_private(cls, sizeof(PHMapProviderActionPrivate));
}

/*
 * Instance initialization code.
 */
static void
ph_map_provider_action_init(PHMapProviderAction *action)
{
    PHMapProviderActionPrivate *priv =
        PH_MAP_PROVIDER_ACTION_GET_PRIVATE(action);
    GtkListStore *map_providers = ph_config_get_map_providers();

    action->priv = priv;
    action->priv->selected_index = -1;

    action->priv->list_handlers[0] =
        g_signal_connect(map_providers, "row-changed",
                G_CALLBACK(ph_map_provider_action_row_changed), action);
    action->priv->list_handlers[1] =
        g_signal_connect(map_providers, "row-deleted",
                G_CALLBACK(ph_map_provider_action_row_deleted), action);
}

/*
 * Drop references to other objects.
 */
static void
ph_map_provider_action_dispose(GObject *object)
{
    PHMapProviderAction *action = PH_MAP_PROVIDER_ACTION(object);
    GtkListStore *map_providers = ph_config_get_map_providers();
    guint i;

    for (i = 0; i < G_N_ELEMENTS(action->priv->list_handlers); ++i) {
        gulong *handler = &action->priv->list_handlers[i];
        if (*handler == 0)
            continue;
        g_signal_handler_disconnect(map_providers, *handler);
        *handler = 0;
    }
}

/* Action implementation {{{1 */

/*
 * Connect the zoom slider widget to the adjustment.
 */
static void
ph_map_provider_action_connect_proxy(GtkAction *parent_action,
                                     GtkWidget *proxy)
{
    PHMapProviderAction *action = PH_MAP_PROVIDER_ACTION(parent_action);

    g_signal_connect(proxy, "changed",
            G_CALLBACK(ph_map_provider_action_selected), action);
    g_object_set(proxy, "selected-index", action->priv->selected_index, NULL);

    if (GTK_ACTION_CLASS(ph_map_provider_action_parent_class)->connect_proxy !=
            NULL)
        GTK_ACTION_CLASS(ph_map_provider_action_parent_class)->connect_proxy(
                parent_action, proxy);
}

/* Signal handlers {{{1 */

/*
 * Further propagate a map provider change via the "changed" signal.
 */
static void
ph_map_provider_action_selected(GtkWidget *widget,
                                gint item,
                                gpointer data)
{
    PHMapProviderAction *action = PH_MAP_PROVIDER_ACTION(data);

    if (item == -1)
        return;

    ph_map_provider_action_set_selected_index(action, item);
}

/*
 * When the settings of the currently selected map provider have changed,
 * trigger its selection again, in order to ultimately notify the map of the
 * modifications.
 */
static void
ph_map_provider_action_row_changed(GtkTreeModel *tree_model,
                                   GtkTreePath *path,
                                   GtkTreeIter *iter,
                                   gpointer data)
{
    PHMapProviderAction *action = PH_MAP_PROVIDER_ACTION(data);
    gint *indices = gtk_tree_path_get_indices(path);

    if (indices[0] != action->priv->selected_index)
        return;

    action->priv->selected_index = -1;
    ph_map_provider_action_set_selected_index(action, indices[0]);
}

/*
 * When the currently selected map provider is deleted, select another one.
 */
static void
ph_map_provider_action_row_deleted(GtkTreeModel *tree_model,
                                   GtkTreePath *path,
                                   gpointer data)
{
    PHMapProviderAction *action = PH_MAP_PROVIDER_ACTION(data);
    gint *indices = gtk_tree_path_get_indices(path);
    GtkTreeModel *model = GTK_TREE_MODEL(ph_config_get_map_providers());
    gint count = gtk_tree_model_iter_n_children(model, NULL);

    if (indices[0] != action->priv->selected_index)
        return;

    if (indices[0] >= count)
        ph_map_provider_action_set_selected_index(action, count - 1);
    else {
        action->priv->selected_index = -1;
        ph_map_provider_action_set_selected_index(action, indices[0]);
    }
}

/* Public interface {{{1 */

/*
 * Create a new zoom action instance with the specified name.
 */
GtkAction *
ph_map_provider_action_new(const gchar *name)
{
    g_return_val_if_fail(name != NULL, NULL);

    return GTK_ACTION(g_object_new(PH_TYPE_MAP_PROVIDER_ACTION,
                "name", name,
                NULL));
}

/*
 * Change the selection.
 */
void
ph_map_provider_action_set_selected_index(PHMapProviderAction *action,
                                          gint index)
{
    GSList *proxies;

    g_return_if_fail(action != NULL && PH_IS_MAP_PROVIDER_ACTION(action));

    if (action->priv->selected_index == index)
        return;

    action->priv->selected_index = index;

    proxies = gtk_action_get_proxies(GTK_ACTION(action));
    while (proxies != NULL) {
        g_object_set(GTK_WIDGET(proxies->data),
                "selected-index", index,
                NULL);
        proxies = proxies->next;
    }

    if (index != -1) {
        GtkTreePath *path;

        path = gtk_tree_path_new_from_indices(index, -1);
        g_signal_emit(action, ph_map_provider_action_signals[
                PH_MAP_PROVIDER_ACTION_SIGNAL_CHANGED], 0, path);
        gtk_tree_path_free(path);
    }
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
