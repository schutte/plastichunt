/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_PROCESS_DIALOG_H
#define PH_PROCESS_DIALOG_H

/* Includes {{{1 */

#include "ph-process.h"
#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_PROCESS_DIALOG (ph_process_dialog_get_type())
#define PH_PROCESS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
            PH_TYPE_PROCESS_DIALOG, PHProcessDialog))
#define PH_IS_PROCESS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
            PH_TYPE_PROCESS_DIALOG))
#define PH_PROCESS_DIALOG_CLASS(cls) (G_TYPE_CHECK_CLASS_CAST((cls), \
            PH_TYPE_PROCESS_DIALOG, PHProcessDialogClass))
#define PH_IS_PROCESS_DIALOG_CLASS(cls) (G_TYPE_CHECK_CLASS_TYPE((cls), \
            PH_TYPE_PROCESS_DIALOG))
#define PH_PROCESS_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            PH_TYPE_PROCESS_DIALOG, PHProcessDialogClass))

GType ph_process_dialog_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHProcessDialog PHProcessDialog;
typedef struct _PHProcessDialogClass PHProcessDialogClass;
typedef struct _PHProcessDialogPrivate PHProcessDialogPrivate;

struct _PHProcessDialog {
    GtkDialog parent;
    PHProcessDialogPrivate *priv;
};

struct _PHProcessDialogClass {
    GtkDialogClass parent_class;

    /* virtual */
    PHProcess *(*create_process)(PHProcessDialog *dialog);
};

/* Public interface {{{1 */

void ph_process_dialog_run(PHProcessDialog *dialog);

GtkWidget *ph_process_dialog_get_content_vbox(PHProcessDialog *dialog);
GtkWidget *ph_process_dialog_get_button_box(PHProcessDialog *dialog);
GtkWidget *ph_process_dialog_get_status_vbox(PHProcessDialog *dialog);

void ph_process_dialog_set_status_text(PHProcessDialog *dialog,
                                       const gchar *format,
                                       ...);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
