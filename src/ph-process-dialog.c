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

#include "ph-process-dialog.h"
#include <glib/gi18n.h>

/* Private data {{{1 */

#define PH_PROCESS_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE(\
            (obj), PH_TYPE_PROCESS_DIALOG, PHProcessDialogPrivate))

struct _PHProcessDialogPrivate {
    PHProcess *process;             /* currently running process */
    GMainLoop *loop;                /* event loop for this dialog */

    GtkWidget *content_vbox;        /* configuration controls */
    GtkWidget *button_box;          /* horizontal button box */
    GtkWidget *status_vbox;         /* status area */
    GtkWidget *status_label;        /* textual progress information */
    GtkWidget *progress_bar;        /* progress bar */
};

/* Forward declarations {{{1 */

static void ph_process_dialog_class_init(PHProcessDialogClass *cls);
static void ph_process_dialog_init(PHProcessDialog *dialog);

static void ph_process_dialog_create_gui(PHProcessDialog *dialog);

static gboolean ph_process_dialog_delete(GtkWidget *widget, GdkEventAny *event);

static void ph_process_dialog_ok_clicked(GtkButton *button, gpointer data);
static void ph_process_dialog_close_clicked(GtkButton *button, gpointer data);
static void ph_process_dialog_cancel_clicked(GtkButton *button, gpointer data);

static void ph_process_dialog_progress_notify(PHProcess *process,
                                              gdouble fraction, gpointer data);
static void ph_process_dialog_error_notify(PHProcess *process, GError *error,
                                           gpointer data);
static void ph_process_dialog_stop_notify(PHProcess *process, gpointer data);

static void ph_process_dialog_stop(PHProcessDialog *dialog);

/* Standard GObject code {{{1 */

G_DEFINE_ABSTRACT_TYPE(PHProcessDialog, ph_process_dialog, GTK_TYPE_WINDOW)

/*
 * Class initialization code.
 */
static void
ph_process_dialog_class_init(PHProcessDialogClass *cls)
{
    GtkWidgetClass *widget_cls = GTK_WIDGET_CLASS(cls);

    widget_cls->delete_event = ph_process_dialog_delete;

    g_type_class_add_private(cls, sizeof(PHProcessDialogPrivate));
}

/*
 * Instance initialization code.
 */
static void
ph_process_dialog_init(PHProcessDialog *dialog)
{
    PHProcessDialogPrivate *priv = PH_PROCESS_DIALOG_GET_PRIVATE(dialog);

    dialog->priv = priv;

    gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    ph_process_dialog_create_gui(dialog);
}

/* GUI construction {{{1 */

/*
 * Build the graphical user interface.
 */
static void
ph_process_dialog_create_gui(PHProcessDialog *dialog)
{
    GtkWidget *box;
    GtkWidget *button;

    gtk_container_set_border_width(GTK_CONTAINER(dialog), 10);

    /* empty content box */
    dialog->priv->content_vbox = gtk_vbox_new(FALSE, 0);

    /* button box with "Close" and "OK" */
    dialog->priv->button_box = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(dialog->priv->button_box),
            GTK_BUTTONBOX_END);

    button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    g_signal_connect(button, "clicked",
            G_CALLBACK(ph_process_dialog_close_clicked), dialog);
    gtk_container_add(GTK_CONTAINER(dialog->priv->button_box), button);

    button = gtk_button_new_from_stock(GTK_STOCK_OK);
    g_signal_connect(button, "clicked",
            G_CALLBACK(ph_process_dialog_ok_clicked), dialog);
    gtk_container_add(GTK_CONTAINER(dialog->priv->button_box), button);

    /* status box with label, progress bar and cancel button */
    dialog->priv->status_vbox = gtk_vbox_new(FALSE, 3);

    dialog->priv->status_label = gtk_label_new(NULL);
    gtk_label_set_text(GTK_LABEL(dialog->priv->status_label), _("Ready."));
    gtk_misc_set_alignment(GTK_MISC(dialog->priv->status_label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(dialog->priv->status_vbox),
            dialog->priv->status_label, FALSE, FALSE, 0);

    box = gtk_hbox_new(FALSE, 5);

    dialog->priv->progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(box), dialog->priv->progress_bar, TRUE, TRUE, 0);

    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect(button, "clicked",
            G_CALLBACK(ph_process_dialog_cancel_clicked), dialog);
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(dialog->priv->status_vbox), box,
            FALSE, FALSE, 0);

    gtk_widget_set_sensitive(dialog->priv->status_vbox, FALSE);

    /* put it all together */
    box = gtk_vbox_new(FALSE, 10);
    gtk_box_pack_start(GTK_BOX(box), dialog->priv->content_vbox,
            TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), dialog->priv->button_box,
            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), gtk_hseparator_new(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), dialog->priv->status_vbox,
            FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(dialog), box);
}

/* Close request {{{1 */

static gboolean
ph_process_dialog_delete(GtkWidget *widget,
                         GdkEventAny *event)
{
    PHProcessDialog *dialog = PH_PROCESS_DIALOG(widget);

    if (dialog->priv->process == NULL ||
            ph_process_get_state(dialog->priv->process) !=
                PH_PROCESS_STATE_STOPPED)
        ph_process_dialog_stop(dialog);

    return TRUE;
}

/* GUI signal handlers {{{1 */

/*
 * Start the process when the "OK" button is clicked.
 */
static void
ph_process_dialog_ok_clicked(GtkButton *button,
                             gpointer data)
{
    PHProcessDialog *dialog = PH_PROCESS_DIALOG(data);
    PHProcessDialogClass *cls = PH_PROCESS_DIALOG_GET_CLASS(dialog);

    if (dialog->priv->process != NULL)
        /* already running */
        return;

    dialog->priv->process = cls->create_process(dialog);

    if (dialog->priv->process != NULL) {
        g_signal_connect(dialog->priv->process, "progress-notify",
                G_CALLBACK(ph_process_dialog_progress_notify), dialog);
        g_signal_connect(dialog->priv->process, "error-notify",
                G_CALLBACK(ph_process_dialog_error_notify), dialog);
        g_signal_connect(dialog->priv->process, "stop-notify",
                G_CALLBACK(ph_process_dialog_stop_notify), dialog);

        gtk_widget_set_sensitive(dialog->priv->content_vbox, FALSE);
        gtk_widget_set_sensitive(dialog->priv->button_box, FALSE);
        gtk_widget_set_sensitive(dialog->priv->status_vbox, TRUE);

        ph_process_start(dialog->priv->process);
    }
}

/*
 * Close the dialog.
 */
static void
ph_process_dialog_close_clicked(GtkButton *button,
                                gpointer data)
{
    PHProcessDialog *dialog = PH_PROCESS_DIALOG(data);

    ph_process_dialog_stop(dialog);
}

/*
 * Try to stop the process as soon as possible when "Cancel" is clicked.
 */
static void
ph_process_dialog_cancel_clicked(GtkButton *button,
                                 gpointer data)
{
    PHProcessDialog *dialog = PH_PROCESS_DIALOG(data);

    if (dialog->priv->process != NULL)
        ph_process_stop(dialog->priv->process);
}

/* Process signal handlers {{{1 */

/*
 * Update the progress bar to show the current fraction of the task that has
 * been completed.
 */
static void
ph_process_dialog_progress_notify(PHProcess *process,
                                  gdouble fraction,
                                  gpointer data)
{
    PHProcessDialog *dialog = PH_PROCESS_DIALOG(data);

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dialog->priv->progress_bar),
            fraction);
}

/*
 * Relay an error message from the process to the user.
 */
static void
ph_process_dialog_error_notify(PHProcess *process,
                               GError *error,
                               gpointer data)
{
    PHProcessDialog *dialog = PH_PROCESS_DIALOG(data);
    GtkWidget *error_dialog;

    error_dialog = gtk_message_dialog_new(GTK_WINDOW(dialog),
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "%s", error->message);
    gtk_dialog_run(GTK_DIALOG(error_dialog));
    gtk_widget_destroy(error_dialog);
}

/*
 * Make the content box sensitive again after the process has finished
 * (successfully or with an error).
 */
static void
ph_process_dialog_stop_notify(PHProcess *process,
                              gpointer data)
{
    PHProcessDialog *dialog = PH_PROCESS_DIALOG(data);

    g_object_unref(dialog->priv->process);
    dialog->priv->process = NULL;

    gtk_label_set_text(GTK_LABEL(dialog->priv->status_label), _("Ready."));
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dialog->priv->progress_bar),
            0.0);

    gtk_widget_set_sensitive(dialog->priv->content_vbox, TRUE);
    gtk_widget_set_sensitive(dialog->priv->button_box, TRUE);
    gtk_widget_set_sensitive(dialog->priv->status_vbox, FALSE);
}

/* Main loop {{{1 */

/*
 * Display the dialog and block until it is closed.
 */
void
ph_process_dialog_run(PHProcessDialog *dialog)
{
    g_return_if_fail(dialog != NULL && PH_IS_PROCESS_DIALOG(dialog));

    gtk_widget_show_all(GTK_WIDGET(dialog));

    dialog->priv->loop = g_main_loop_new(NULL, FALSE);

    GDK_THREADS_LEAVE();
    g_main_loop_run(dialog->priv->loop);
    GDK_THREADS_ENTER();

    g_main_loop_unref(dialog->priv->loop);
    dialog->priv->loop = NULL;

    gtk_widget_hide(GTK_WIDGET(dialog));
}

/*
 * Stop the main loop, causing ph_process_dialog_run() to return.
 */
static void
ph_process_dialog_stop(PHProcessDialog *dialog)
{
    if (dialog->priv->loop != NULL &&
            g_main_loop_is_running(dialog->priv->loop))
        g_main_loop_quit(dialog->priv->loop);
}

/* Accessors {{{1 */

/*
 * Retrieve the content box, which holds the widgets that allow the
 * configuration of the process to be run.
 */
GtkWidget *
ph_process_dialog_get_content_vbox(PHProcessDialog *dialog)
{
    g_return_val_if_fail(dialog != NULL && PH_IS_PROCESS_DIALOG(dialog), NULL);

    return dialog->priv->content_vbox;
}

/*
 * Retrieve the box holding the "Close" and "OK" buttons.
 */
GtkWidget *
ph_process_dialog_get_button_box(PHProcessDialog *dialog)
{
    g_return_val_if_fail(dialog != NULL && PH_IS_PROCESS_DIALOG(dialog), NULL);

    return dialog->priv->button_box;
}

/*
 * Retrieve the box with the progress bar and status label.
 */
GtkWidget *
ph_process_dialog_get_status_vbox(PHProcessDialog *dialog)
{
    g_return_val_if_fail(dialog != NULL && PH_IS_PROCESS_DIALOG(dialog), NULL);

    return dialog->priv->status_vbox;
}

/* Status text {{{1 */

/*
 * Display some informative text about the progress.  The format string and the
 * variable arguments are processed like g_strdup_printf().
 */
void
ph_process_dialog_set_status_text(PHProcessDialog *dialog,
                                  const gchar *format,
                                  ...)
{
    va_list args;
    gchar *text;

    g_return_if_fail(dialog != NULL && PH_IS_PROCESS_DIALOG(dialog));

    va_start(args, format);
    text = g_strdup_vprintf(format, args);
    va_end(args);

    gtk_label_set_text(GTK_LABEL(dialog->priv->status_label), text);
    g_free(text);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
