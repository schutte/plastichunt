/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_MAP_PROVIDER_TOOL_ITEM_H
#define PH_MAP_PROVIDER_TOOL_ITEM_H

/* Includes {{{1 */

#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_MAP_PROVIDER_TOOL_ITEM (ph_map_provider_tool_item_get_type())
#define PH_MAP_PROVIDER_TOOL_ITEM(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_MAP_PROVIDER_TOOL_ITEM, \
                                PHMapProviderToolItem))
#define PH_MAP_PROVIDER_TOOL_ITEM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), PH_TYPE_MAP_PROVIDER_TOOL_ITEM, \
                             PHMapProviderToolItemClass))
#define PH_IS_MAP_PROVIDER_TOOL_ITEM(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_MAP_PROVIDER_TOOL_ITEM))
#define PH_IS_MAP_PROVIDER_TOOL_ITEM_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), PH_TYPE_MAP_PROVIDER_TOOL_ITEM))
#define PH_MAP_PROVIDER_TOOL_ITEM_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_MAP_PROVIDER_TOOL_ITEM, \
                               PHMapProviderToolItemClass))

GType ph_map_provider_tool_item_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHMapProviderToolItem PHMapProviderToolItem;
typedef struct _PHMapProviderToolItemClass PHMapProviderToolItemClass;
typedef struct _PHMapProviderToolItemPrivate PHMapProviderToolItemPrivate;

struct _PHMapProviderToolItem {
    GtkToolItem parent;
    PHMapProviderToolItemPrivate *priv;
};

struct _PHMapProviderToolItemClass {
    GtkToolItemClass parent_class;

    /* signals */
    void (*changed)(PHMapProviderToolItem *item, gint index);
};

/* Public interface {{{1 */

GtkToolItem *ph_map_provider_tool_item_new();

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
