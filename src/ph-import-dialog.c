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

#include "ph-import-dialog.h"
#include "ph-import-process.h"
#include <glib/gi18n.h>

/* Properties {{{1 */

enum {
    PH_IMPORT_DIALOG_PROP_0,
    PH_IMPORT_DIALOG_PROP_DATABASE
};

/* Private data {{{1 */

#define PH_IMPORT_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
            PH_TYPE_IMPORT_DIALOG, PHImportDialogPrivate))

struct _PHImportDialogPrivate {
    PHDatabase *database;               /* database connection */

    GtkWidget *file_radio;              /* import a single file? */
    GtkWidget *file_chooser;
    GtkWidget *directory_radio;         /* or an entire directory? */
    GtkWidget *directory_chooser;
};

/* Forward declarations {{{1 */

static void ph_import_dialog_class_init(PHImportDialogClass *cls);
static void ph_import_dialog_init(PHImportDialog *dialog);
static void ph_import_dialog_dispose(GObject *object);
static void ph_import_dialog_set_property(
    GObject *object, guint id, const GValue *value, GParamSpec *spec);
static void ph_import_dialog_get_property(
    GObject *object, guint id, GValue *value, GParamSpec *spec);

static void ph_import_dialog_create_gui(PHImportDialog *dialog);

static void ph_import_dialog_input_type_changed(GtkToggleButton *button,
                                                gpointer data);

static PHProcess *ph_import_dialog_create_process(
    PHProcessDialog *parent_dialog);

static void ph_import_dialog_filename_notify(PHImportProcess *process,
                                             gchar *filename, gpointer data);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHImportDialog, ph_import_dialog, PH_TYPE_PROCESS_DIALOG)

/*
 * Class initialization code.
 */
static void
ph_import_dialog_class_init(PHImportDialogClass *cls)
{
    GObjectClass *g_obj_cls = G_OBJECT_CLASS(cls);
    PHProcessDialogClass *process_dialog_cls = PH_PROCESS_DIALOG_CLASS(cls);

    g_obj_cls->dispose = ph_import_dialog_dispose;
    g_obj_cls->set_property = ph_import_dialog_set_property;
    g_obj_cls->get_property = ph_import_dialog_get_property;

    process_dialog_cls->create_process = ph_import_dialog_create_process;

    g_object_class_install_property(g_obj_cls,
            PH_IMPORT_DIALOG_PROP_DATABASE,
            g_param_spec_object("database", "target database",
                "database to store imported geocache information",
                PH_TYPE_DATABASE,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_type_class_add_private(cls, sizeof(PHImportDialogPrivate));
}

/*
 * Instance initialization code.
 */
static void
ph_import_dialog_init(PHImportDialog *dialog)
{
    PHImportDialogPrivate *priv = PH_IMPORT_DIALOG_GET_PRIVATE(dialog);

    dialog->priv = priv;

    ph_import_dialog_create_gui(dialog);
}

/*
 * Drop references to other objects.
 */
static void
ph_import_dialog_dispose(GObject *object)
{
    PHImportDialog *dialog = PH_IMPORT_DIALOG(object);

    if (dialog->priv->database != NULL) {
        g_object_unref(dialog->priv->database);
        dialog->priv->database = NULL;
    }

    if (G_OBJECT_CLASS(ph_import_dialog_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(ph_import_dialog_parent_class)->dispose(object);
}

/*
 * Property mutator.
 */
static void
ph_import_dialog_set_property(GObject *object,
                              guint id,
                              const GValue *value,
                              GParamSpec *spec)
{
    PHImportDialog *dialog = PH_IMPORT_DIALOG(object);

    switch (id) {
    case PH_IMPORT_DIALOG_PROP_DATABASE:
        if (dialog->priv->database != NULL)
            g_object_unref(dialog->priv->database);
        dialog->priv->database = PH_DATABASE(g_value_dup_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
    }
}

/*
 * Property accessor.
 */
static void
ph_import_dialog_get_property(GObject *object,
                              guint id,
                              GValue *value,
                              GParamSpec *spec)
{
    PHImportDialog *dialog = PH_IMPORT_DIALOG(object);

    switch (id) {
    case PH_IMPORT_DIALOG_PROP_DATABASE:
        g_value_set_object(value, dialog->priv->database);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
    }
}

/* GUI construction {{{1 */

/*
 * Build the graphical user interface.
 */
static void
ph_import_dialog_create_gui(PHImportDialog *dialog)
{
    GtkWidget *content_vbox = ph_process_dialog_get_content_vbox(
            PH_PROCESS_DIALOG(dialog));
    GtkWidget *table = gtk_table_new(2, 2, FALSE);
    GtkWidget *label;

    gtk_window_set_title(GTK_WINDOW(dialog), _("Import file"));

    /* file chooser */
    dialog->priv->file_radio = gtk_radio_button_new(NULL);
    gtk_widget_set_can_focus(dialog->priv->file_radio, FALSE);
    label = gtk_label_new_with_mnemonic(_("_File:"));
    gtk_container_add(GTK_CONTAINER(dialog->priv->file_radio), label);
    g_signal_connect(dialog->priv->file_radio, "toggled",
            G_CALLBACK(ph_import_dialog_input_type_changed), dialog);
    gtk_table_attach(GTK_TABLE(table), dialog->priv->file_radio,
            0, 1, 0, 1, GTK_FILL, 0, 0, 0);

    dialog->priv->file_chooser = gtk_file_chooser_button_new(
            _("Import file"), GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_table_attach(GTK_TABLE(table), dialog->priv->file_chooser,
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    /* directory chooser */
    dialog->priv->directory_radio = gtk_radio_button_new_from_widget(
            GTK_RADIO_BUTTON(dialog->priv->file_radio));
    gtk_widget_set_can_focus(dialog->priv->directory_radio, FALSE);
    label = gtk_label_new_with_mnemonic(_("_Directory:"));
    gtk_container_add(GTK_CONTAINER(dialog->priv->directory_radio), label);
    g_signal_connect(dialog->priv->directory_radio, "toggled",
            G_CALLBACK(ph_import_dialog_input_type_changed), dialog);
    gtk_table_attach(GTK_TABLE(table), dialog->priv->directory_radio,
            0, 1, 1, 2, GTK_FILL, 0, 0, 0);

    dialog->priv->directory_chooser = gtk_file_chooser_button_new(
            _("Import directory"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_widget_set_sensitive(dialog->priv->directory_chooser, FALSE);
    gtk_table_attach(GTK_TABLE(table), dialog->priv->directory_chooser,
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    gtk_table_set_col_spacings(GTK_TABLE(table), 5);

    gtk_box_pack_start(GTK_BOX(content_vbox), table, TRUE, TRUE, 0);
}

/* File/directory radio buttons {{{1 */

/*
 * The choice between files or directories has changed.
 */
static void
ph_import_dialog_input_type_changed(GtkToggleButton *button,
                                    gpointer data)
{
    PHImportDialog *dialog = PH_IMPORT_DIALOG(data);
    gboolean active = gtk_toggle_button_get_active(button);
    GtkWidget *target;

    if (GTK_WIDGET(button) == dialog->priv->file_radio)
        target = dialog->priv->file_chooser;
    else
        target = dialog->priv->directory_chooser;

    gtk_widget_set_sensitive(target, active);
}

/* Process creation {{{1 */

/*
 * Create the import process to run.
 */
static PHProcess *
ph_import_dialog_create_process(PHProcessDialog *parent_dialog)
{
    PHImportDialog *dialog = PH_IMPORT_DIALOG(parent_dialog);
    PHProcess *process = NULL;
    const gchar *path;
    GtkWidget *target;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
                    dialog->priv->file_radio)))
        target = dialog->priv->file_chooser;
    else
        target = dialog->priv->directory_chooser;

    path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(target));

    if (path != NULL) {
        /* do not start without a path */
        process = ph_import_process_new(dialog->priv->database, path);
        g_signal_connect(process, "filename-notify",
                G_CALLBACK(ph_import_dialog_filename_notify), dialog);
    }

    return process;
}

/* Process signal handlers {{{1 */

static void
ph_import_dialog_filename_notify(PHImportProcess *process,
                                 gchar *filename,
                                 gpointer data)
{
    PHProcessDialog *dialog = PH_PROCESS_DIALOG(data);

    ph_process_dialog_set_status_text(dialog,
            _("Importing “%s”…"), filename);
}

/* Public interface {{{1 */

GtkWidget *
ph_import_dialog_new(GtkWindow *parent,
                     PHDatabase *database)
{
    GtkWindow *window = GTK_WINDOW(g_object_new(PH_TYPE_IMPORT_DIALOG,
                "database", database, NULL));

    gtk_window_set_transient_for(window, parent);
    gtk_window_set_position(window, GTK_WIN_POS_CENTER_ON_PARENT);

    return GTK_WIDGET(window);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
