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
#include "ph-map-provider.h"
#include "ph-map-provider-tool-item.h"

/* Signals {{{1 */

enum {
    PH_MAP_PROVIDER_TOOL_ITEM_SIGNAL_CHANGED,
    PH_MAP_PROVIDER_TOOL_ITEM_COUNT
};

static guint ph_map_provider_tool_item_signals[
    PH_MAP_PROVIDER_TOOL_ITEM_COUNT];

/* Properties {{{1 */

enum {
    PH_MAP_PROVIDER_TOOL_ITEM_PROP_0,
    PH_MAP_PROVIDER_TOOL_ITEM_PROP_SELECTED_INDEX
};

/* Private data {{{1 */

#define PH_MAP_PROVIDER_TOOL_ITEM_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), PH_TYPE_MAP_PROVIDER_TOOL_ITEM, \
                                 PHMapProviderToolItemPrivate))

struct _PHMapProviderToolItemPrivate {
    GtkWidget *combo_box;                   /* underlying combo box */
};

/* Forward declarations {{{1 */

static void ph_map_provider_tool_item_class_init(
    PHMapProviderToolItemClass *cls);
static void ph_map_provider_tool_item_init(PHMapProviderToolItem *item);
static void ph_map_provider_tool_item_set_property(
    GObject *object, guint id, const GValue *value, GParamSpec *param);
static void ph_map_provider_tool_item_get_property(
    GObject *object, guint id, GValue *value, GParamSpec *param);

static void ph_map_provider_tool_item_selected(GtkComboBox *combo_box,
                                               gpointer data);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHMapProviderToolItem, ph_map_provider_tool_item,
        GTK_TYPE_TOOL_ITEM)

/*
 * Class initialization code.
 */
static void
ph_map_provider_tool_item_class_init(PHMapProviderToolItemClass *cls)
{
    GObjectClass *g_obj_cls = G_OBJECT_CLASS(cls);

    g_obj_cls->set_property = ph_map_provider_tool_item_set_property;
    g_obj_cls->get_property = ph_map_provider_tool_item_get_property;

    ph_map_provider_tool_item_signals[
            PH_MAP_PROVIDER_TOOL_ITEM_SIGNAL_CHANGED] =
        g_signal_new("changed", PH_TYPE_MAP_PROVIDER_TOOL_ITEM,
                G_SIGNAL_RUN_FIRST,
                G_STRUCT_OFFSET(PHMapProviderToolItemClass, changed),
                NULL, NULL,
                g_cclosure_marshal_VOID__INT,
                G_TYPE_NONE, 1, G_TYPE_INT);

    g_object_class_install_property(g_obj_cls,
            PH_MAP_PROVIDER_TOOL_ITEM_PROP_SELECTED_INDEX,
            g_param_spec_int("selected-index", "index of the selection",
                "index of the selected map provider in the tree model",
                -1, G_MAXINT, -1,
                G_PARAM_READWRITE));

    g_type_class_add_private(cls, sizeof(PHMapProviderToolItemPrivate));
}

/*
 * Instance initialization code.
 */
static void
ph_map_provider_tool_item_init(PHMapProviderToolItem *item)
{
    PHMapProviderToolItemPrivate *priv =
        PH_MAP_PROVIDER_TOOL_ITEM_GET_PRIVATE(item);
    GtkTreeModel *model;
    GtkCellRenderer *renderer;
    GtkWidget *vbox;

    item->priv = priv;

    model = GTK_TREE_MODEL(ph_config_get_map_providers());
    item->priv->combo_box = gtk_combo_box_new_with_model(model);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(item->priv->combo_box),
            renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(item->priv->combo_box),
            renderer, "text", PH_MAP_PROVIDER_COLUMN_NAME);

    vbox = gtk_vbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), item->priv->combo_box, TRUE, FALSE, 0);

    g_signal_connect(item->priv->combo_box, "changed",
            G_CALLBACK(ph_map_provider_tool_item_selected), item);

    gtk_container_add(GTK_CONTAINER(item), vbox);
}

/*
 * Property mutator.
 */
static void
ph_map_provider_tool_item_set_property(GObject *object,
                                       guint id,
                                       const GValue *value,
                                       GParamSpec *param)
{
    PHMapProviderToolItem *item = PH_MAP_PROVIDER_TOOL_ITEM(object);

    switch (id) {
    case PH_MAP_PROVIDER_TOOL_ITEM_PROP_SELECTED_INDEX:
        gtk_combo_box_set_active(GTK_COMBO_BOX(item->priv->combo_box),
                g_value_get_int(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, param);
    }
}

/*
 * Property accessor.
 */
static void
ph_map_provider_tool_item_get_property(GObject *object,
                                       guint id,
                                       GValue *value,
                                       GParamSpec *param)
{
    PHMapProviderToolItem *item = PH_MAP_PROVIDER_TOOL_ITEM(object);

    switch (id) {
    case PH_MAP_PROVIDER_TOOL_ITEM_PROP_SELECTED_INDEX:
        g_value_set_int(value, gtk_combo_box_get_active(
                    GTK_COMBO_BOX(item->priv->combo_box)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, param);
    }
}

/* Signal handlers {{{1 */

/*
 * Propagate a map provider change from the combo box through the "changed"
 * signal.
 */
static void
ph_map_provider_tool_item_selected(GtkComboBox *combo_box,
                                   gpointer data)
{
    PHMapProviderToolItem *item = PH_MAP_PROVIDER_TOOL_ITEM(data);
    gint index = gtk_combo_box_get_active(combo_box);

    g_signal_emit(item, ph_map_provider_tool_item_signals[
            PH_MAP_PROVIDER_TOOL_ITEM_SIGNAL_CHANGED], 0, index);
}

/* Public interface {{{1 */

/*
 * Create a new zoom slider tool item.
 */
GtkToolItem *
ph_map_provider_tool_item_new()
{
    return GTK_TOOL_ITEM(g_object_new(PH_TYPE_MAP_PROVIDER_TOOL_ITEM, NULL));
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
