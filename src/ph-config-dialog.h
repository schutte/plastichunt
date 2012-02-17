/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_CONFIG_DIALOG_H
#define PH_CONFIG_DIALOG_H

/* Includes {{{1 */

#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_CONFIG_DIALOG \
    (ph_config_dialog_get_type())
#define PH_CONFIG_DIALOG(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_CONFIG_DIALOG, PHConfigDialog))
#define PH_IS_CONFIG_DIALOG(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_CONFIG_DIALOG))
#define PH_CONFIG_DIALOG_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST((cls), PH_TYPE_CONFIG_DIALOG, PHConfigDialogClass))
#define PH_IS_CONFIG_DIALOG_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE((cls), PH_TYPE_CONFIG_DIALOG))
#define PH_CONFIG_DIALOG_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_CONFIG_DIALOG, \
                               PHConfigDialogClass))

GType ph_config_dialog_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHConfigDialog PHConfigDialog;
typedef struct _PHConfigDialogClass PHConfigDialogClass;
typedef struct _PHConfigDialogPrivate PHConfigDialogPrivate;

struct _PHConfigDialog {
    GtkDialog parent;
    PHConfigDialogPrivate *priv;
};

struct _PHConfigDialogClass {
    GtkDialogClass parent_class;
};

/* Public interface {{{1 */

GtkWidget *ph_config_dialog_new();

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
