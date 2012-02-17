/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_MAP_PROVIDER_ACTION_H
#define PH_MAP_PROVIDER_ACTION_H

/* Includes {{{1 */

#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_MAP_PROVIDER_ACTION (ph_map_provider_action_get_type())
#define PH_MAP_PROVIDER_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_MAP_PROVIDER_ACTION, \
                                PHMapProviderAction))
#define PH_MAP_PROVIDER_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), PH_TYPE_MAP_PROVIDER_ACTION, \
                             PHMapProviderActionClass))
#define PH_IS_MAP_PROVIDER_ACTION(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_MAP_PROVIDER_ACTION))
#define PH_IS_MAP_PROVIDER_ACTION_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), PH_TYPE_MAP_PROVIDER_ACTION))
#define PH_MAP_PROVIDER_ACTION_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_MAP_PROVIDER_ACTION, \
                               PHMapProviderActionClass))

GType ph_map_provider_action_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHMapProviderAction PHMapProviderAction;
typedef struct _PHMapProviderActionClass PHMapProviderActionClass;
typedef struct _PHMapProviderActionPrivate PHMapProviderActionPrivate;

struct _PHMapProviderAction {
    GtkAction parent;
    PHMapProviderActionPrivate *priv;
};

struct _PHMapProviderActionClass {
    GtkActionClass parent_class;

    /* signals */
    void (*changed)(PHMapProviderAction *action, GtkTreePath *selection);
};

/* Public interface {{{1 */

GtkAction *ph_map_provider_action_new(const gchar *name);
void ph_map_provider_action_set_selected_index(PHMapProviderAction *action,
                                               gint index);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
