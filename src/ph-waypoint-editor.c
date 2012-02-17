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

#include "ph-waypoint-editor.h"
#include "ph-geo.h"
#include <glib/gi18n.h>

/* Signals {{{1 */

enum {
    PH_WAYPOINT_EDITOR_SIGNAL_CHANGED,
    PH_WAYPOINT_EDITOR_SIGNAL_COUNT
};

static guint ph_waypoint_editor_signals[PH_WAYPOINT_EDITOR_SIGNAL_COUNT];

/* Private data {{{1 */

#define PH_WAYPOINT_EDITOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
            PH_TYPE_WAYPOINT_EDITOR, PHWaypointEditorPrivate))

struct _PHWaypointEditorPrivate {
    GtkWidget *latitude;
    GtkWidget *longitude;
};

/* Forward declarations {{{1 */

static void ph_waypoint_editor_init(PHWaypointEditor *editor);
static void ph_waypoint_editor_class_init(PHWaypointEditorClass *cls);

static void ph_waypoint_editor_create_ui(PHWaypointEditor *editor);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHWaypointEditor, ph_waypoint_editor, GTK_TYPE_TABLE)

/*
 * Class initialization code.
 */
static void
ph_waypoint_editor_class_init(PHWaypointEditorClass *cls)
{
    g_type_class_add_private(cls, sizeof(PHWaypointEditorPrivate));

    ph_waypoint_editor_signals[PH_WAYPOINT_EDITOR_SIGNAL_CHANGED] =
        g_signal_new("changed", G_OBJECT_CLASS_TYPE(cls),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET(PHWaypointEditorClass, changed),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);
}

/*
 * Instance initialization code.
 */
static void
ph_waypoint_editor_init(PHWaypointEditor *editor)
{
    PHWaypointEditorPrivate *priv = PH_WAYPOINT_EDITOR_GET_PRIVATE(editor);

    editor->priv = priv;

    g_object_set(editor,
            "n-columns", 2,
            "n-rows", 2,
            NULL);

    ph_waypoint_editor_create_ui(editor);
}

/* GUI construction {{{1 */

static void
ph_waypoint_editor_create_ui(PHWaypointEditor *editor)
{
    GtkWidget *label;

    label = gtk_label_new_with_mnemonic(_("L_atitude:"));
    gtk_table_attach(GTK_TABLE(editor), label,
            0, 1, 0, 1, 0, 0, 0, 0);
    editor->priv->latitude = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(editor), editor->priv->latitude,
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), editor->priv->latitude);

    label = gtk_label_new_with_mnemonic(_("L_ongitude:"));
    gtk_table_attach(GTK_TABLE(editor), label,
            0, 1, 1, 2, 0, 0, 0, 0);
    editor->priv->longitude = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(editor), editor->priv->longitude,
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), editor->priv->longitude);
}

/* Public interface {{{1 */

/*
 * Create a new PHWaypointEditor instance.
 */
GtkWidget *
ph_waypoint_editor_new()
{
    GtkWidget *result = GTK_WIDGET(g_object_new(PH_TYPE_WAYPOINT_EDITOR, NULL));
    gtk_widget_set_sensitive(result, FALSE);
    return result;
}

/*
 * Begin editing a waypoint specified by latitude and longitude.
 */
void
ph_waypoint_editor_start(PHWaypointEditor *editor,
                         gdouble latitude,
                         gdouble longitude)
{
    gchar *temp;

    temp = ph_geo_latitude_deg_to_string(latitude);
    gtk_entry_set_text(GTK_ENTRY(editor->priv->latitude), temp);
    g_free(temp);

    temp = ph_geo_longitude_deg_to_string(longitude);
    gtk_entry_set_text(GTK_ENTRY(editor->priv->longitude), temp);
    g_free(temp);

    gtk_widget_set_sensitive(GTK_WIDGET(editor), TRUE);
}

/*
 * End editing and retrieve the new coordinates of the waypoint.
 */
void
ph_waypoint_editor_end(PHWaypointEditor *editor,
                       gdouble *latitude,
                       gdouble *longitude)
{
    if (latitude != NULL) {
        const gchar *lat_string = gtk_entry_get_text(
                GTK_ENTRY(editor->priv->latitude));
        *latitude = ph_geo_latitude_string_to_deg(lat_string);
    }
    if (longitude != NULL) {
        const gchar *lon_string = gtk_entry_get_text(
                GTK_ENTRY(editor->priv->longitude));
        *longitude = ph_geo_longitude_string_to_deg(lon_string);
    }

    gtk_entry_set_text(GTK_ENTRY(editor->priv->latitude), "");
    gtk_entry_set_text(GTK_ENTRY(editor->priv->longitude), "");
    gtk_widget_set_sensitive(GTK_WIDGET(editor), FALSE);
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
