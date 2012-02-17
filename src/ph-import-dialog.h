
/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_IMPORT_DIALOG_H
#define PH_IMPORT_DIALOG_H

/* Includes {{{1 */

#include "ph-database.h"
#include "ph-process-dialog.h"
#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_IMPORT_DIALOG \
    (ph_import_dialog_get_type())
#define PH_IMPORT_DIALOG(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_IMPORT_DIALOG, PHImportDialog))
#define PH_IS_IMPORT_DIALOG(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_IMPORT_DIALOG))
#define PH_IMPORT_DIALOG_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST((cls), PH_TYPE_IMPORT_DIALOG, PHImportDialogClass))
#define PH_IS_IMPORT_DIALOG_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE((cls), PH_TYPE_IMPORT_DIALOG))
#define PH_IMPORT_DIALOG_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_IMPORT_DIALOG, \
                               PHImportDialogClass))

GType ph_import_dialog_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHImportDialog PHImportDialog;
typedef struct _PHImportDialogClass PHImportDialogClass;
typedef struct _PHImportDialogPrivate PHImportDialogPrivate;

struct _PHImportDialog {
    PHProcessDialog parent;
    PHImportDialogPrivate *priv;
};

struct _PHImportDialogClass {
    PHProcessDialogClass parent_class;
};

/* Public interface {{{1 */

GtkWidget *ph_import_dialog_new(GtkWindow *parent,
                                PHDatabase *database);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
