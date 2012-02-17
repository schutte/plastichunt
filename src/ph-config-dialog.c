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
#include "ph-config-dialog.h"
#include "ph-map-provider.h"
#include "ph-map-tile-cache.h"
#include <glib/gi18n.h>
#include <math.h>

/* Private data {{{1 */

#define PH_CONFIG_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
            PH_TYPE_CONFIG_DIALOG, PHConfigDialogPrivate))

struct _PHConfigDialogPrivate {
    GtkWidget *notebook;            /* content area */

    GtkWidget *map_providers;       /* map provider combo box */
    GList *map_provider_specs;      /* known map provider settings */

    GtkWidget *tile_cache_settings; /* map tile cache setting widgets */
};

/* Callback data structure {{{1 */

/*
 * Data for configuration string callbacks.
 */
typedef struct _PHConfigDialogSpec PHConfigDialogSpec;
struct _PHConfigDialogSpec {
    /* filled in by ph_config_dialog_spec_new */
    PHConfigDialog *dialog;
    GType type;
    guint refs;

    /* filled in during high-level GUI construction */
    union {
        void (*without_data)(GValue *value);
        void (*with_data)(PHConfigDialog *dialog, GValue *value,
                          const PHConfigDialogSpec *spec);
    } getter;
    union {
        void (*without_data)(const GValue *value);
        void (*with_data)(PHConfigDialog *dialog, GValue *value,
                          const PHConfigDialogSpec *spec);
    } setter;
    gboolean has_data;
    gpointer data;
    GFreeFunc free_func;

    /* filled in during low-level GUI construction */
    GtkWidget *input;
};

/* Forward declarations {{{1 */

static void ph_config_dialog_class_init(PHConfigDialogClass *cls);
static void ph_config_dialog_init(PHConfigDialog *dialog);
static void ph_config_dialog_dispose(GObject *object);

static void ph_config_dialog_response(GtkDialog *dialog, gint response_id);

static void ph_config_dialog_show_error(PHConfigDialog *dialog,
                                        const gchar *format, ...);

static GtkWidget *ph_config_dialog_create_programs_tab(PHConfigDialog *dialog);
static GtkWidget *ph_config_dialog_create_providers_tab(PHConfigDialog *dialog);
static GtkWidget *ph_config_dialog_create_tile_cache_tab(
    PHConfigDialog *dialog);

static void ph_config_dialog_create_input(
    PHConfigDialog *dialog, GtkTable *table, gint row,
    const gchar *label_text, gboolean clear, PHConfigDialogSpec *spec);

static void ph_config_dialog_set_string(GtkEntry *entry, gpointer data);
static void ph_config_dialog_clear_string(GtkButton *button, gpointer data);

static void ph_config_dialog_set_numeric(GtkSpinButton *spin_button,
                                         gpointer data);
static void ph_config_dialog_clear_numeric(GtkButton *button, gpointer data);

static void ph_config_dialog_map_provider_changed(GtkComboBox *combo_box,
                                                  gpointer data);
static void ph_config_dialog_map_provider_getter(
    PHConfigDialog *dialog, GValue *value, const PHConfigDialogSpec *spec);
static void ph_config_dialog_map_provider_setter(
    PHConfigDialog *dialog, GValue *value, const PHConfigDialogSpec *spec);
static void ph_config_dialog_add_map_provider(GtkButton *button, gpointer data);
static void ph_config_dialog_remove_map_provider(GtkButton *button,
                                                 gpointer data);

static void ph_config_dialog_map_cache_enabled_toggled(PHConfigDialog *dialog,
                                                       GtkToggleButton *toggle);

static PHConfigDialogSpec *ph_config_dialog_spec_new(PHConfigDialog *dialog,
                                                     GType type);
static PHConfigDialogSpec *ph_config_dialog_spec_ref(PHConfigDialogSpec *spec);
static void ph_config_dialog_spec_unref(PHConfigDialogSpec *spec);
static void ph_config_dialog_spec_call_getter(const PHConfigDialogSpec *spec,
                                              GValue *value);
static void ph_config_dialog_spec_call_setter(const PHConfigDialogSpec *spec,
                                              GValue *value);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHConfigDialog, ph_config_dialog, GTK_TYPE_DIALOG)

/*
 * Class initialization code.
 */
static void
ph_config_dialog_class_init(PHConfigDialogClass *cls)
{
    GObjectClass *g_obj_cls = G_OBJECT_CLASS(cls);
    GtkDialogClass *gtk_dialog_cls = GTK_DIALOG_CLASS(cls);

    g_obj_cls->dispose = ph_config_dialog_dispose;

    gtk_dialog_cls->response = ph_config_dialog_response;

    g_type_class_add_private(cls, sizeof(PHConfigDialogPrivate));
}

/*
 * Instance initialization code.
 */
static void
ph_config_dialog_init(PHConfigDialog *dialog)
{
    PHConfigDialogPrivate *priv = PH_CONFIG_DIALOG_GET_PRIVATE(dialog);
    GtkWidget *content_vbox;

    dialog->priv = priv;

    gtk_window_set_title(GTK_WINDOW(dialog), _("Preferences"));
    gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CLOSE,
            GTK_RESPONSE_CLOSE);

    dialog->priv->notebook = gtk_notebook_new();

    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->priv->notebook),
                             ph_config_dialog_create_programs_tab(dialog),
                             gtk_label_new(_("Programs")));
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->priv->notebook),
                             ph_config_dialog_create_providers_tab(dialog),
                             gtk_label_new(_("Map providers")));
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->priv->notebook),
                             ph_config_dialog_create_tile_cache_tab(dialog),
                             gtk_label_new(_("Map tile cache")));

    content_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(content_vbox), dialog->priv->notebook,
            TRUE, TRUE, 0);
    gtk_widget_show_all(dialog->priv->notebook);
}

/*
 * Drop references to other objects.
 */
static void
ph_config_dialog_dispose(GObject *object)
{
    PHConfigDialog *dialog = PH_CONFIG_DIALOG(object);

    if (dialog->priv->map_provider_specs != NULL) {
        g_list_free_full(dialog->priv->map_provider_specs,
                (GDestroyNotify) ph_config_dialog_spec_unref);
        dialog->priv->map_provider_specs = NULL;
    }

    if (G_OBJECT_CLASS(ph_config_dialog_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(ph_config_dialog_parent_class)->dispose(object);
}

/* Save configuration {{{1 */

/*
 * Trigger the map tile cache and store the configuration back to where it was
 * loaded from when the window is closed.
 */
static void
ph_config_dialog_response(GtkDialog *dialog,
                          gint response_id)
{
    GError *error = NULL;

    ph_map_tile_cache_restart();

    if (!ph_config_save(&error)) {
        ph_config_dialog_show_error(PH_CONFIG_DIALOG(dialog),
                "Failed to write the configuration file: %s.",
                error->message);
        g_error_free(error);
    }
}

/* Error dialog {{{1 */

/*
 * Show a modal error dialog with the given printf()-style message.
 */
static void
ph_config_dialog_show_error(PHConfigDialog *dialog,
                            const gchar *format,
                            ...)
{
    va_list args;
    gchar *message;
    GtkWidget *message_dialog;

    va_start(args, format);
    message = g_strdup_vprintf(format, args);
    va_end(args);

    message_dialog = gtk_message_dialog_new(GTK_WINDOW(dialog),
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
            "%s", message);
    (void) gtk_dialog_run(GTK_DIALOG(message_dialog));
    gtk_widget_destroy(message_dialog);

    g_free(message);
}

/* High-level GUI construction {{{1 */

/*
 * Populate the "Programs" tab.
 */
static GtkWidget *
ph_config_dialog_create_programs_tab(PHConfigDialog *dialog)
{
    GtkWidget *table = gtk_table_new(1, 3, FALSE);
    PHConfigDialogSpec *spec;

    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_table_set_col_spacing(GTK_TABLE(table), 0, 10);
    gtk_table_set_row_spacings(GTK_TABLE(table), 3);

    /* web browser entry */
    spec = ph_config_dialog_spec_new(dialog, G_TYPE_STRING);
    spec->getter.without_data = ph_config_get_browser;
    spec->setter.without_data = ph_config_set_browser;
    ph_config_dialog_create_input(dialog, GTK_TABLE(table), 0,
            _("_Web browser:"), TRUE, spec);
    ph_config_dialog_spec_unref(spec);

    return table;
}

/*
 * Populate the "Map providers" tab.
 */
static GtkWidget *
ph_config_dialog_create_providers_tab(PHConfigDialog *dialog)
{
    GtkWidget *table = gtk_table_new(6, 3, FALSE);
    GtkWidget *hbox;
    GtkCellRenderer *renderer;
    GtkWidget *button;
    GtkAdjustment *adjustment;
    PHConfigDialogSpec *spec;

    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_table_set_col_spacing(GTK_TABLE(table), 0, 10);
    gtk_table_set_row_spacings(GTK_TABLE(table), 3);

    /* combo box and add/remove buttons */
    dialog->priv->map_providers = gtk_combo_box_new_with_model(
            GTK_TREE_MODEL(ph_config_get_map_providers()));

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dialog->priv->map_providers),
            renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(dialog->priv->map_providers),
            renderer, "text", PH_MAP_PROVIDER_COLUMN_NAME);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), dialog->priv->map_providers, TRUE, TRUE, 0);

    button = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(button),
            gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(button, "clicked",
            G_CALLBACK(ph_config_dialog_add_map_provider), dialog);

    button = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(button),
            gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(button, "clicked",
            G_CALLBACK(ph_config_dialog_remove_map_provider), dialog);

    gtk_table_attach(GTK_TABLE(table), hbox, 0, 3, 0, 1,
            GTK_EXPAND | GTK_FILL, 0, 0, 0);

    /* URL pattern entry */
    spec = ph_config_dialog_spec_new(dialog, G_TYPE_STRING);
    spec->getter.with_data = ph_config_dialog_map_provider_getter;
    spec->setter.with_data = ph_config_dialog_map_provider_setter;
    spec->has_data = TRUE;
    spec->data = GINT_TO_POINTER(PH_MAP_PROVIDER_COLUMN_URL);
    ph_config_dialog_create_input(dialog, GTK_TABLE(table), 1,
            _("_URL pattern:"), TRUE, spec);
    dialog->priv->map_provider_specs =
        g_list_prepend(dialog->priv->map_provider_specs, spec);
    ph_config_dialog_spec_unref(spec);

    /* tile size */
    spec = ph_config_dialog_spec_new(dialog, G_TYPE_DOUBLE);
    spec->getter.with_data = ph_config_dialog_map_provider_getter;
    spec->setter.with_data = ph_config_dialog_map_provider_setter;
    spec->has_data = TRUE;
    spec->data = GINT_TO_POINTER(PH_MAP_PROVIDER_COLUMN_TILE_SIZE);
    ph_config_dialog_create_input(dialog, GTK_TABLE(table), 2,
            _("_Tile size:"), TRUE, spec);
    adjustment = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spec->input));
    g_object_set(adjustment,
            "lower", 0.0, "upper", (gdouble) G_MAXUINT,
            "step-increment", 1.0, "page-increment", 1.0, "page-size", 0.0,
            NULL);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spec->input), 0);
    dialog->priv->map_provider_specs =
        g_list_prepend(dialog->priv->map_provider_specs, spec);
    ph_config_dialog_spec_unref(spec);

    /* minimum zoom level */
    spec = ph_config_dialog_spec_new(dialog, G_TYPE_DOUBLE);
    spec->getter.with_data = ph_config_dialog_map_provider_getter;
    spec->setter.with_data = ph_config_dialog_map_provider_setter;
    spec->has_data = TRUE;
    spec->data = GINT_TO_POINTER(PH_MAP_PROVIDER_COLUMN_ZOOM_MIN);
    ph_config_dialog_create_input(dialog, GTK_TABLE(table), 3,
            _("_Minimum zoom level:"), TRUE, spec);
    adjustment = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spec->input));
    g_object_set(adjustment,
            "lower", 0.0, "upper", (gdouble) G_MAXUINT,
            "step-increment", 1.0, "page-increment", 1.0, "page-size", 0.0,
            NULL);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spec->input), 0);
    dialog->priv->map_provider_specs =
        g_list_prepend(dialog->priv->map_provider_specs, spec);
    ph_config_dialog_spec_unref(spec);

    /* maximum zoom level */
    spec = ph_config_dialog_spec_new(dialog, G_TYPE_DOUBLE);
    spec->getter.with_data = ph_config_dialog_map_provider_getter;
    spec->setter.with_data = ph_config_dialog_map_provider_setter;
    spec->has_data = TRUE;
    spec->data = GINT_TO_POINTER(PH_MAP_PROVIDER_COLUMN_ZOOM_MAX);
    ph_config_dialog_create_input(dialog, GTK_TABLE(table), 4,
            _("Ma_ximum zoom level:"), TRUE, spec);
    adjustment = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spec->input));
    g_object_set(adjustment,
            "lower", 0.0, "upper", (gdouble) G_MAXUINT,
            "step-increment", 1.0, "page-increment", 1.0, "page-size", 0.0,
            NULL);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spec->input), 0);
    dialog->priv->map_provider_specs =
        g_list_prepend(dialog->priv->map_provider_specs, spec);
    ph_config_dialog_spec_unref(spec);

    /* detail zoom level */
    spec = ph_config_dialog_spec_new(dialog, G_TYPE_DOUBLE);
    spec->getter.with_data = ph_config_dialog_map_provider_getter;
    spec->setter.with_data = ph_config_dialog_map_provider_setter;
    spec->has_data = TRUE;
    spec->data = GINT_TO_POINTER(PH_MAP_PROVIDER_COLUMN_ZOOM_DETAIL);
    ph_config_dialog_create_input(dialog, GTK_TABLE(table), 5,
            _("_Detail zoom level:"), TRUE, spec);
    adjustment = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spec->input));
    g_object_set(adjustment,
            "lower", 0.0, "upper", (gdouble) G_MAXUINT,
            "step-increment", 1.0, "page-increment", 1.0, "page-size", 0.0,
            NULL);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spec->input), 0);
    dialog->priv->map_provider_specs =
        g_list_prepend(dialog->priv->map_provider_specs, spec);
    ph_config_dialog_spec_unref(spec);

    g_signal_connect(dialog->priv->map_providers, "changed",
            G_CALLBACK(ph_config_dialog_map_provider_changed), dialog);
    gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->priv->map_providers), 0);

    return table;
}

/*
 * Populate the "Map tile cache" tab.
 */
static GtkWidget *
ph_config_dialog_create_tile_cache_tab(PHConfigDialog *dialog)
{
    GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
    GtkWidget *table = gtk_table_new(3, 2, FALSE);
    GtkWidget *enabled;
    GValue value = { 0 };
    PHConfigDialogSpec *spec;

    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_table_set_col_spacing(GTK_TABLE(table), 0, 10);
    gtk_table_set_row_spacings(GTK_TABLE(table), 3);

    /* main switch */
    enabled = gtk_check_button_new_with_mnemonic(
        _("_Cache downloaded map tiles"));
    g_signal_connect_swapped(
        enabled, "toggled",
        G_CALLBACK(ph_config_dialog_map_cache_enabled_toggled),
        dialog);
    gtk_box_pack_start(GTK_BOX(vbox), enabled, FALSE, FALSE, 0);

    /* cache location */
    spec = ph_config_dialog_spec_new(dialog, G_TYPE_STRING);
    spec->getter.without_data = ph_config_get_tile_cache_location;
    spec->setter.without_data = ph_config_set_tile_cache_location;
    ph_config_dialog_create_input(dialog, GTK_TABLE(table), 0,
            _("_Directory:"), TRUE, spec);
    ph_config_dialog_spec_unref(spec);

    /* maximum tile age */
    spec = ph_config_dialog_spec_new(dialog, G_TYPE_UINT);
    spec->getter.without_data = ph_config_get_max_tile_age,
    spec->setter.without_data = ph_config_set_max_tile_age;
    ph_config_dialog_create_input(dialog, GTK_TABLE(table), 1,
            _("Maximum tile _age (in days):"), TRUE, spec);
    ph_config_dialog_spec_unref(spec);

    /* maximum cache size */
    spec = ph_config_dialog_spec_new(dialog, G_TYPE_UINT);
    spec->getter.without_data = ph_config_get_max_tile_cache_size,
    spec->setter.without_data = ph_config_set_max_tile_cache_size;
    ph_config_dialog_create_input(dialog, GTK_TABLE(table), 2,
            _("Total cache _size (in MB):"), TRUE, spec);
    ph_config_dialog_spec_unref(spec);

    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

    /* load current setting of the main switch */
    dialog->priv->tile_cache_settings = table;
    gtk_widget_set_sensitive(table, FALSE);

    g_value_init(&value, G_TYPE_BOOLEAN);
    ph_config_get_tile_cache_enabled(&value);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enabled),
                                 g_value_get_boolean(&value));
    g_value_unset(&value);

    return vbox;
}

/* Low-level GUI construction {{{1 */

/*
 * Create a label, entry and an optional clear button, attach them to the given
 * table row and connect the entry "changed" and button "clicked" events
 * according to the PHConfigDialogSpec.
 */
static void
ph_config_dialog_create_input(PHConfigDialog *dialog,
                              GtkTable *table,
                              gint row,
                              const gchar *label_text,
                              gboolean clear,
                              PHConfigDialogSpec *spec)
{
    GtkWidget *label;
    GValue value = { 0 };
    GCallback clear_func;

    label = gtk_label_new_with_mnemonic(label_text);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
            GTK_FILL, GTK_FILL, 0, 0);

    g_value_init(&value, spec->type);
    ph_config_dialog_spec_call_getter(spec, &value);

    if (spec->type == G_TYPE_STRING) {
        spec->input = gtk_entry_new();
        if (g_value_get_string(&value) != NULL)
            gtk_entry_set_text(GTK_ENTRY(spec->input),
                    g_value_get_string(&value));

        g_signal_connect_data(spec->input, "changed",
                G_CALLBACK(ph_config_dialog_set_string),
                ph_config_dialog_spec_ref(spec),
                (GClosureNotify) ph_config_dialog_spec_unref,
                0);
        clear_func = G_CALLBACK(ph_config_dialog_clear_string);
    }
    else {      /* numeric values */
        if (spec->type == G_TYPE_DOUBLE) {
            spec->input = gtk_spin_button_new_with_range(
                G_MINDOUBLE, G_MAXDOUBLE, 0.1);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spec->input),
                                      g_value_get_double(&value));
        }
        else if (spec->type == G_TYPE_UINT) {
            spec->input = gtk_spin_button_new_with_range(0, G_MAXUINT, 1.0);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spec->input),
                                      g_value_get_uint(&value));
        }

        g_signal_connect_data(spec->input, "value-changed",
                G_CALLBACK(ph_config_dialog_set_numeric),
                ph_config_dialog_spec_ref(spec),
                (GClosureNotify) ph_config_dialog_spec_unref,
                0);
        clear_func = G_CALLBACK(ph_config_dialog_clear_numeric);
    }

    g_value_unset(&value);

    if (spec->input == NULL)
        /* unknown type */
        g_return_if_reached();

    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spec->input);
    gtk_table_attach(GTK_TABLE(table), spec->input, 1, 2, row, row + 1,
            GTK_EXPAND | GTK_FILL, 0, 0, 0);

    if (clear) {
        GtkWidget *button = gtk_button_new();

        gtk_button_set_image(GTK_BUTTON(button),
                gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
        gtk_table_attach(GTK_TABLE(table), button, 2, 3,
                row, row + 1, 0, 0, 0, 0);

        g_signal_connect_data(button, "clicked", clear_func,
                ph_config_dialog_spec_ref(spec),
                (GClosureNotify) ph_config_dialog_spec_unref,
                0);
    }
}

/* Textual settings {{{1 */

/*
 * Run the "setter" callback whenever the entry changes.
 */
static void
ph_config_dialog_set_string(GtkEntry *entry,
                            gpointer data)
{
    PHConfigDialogSpec *spec = (PHConfigDialogSpec *) data;
    GValue value = { 0 };
    const gchar *text;

    text = gtk_entry_get_text(GTK_ENTRY(spec->input));

    g_value_init(&value, G_TYPE_STRING);
    ph_config_dialog_spec_call_getter(spec, &value);

    if (strcmp(text, g_value_get_string(&value)) != 0) {
        g_value_set_string(&value, text);
        ph_config_dialog_spec_call_setter(spec, &value);
    }

    g_value_unset(&value);
}

/*
 * When the clear button is clicked, run the "setter" callback with a NULL value
 * and use the "getter" callback to retrieve and display the default value.
 */
static void
ph_config_dialog_clear_string(GtkButton *button,
                              gpointer data)
{
    PHConfigDialogSpec *spec = (PHConfigDialogSpec *) data;
    GValue value = { 0 };

    ph_config_dialog_spec_call_setter(spec, NULL);

    g_value_init(&value, G_TYPE_STRING);
    ph_config_dialog_spec_call_getter(spec, &value);

    gtk_entry_set_text(GTK_ENTRY(spec->input), g_value_get_string(&value));

    g_value_unset(&value);
}

/* Numeric settings {{{1 */

/*
 * Run the "setter" callback whenever the spin button changes.
 */
static void
ph_config_dialog_set_numeric(GtkSpinButton *spin_button,
                             gpointer data)
{
    PHConfigDialogSpec *spec = (PHConfigDialogSpec *) data;
    GValue value = { 0 }, double_value = { 0 };
    gdouble new_value;

    new_value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spec->input));

    g_value_init(&value, spec->type);
    ph_config_dialog_spec_call_getter(spec, &value);

    g_value_init(&double_value, G_TYPE_DOUBLE);
    g_value_transform(&value, &double_value);

    if (fabs(new_value - g_value_get_double(&double_value)) > 0.001) {
        g_value_set_double(&double_value, new_value);
        g_value_transform(&double_value, &value);
        ph_config_dialog_spec_call_setter(spec, &value);
    }

    g_value_unset(&value);
    g_value_unset(&double_value);
}

/*
 * When the clear button is clicked, run the "setter" callback with a NULL value
 * and use the "getter" callback to retrieve and display the default value.
 */
static void
ph_config_dialog_clear_numeric(GtkButton *button,
                               gpointer data)
{
    PHConfigDialogSpec *spec = (PHConfigDialogSpec *) data;
    GValue value = { 0 }, double_value = { 0 };

    ph_config_dialog_spec_call_setter(spec, NULL);

    g_value_init(&value, spec->type);
    ph_config_dialog_spec_call_getter(spec, &value);

    g_value_init(&double_value, G_TYPE_DOUBLE);
    g_value_transform(&value, &double_value);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spec->input),
            g_value_get_double(&double_value));

    g_value_unset(&value);
    g_value_unset(&double_value);
}

/* Map providers {{{1 */

/*
 * Handle map provider selections by calling the clear functions on all specs.
 */
static void
ph_config_dialog_map_provider_changed(GtkComboBox *combo_box,
                                      gpointer data)
{
    PHConfigDialog *dialog = PH_CONFIG_DIALOG(data);
    GList *cur;
    GValue value = { 0 };

    if (gtk_combo_box_get_active(combo_box) == -1)
        return;

    for (cur = dialog->priv->map_provider_specs; cur != NULL; cur = cur->next) {
        PHConfigDialogSpec *spec = (PHConfigDialogSpec *) cur->data;
        g_value_init(&value, spec->type);
        ph_config_dialog_spec_call_getter(spec, &value);
        switch (spec->type) {
        case G_TYPE_STRING:
            gtk_entry_set_text(GTK_ENTRY(spec->input),
                    g_value_get_string(&value));
            break;
        case G_TYPE_DOUBLE:
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spec->input),
                    g_value_get_double(&value));
            break;
        }
        g_value_unset(&value);
    }
}

/*
 * Getter function for map provider preferences.
 */
static void
ph_config_dialog_map_provider_getter(PHConfigDialog *dialog,
                                     GValue *value,
                                     const PHConfigDialogSpec *spec)
{
    GtkTreeIter iter;
    GtkComboBox *combo_box = GTK_COMBO_BOX(dialog->priv->map_providers);
    GtkTreeModel *model = gtk_combo_box_get_model(combo_box);
    GValue original = { 0 };

    if (gtk_combo_box_get_active(combo_box) == -1)
        /* nothing selected */
        return;

    gtk_combo_box_get_active_iter(combo_box, &iter);
    gtk_tree_model_get_value(model, &iter,
            GPOINTER_TO_INT(spec->data), &original);
    g_value_transform(&original, value);

    g_value_unset(&original);
}

/*
 * Setter function for map provider preferences.
 */
static void
ph_config_dialog_map_provider_setter(PHConfigDialog *dialog,
                                     GValue *value,
                                     const PHConfigDialogSpec *spec)
{
    GtkTreeIter iter;
    GtkComboBox *combo_box = GTK_COMBO_BOX(dialog->priv->map_providers);
    gint row = gtk_combo_box_get_active(combo_box);
    gint column = GPOINTER_TO_INT(spec->data);

    if (row == -1)
        /* nothing selected */
        return;

    gtk_combo_box_get_active_iter(combo_box, &iter);
    ph_config_set_map_provider(&iter, (guint) row, column, value);
}

/*
 * Create a new map provider.
 */
static void
ph_config_dialog_add_map_provider(GtkButton *button,
                                  gpointer data)
{
    PHConfigDialog *dialog = PH_CONFIG_DIALOG(data);
    GtkWidget *name_dialog;
    GtkWidget *vbox;
    GtkWidget *name;
    GtkWidget *label;
    gint response;

    name_dialog = gtk_dialog_new_with_buttons(_("Add a map provider"),
            GTK_WINDOW(dialog), GTK_DIALOG_MODAL,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT,
            NULL);
    vbox = gtk_dialog_get_content_area(GTK_DIALOG(name_dialog));

    label = gtk_label_new(_("Name:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

    name = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), name, TRUE, TRUE, 0);

    gtk_widget_show_all(vbox);

    response = gtk_dialog_run(GTK_DIALOG(name_dialog));

    if (response == GTK_RESPONSE_ACCEPT) {
        gint row;

        row = ph_config_add_map_provider(gtk_entry_get_text(GTK_ENTRY(name)));
        if (row == -1)
            ph_config_dialog_show_error(dialog,
                    _("There already is a map provider with this name."));
        else
            gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->priv->map_providers),
                    row);
    }

    gtk_widget_destroy(name_dialog);
}

/*
 * Remove an existing map provider.
 */
static void
ph_config_dialog_remove_map_provider(GtkButton *button,
                                     gpointer data)
{
    PHConfigDialog *dialog = PH_CONFIG_DIALOG(data);
    GtkTreeIter iter;
    gint row;

    row = gtk_combo_box_get_active(GTK_COMBO_BOX(dialog->priv->map_providers));
    if (row == -1)
        return;

    gtk_combo_box_get_active_iter(GTK_COMBO_BOX(dialog->priv->map_providers),
            &iter);

    row = ph_config_remove_map_provider(&iter, row);
    if (row == -1)
        ph_config_dialog_show_error(dialog,
                _("You cannot remove a predefined map provider."));
    else
        gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->priv->map_providers),
                row);
}

/* Map tile cache {{{1 */

/*
 * React to the map tile cache being enabled or disabled: Write the
 * configuration setting and adjust the sensitivity of the widgets for the
 * detailed settings.
 */
static void
ph_config_dialog_map_cache_enabled_toggled(PHConfigDialog *dialog,
                                           GtkToggleButton *toggle)
{
    gboolean enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
    GValue value = { 0 };

    g_value_init(&value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&value, enabled);
    ph_config_set_tile_cache_enabled(&value);
    g_value_unset(&value);

    gtk_widget_set_sensitive(dialog->priv->tile_cache_settings, enabled);
}

/* Callback struct utilities {{{1 */

/*
 * Create a new PHConfigDialogSpec with an initial reference count of 1.
 */
static PHConfigDialogSpec *
ph_config_dialog_spec_new(PHConfigDialog *dialog,
                          GType type)
{
    PHConfigDialogSpec *spec = g_new0(PHConfigDialogSpec, 1);

    spec->dialog = dialog;
    spec->type = type;
    spec->refs = 1;

    return spec;
}

/*
 * Increase the reference count of a PHConfigDialogSpec.
 */
static PHConfigDialogSpec *
ph_config_dialog_spec_ref(PHConfigDialogSpec *spec)
{
    ++spec->refs;
    return spec;
}

/*
 * Decrease the reference count of a PHConfigDialogSpec and free it once it
 * becomes 0.
 */
static void
ph_config_dialog_spec_unref(PHConfigDialogSpec *spec)
{
    if (--spec->refs == 0) {
        if (spec->free_func != NULL)
            spec->free_func(spec->data);
        g_free(spec);
    }
}

/*
 * Call the correct getter function of a PHConfigDialogSpec, depending on
 * whether a value is set.
 */
static void
ph_config_dialog_spec_call_getter(const PHConfigDialogSpec *spec,
                                  GValue *value)
{
    if (spec->has_data)
        spec->getter.with_data(spec->dialog, value, spec);
    else
        spec->getter.without_data(value);
}

/*
 * Same thing for the setter callback.
 */
static void
ph_config_dialog_spec_call_setter(const PHConfigDialogSpec *spec,
                                  GValue *value)
{
    if (spec->has_data)
        spec->setter.with_data(spec->dialog, value, spec);
    else
        spec->setter.without_data(value);
}

/* Public interface {{{1 */

/*
 * Create a new configuration dialog.
 */
GtkWidget *
ph_config_dialog_new()
{
    return GTK_WIDGET(g_object_new(PH_TYPE_CONFIG_DIALOG, NULL));
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
