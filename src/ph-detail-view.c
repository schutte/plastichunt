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

#include "ph-cell-renderer-sprite.h"
#include "ph-detail-view.h"
#include "ph-geo.h"
#include "ph-sprite-image.h"
#include "ph-util.h"
#include "ph-waypoint-editor.h"
#include <glib/gi18n.h>
#include <webkit/webkit.h>

/* Properties {{{1 */

enum {
    PH_DETAIL_VIEW_PROP_0,
    PH_DETAIL_VIEW_PROP_DATABASE
};

/* Signals {{{1 */

enum {
    PH_DETAIL_VIEW_SIGNAL_CLOSED,
    PH_DETAIL_VIEW_SIGNAL_COUNT
};

static guint ph_detail_view_signals[PH_DETAIL_VIEW_SIGNAL_COUNT];

/* Private data {{{1 */

#define PH_DETAIL_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
            PH_TYPE_DETAIL_VIEW, PHDetailViewPrivate))

struct _PHDetailViewPrivate {
    PHDatabase *database;               /* data source */
    gulong db_signal_handlers[2];       /* database signal handlers */
    gboolean updating;                  /* did we trigger DB signals? */

    /* displaying immutable data */
    GtkWidget *tab_label, *tab_image, *tab_name;
    GtkWidget *name;
    GtkWidget *type_image, *size_image, *difficulty_image, *terrain_image;
    GtkWidget *description;
    GtkWidget *hint;

    GtkListStore *waypoints;
    GtkWidget *logs;
    GtkListStore *trackables;

    /* custom notes */
    PHGeocacheNote *geocache_note;
    GtkWidget *found;
    GtkWidget *note_editor;
    GtkWidget *note_edit_buttons;
    GtkTreePath *current_waypoint;
    GtkWidget *waypoint_editor;
    GtkWidget *waypoint_edit_buttons;
};

/* Forward declarations {{{1 */

static void ph_detail_view_init(PHDetailView *view);
static void ph_detail_view_class_init(PHDetailViewClass *cls);
static void ph_detail_view_dispose(GObject *object);
static void ph_detail_view_finalize(GObject *object);
static void ph_detail_view_set_property(GObject *object, guint id,
                                        const GValue *value, GParamSpec *spec);
static void ph_detail_view_get_property(GObject *object, guint id,
                                        GValue *value, GParamSpec *spec);

static GtkWidget *ph_detail_view_create_header(PHDetailView *view);
static GtkWidget *ph_detail_view_create_body(PHDetailView *view);
static GtkWidget *ph_detail_view_create_sidebar(PHDetailView *view);
static void ph_detail_view_create_tab_label(PHDetailView *view);

static void ph_detail_view_set_database(PHDetailView *view,
                                        PHDatabase *database);
static void ph_detail_view_geocache_updated(PHDatabase *database, gchar *id,
                                            gpointer data);
static void ph_detail_view_bulk_updated(PHDatabase *database, gpointer data);
static void ph_detail_view_store_geocache_note(PHDetailView *view);
static void ph_detail_view_store_waypoint_note(PHDetailView *view,
                                               const PHWaypointNote *note);

static gboolean ph_detail_view_load(PHDetailView *view, const gchar *id,
                                    GError **error);
static void ph_detail_view_show_geocache(PHDetailView *view,
                                         const PHGeocache *geocache,
                                         const PHWaypoint *primary);
static void ph_detail_view_show_waypoints(PHDetailView *view,
                                          const GList *waypoints);
static void ph_detail_view_show_logs(PHDetailView *view, const GList *logs);
static void ph_detail_view_show_trackables(PHDetailView *view,
                                           const GList *trackables);

static void ph_detail_view_cell_waypoint_name(
    GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model,
    GtkTreeIter *iter, gpointer data);

static void ph_detail_view_change_note(GtkTextBuffer *buffer, gpointer data);
static void ph_detail_view_cancel_note(GtkButton *button, gpointer data);
static void ph_detail_view_save_note(GtkButton *button, gpointer data);
static void ph_detail_view_toggle_found(GtkToggleButton *found_button,
                                        gpointer data);
static void ph_detail_view_waypoint_activated(
    GtkTreeView *tree_view, GtkTreePath *path,
    GtkTreeViewColumn *column, gpointer data);
static void ph_detail_view_discard_waypoint(GtkButton *button, gpointer data);
static void ph_detail_view_cancel_waypoint(GtkButton *button, gpointer data);
static void ph_detail_view_save_waypoint(GtkButton *button, gpointer data);

static gboolean ph_detail_view_handle_link(
    WebKitWebView *view, WebKitWebFrame *frame, WebKitNetworkRequest *request,
    WebKitWebNavigationAction *action, WebKitWebPolicyDecision *decision,
    gpointer data);
static void ph_detail_view_close_button_clicked(GtkButton *button,
                                                gpointer data);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHDetailView, ph_detail_view, GTK_TYPE_VBOX)

/*
 * Class initialization code.
 */
static void
ph_detail_view_class_init(PHDetailViewClass *cls)
{
    GObjectClass *g_obj_cls = G_OBJECT_CLASS(cls);

    g_obj_cls->dispose = ph_detail_view_dispose;
    g_obj_cls->finalize = ph_detail_view_finalize;
    g_obj_cls->set_property = ph_detail_view_set_property;
    g_obj_cls->get_property = ph_detail_view_get_property;

    ph_detail_view_signals[PH_DETAIL_VIEW_SIGNAL_CLOSED] =
        g_signal_new("closed", G_OBJECT_CLASS_TYPE(cls),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET(PHDetailViewClass, closed),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);

    g_object_class_install_property(g_obj_cls,
            PH_DETAIL_VIEW_PROP_DATABASE,
            g_param_spec_object("database", "database",
                "database used to load details and store notes",
                PH_TYPE_DATABASE,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    gtk_rc_parse_string("style \"ph-tab-close-button-style\" {\n"
            "GtkWidget::focus-padding = 0\n"
            "GtkWidget::focus-line-width = 0\n"
            "xthickness = 0\n"
            "ythickness = 0\n"
            "}\n"
            "widget \"*.ph-tab-close-button\""
            " style \"ph-tab-close-button-style\"");

    g_type_class_add_private(cls, sizeof(PHDetailViewPrivate));
}

/*
 * Instance initialization code.
 */
static void
ph_detail_view_init(PHDetailView *view)
{
    PHDetailViewPrivate *priv = PH_DETAIL_VIEW_GET_PRIVATE(view);
    GtkWidget *header, *paned, *body, *sidebar;

    view->priv = priv;

    g_object_set(view, "border-width", 5, "spacing", 5, NULL);

    header = ph_detail_view_create_header(view);
    body = ph_detail_view_create_body(view);
    sidebar = ph_detail_view_create_sidebar(view);

    paned = gtk_hpaned_new();
    gtk_paned_pack1(GTK_PANED(paned), body, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned), sidebar, FALSE, FALSE);

    gtk_box_pack_start(GTK_BOX(view), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(view), paned, TRUE, TRUE, 0);

    ph_detail_view_create_tab_label(view);
}

/*
 * Drop references to other objects.
 */
static void
ph_detail_view_dispose(GObject *object)
{
    PHDetailView *view = PH_DETAIL_VIEW(object);

    if (view->priv->database != NULL)
        ph_detail_view_set_database(view, NULL);

    if (G_OBJECT_CLASS(ph_detail_view_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(ph_detail_view_parent_class)->dispose(object);
}

/*
 * Instance destruction code.
 */
static void
ph_detail_view_finalize(GObject *object) {
    PHDetailView *view = PH_DETAIL_VIEW(object);

    if (view->priv->geocache_note != NULL)
        ph_geocache_note_free(view->priv->geocache_note);
    if (view->priv->current_waypoint != NULL)
        gtk_tree_path_free(view->priv->current_waypoint);

    if (G_OBJECT_CLASS(ph_detail_view_parent_class)->finalize != NULL)
        G_OBJECT_CLASS(ph_detail_view_parent_class)->finalize(object);
}

/*
 * Property mutator.
 */
static void
ph_detail_view_set_property(GObject *object,
                            guint id,
                            const GValue *value,
                            GParamSpec *spec)
{
    PHDetailView *view = PH_DETAIL_VIEW(object);

    switch (id) {
    case PH_DETAIL_VIEW_PROP_DATABASE:
        ph_detail_view_set_database(view, g_value_get_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
    }
}

/*
 * Property accessor.
 */
static void
ph_detail_view_get_property(GObject *object,
                            guint id,
                            GValue *value,
                            GParamSpec *spec)
{
    PHDetailView *view = PH_DETAIL_VIEW(object);

    switch (id) {
    case PH_DETAIL_VIEW_PROP_DATABASE:
        g_value_set_object(value, view->priv->database);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
    }
}

/* GUI construction {{{1 */

/*
 * Create the header displaying some basic facts about the geocache.
 */
static GtkWidget *
ph_detail_view_create_header(PHDetailView *view)
{
    GtkWidget *header = gtk_table_new(2, 3, FALSE);
    GtkWidget *facts = gtk_table_new(3, 2, FALSE);
    GtkWidget *label;

    view->priv->type_image = ph_sprite_image_new(PH_SPRITE_GEOCACHE,
            PH_SPRITE_SIZE_LARGE);

    view->priv->name = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(view->priv->name), 0, 0.5);

    label = gtk_label_new(_("Size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(facts), label, 0, 1, 0, 1,
            GTK_EXPAND | GTK_FILL, 0, 0, 0);
    view->priv->size_image = ph_sprite_image_new(PH_SPRITE_SIZE,
            PH_SPRITE_SIZE_SMALL);
    gtk_table_attach(GTK_TABLE(facts), view->priv->size_image, 1, 2, 0, 1,
            0, 0, 5, 0);

    label = gtk_label_new(_("Difficulty:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(facts), label, 0, 1, 1, 2,
            GTK_EXPAND | GTK_FILL, 0, 0, 0);
    view->priv->difficulty_image = ph_sprite_image_new(PH_SPRITE_DIFFICULTY,
            PH_SPRITE_SIZE_SMALL);
    gtk_table_attach(GTK_TABLE(facts), view->priv->difficulty_image, 1, 2, 1, 2,
            0, 0, 5, 0);

    label = gtk_label_new(_("Terrain:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
    gtk_table_attach(GTK_TABLE(facts), label, 0, 1, 2, 3,
            GTK_EXPAND | GTK_FILL, 0, 0, 0);
    view->priv->terrain_image = ph_sprite_image_new(PH_SPRITE_TERRAIN,
            PH_SPRITE_SIZE_SMALL);
    gtk_table_attach(GTK_TABLE(facts), view->priv->terrain_image, 1, 2, 2, 3,
            0, 0, 5, 0);

    view->priv->found = gtk_check_button_new_with_label(
            _("I found this geocache"));
    g_signal_connect(view->priv->found, "toggled",
            G_CALLBACK(ph_detail_view_toggle_found), view);

    gtk_table_attach(GTK_TABLE(header), view->priv->type_image, 0, 1, 0, 1,
            0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(header), view->priv->name, 1, 2, 0, 1,
            GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(header), facts, 2, 3, 0, 1,
            0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(header), view->priv->found, 1, 2, 1, 2,
            GTK_EXPAND | GTK_FILL, 0, 0, 0);

    return header;
}

/*
 * Create the webkit view and hint display.
 */
static GtkWidget *
ph_detail_view_create_body(PHDetailView *view)
{
    GtkWidget *body = gtk_vbox_new(FALSE, 0);
    WebKitWebSettings *settings;
    GtkWidget *scrolled;
    GtkWidget *expander;
    GtkWidget *hbox;
    GtkWidget *button;

    /* description */
    view->priv->description = webkit_web_view_new();
    settings = webkit_web_view_get_settings(
            WEBKIT_WEB_VIEW(view->priv->description));
    g_object_set(settings, "enable-default-context-menu", FALSE, NULL);
    g_signal_connect(view->priv->description,
            "navigation-policy-decision-requested",
            G_CALLBACK(ph_detail_view_handle_link), NULL);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
            GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(scrolled), view->priv->description);
    gtk_box_pack_start(GTK_BOX(body), scrolled, TRUE, TRUE, 0);

    /* geocache note */
    expander = gtk_expander_new(_("Custom note"));
    gtk_expander_set_expanded(GTK_EXPANDER(expander), TRUE);
    hbox = gtk_hbox_new(FALSE, 5);

    view->priv->note_editor = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view->priv->note_editor),
            GTK_WRAP_WORD_CHAR);
    scrolled = gtk_scrolled_window_new(
            gtk_text_view_get_hadjustment(
                GTK_TEXT_VIEW(view->priv->note_editor)),
            gtk_text_view_get_vadjustment(
                GTK_TEXT_VIEW(view->priv->note_editor)));
    g_signal_connect(
            gtk_text_view_get_buffer(GTK_TEXT_VIEW(view->priv->note_editor)),
            "modified-changed", G_CALLBACK(ph_detail_view_change_note), view);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
            GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(scrolled), view->priv->note_editor);
    gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);

    view->priv->note_edit_buttons = gtk_vbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(view->priv->note_edit_buttons),
            GTK_BUTTONBOX_START);
    button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
    g_signal_connect(button, "clicked",
            G_CALLBACK(ph_detail_view_save_note), view);
    gtk_container_add(GTK_CONTAINER(view->priv->note_edit_buttons), button);
    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect(button, "clicked",
            G_CALLBACK(ph_detail_view_cancel_note), view);
    gtk_container_add(GTK_CONTAINER(view->priv->note_edit_buttons), button);
    gtk_widget_set_sensitive(view->priv->note_edit_buttons, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), view->priv->note_edit_buttons,
            FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(expander), hbox);
    gtk_box_pack_start(GTK_BOX(body), expander, FALSE, FALSE, 0);

    /* hint */
    expander = gtk_expander_new(_("Hint (unencrypted)"));
    view->priv->hint = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view->priv->hint), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view->priv->hint),
            GTK_WRAP_WORD_CHAR);
    scrolled = gtk_scrolled_window_new(
            gtk_text_view_get_hadjustment(GTK_TEXT_VIEW(view->priv->hint)),
            gtk_text_view_get_vadjustment(GTK_TEXT_VIEW(view->priv->hint)));
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
            GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(scrolled), view->priv->hint);
    gtk_container_add(GTK_CONTAINER(expander), scrolled);
    gtk_box_pack_start(GTK_BOX(body), expander, FALSE, FALSE, 0);

    return body;
}

/*
 * Create the tree views and models for waypoints and logs.
 */
static GtkWidget *
ph_detail_view_create_sidebar(PHDetailView *view)
{
    GtkWidget *sidebar = gtk_vbox_new(FALSE, 0);
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkWidget *label;
    GtkWidget *tree_view, *scrolled;
    GtkWidget *button;
    GtkWidget *notebook;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Waypoints</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(sidebar), label, FALSE, FALSE, 0);

    /* waypoint list */
    view->priv->waypoints = gtk_list_store_new(6,
            G_TYPE_STRING,          /* id */
            G_TYPE_STRING,          /* name */
            G_TYPE_UINT,            /* type */
            PH_TYPE_WAYPOINT_NOTE,  /* note holding the current coordinates */
            G_TYPE_INT,             /* original longitude */
            G_TYPE_INT);            /* original latitude */
    tree_view = gtk_tree_view_new_with_model(
            GTK_TREE_MODEL(view->priv->waypoints));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);
    g_signal_connect(tree_view, "row-activated",
            G_CALLBACK(ph_detail_view_waypoint_activated), view);

    /* waypoint type column */
    renderer = ph_cell_renderer_sprite_new(PH_SPRITE_WAYPOINT,
            PH_SPRITE_SIZE_MEDIUM);
    column = gtk_tree_view_column_new_with_attributes("",
            renderer, "value", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    /* waypoint information column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
            ph_detail_view_cell_waypoint_name, NULL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    scrolled = gtk_scrolled_window_new(
            gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(tree_view)),
            gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(tree_view)));
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
            GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(scrolled), tree_view);
    gtk_box_pack_start(GTK_BOX(sidebar), scrolled, TRUE, TRUE, 5);

    /* waypoint editor */
    view->priv->waypoint_editor = ph_waypoint_editor_new();
    gtk_box_pack_start(GTK_BOX(sidebar), view->priv->waypoint_editor,
            FALSE, FALSE, 5);

    view->priv->waypoint_edit_buttons = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(view->priv->waypoint_edit_buttons),
            GTK_BUTTONBOX_END);

    button = gtk_button_new_from_stock(GTK_STOCK_DISCARD);
    gtk_container_add(GTK_CONTAINER(view->priv->waypoint_edit_buttons), button);
    g_signal_connect(button, "clicked",
            G_CALLBACK(ph_detail_view_discard_waypoint), view);

    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_container_add(GTK_CONTAINER(view->priv->waypoint_edit_buttons), button);
    g_signal_connect(button, "clicked",
            G_CALLBACK(ph_detail_view_cancel_waypoint), view);

    button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
    gtk_container_add(GTK_CONTAINER(view->priv->waypoint_edit_buttons), button);
    g_signal_connect(button, "clicked",
            G_CALLBACK(ph_detail_view_save_waypoint), view);

    gtk_widget_set_sensitive(view->priv->waypoint_edit_buttons, FALSE);
    gtk_box_pack_start(GTK_BOX(sidebar), view->priv->waypoint_edit_buttons,
            FALSE, FALSE, 5);

    gtk_box_pack_start(GTK_BOX(sidebar), gtk_hseparator_new(), FALSE, FALSE, 5);

    /* logs and trackables notebook */
    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(sidebar), notebook, TRUE, TRUE, 5);

    /* log vbox */
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    view->priv->logs = gtk_vbox_new(FALSE, 5);
    g_object_set(view->priv->logs, "border-width", 5, NULL);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
            view->priv->logs);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled,
            gtk_label_new(_("Logs")));

    /* trackable list */
    view->priv->trackables = gtk_list_store_new(3,
            G_TYPE_STRING,  /* id */
            G_TYPE_STRING,  /* name */
            G_TYPE_STRING); /* geocache id */
    tree_view = gtk_tree_view_new_with_model(
            GTK_TREE_MODEL(view->priv->trackables));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);

    /* trackable id */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("", renderer,
            "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    /* trackable name */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
            "ellipsize", PANGO_ELLIPSIZE_END,
            "weight", PANGO_WEIGHT_BOLD,
            NULL);
    column = gtk_tree_view_column_new_with_attributes("", renderer,
            "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    scrolled = gtk_scrolled_window_new(
            gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(tree_view)),
            gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(tree_view)));
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
            GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(scrolled), tree_view);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled,
            gtk_label_new(_("Trackables")));

    return sidebar;
}

/*
 * Create the tab label.
 */
static void
ph_detail_view_create_tab_label(PHDetailView *view)
{
    GtkWidget *close_button;

    view->priv->tab_label = gtk_hbox_new(FALSE, 4);

    view->priv->tab_image = ph_sprite_image_new(PH_SPRITE_GEOCACHE,
            PH_SPRITE_SIZE_SMALL);
    gtk_box_pack_start(GTK_BOX(view->priv->tab_label), view->priv->tab_image,
            FALSE, FALSE, 0);

    view->priv->tab_name = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(view->priv->tab_label), view->priv->tab_name,
            TRUE, TRUE, 0);

    close_button = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(close_button),
            gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
    gtk_button_set_focus_on_click(GTK_BUTTON(close_button), FALSE);
    gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);
    gtk_widget_set_name(close_button, "ph-tab-close-button");
    g_signal_connect(close_button, "clicked",
            G_CALLBACK(ph_detail_view_close_button_clicked), view);

    gtk_box_pack_start(GTK_BOX(view->priv->tab_label), close_button,
            FALSE, FALSE, 0);
    gtk_widget_show_all(view->priv->tab_label);
}

/* Database interaction {{{1 */

/*
 * Set the underlying data source.
 */
static void
ph_detail_view_set_database(PHDetailView *view,
                            PHDatabase *database)
{
    if (view->priv->database != NULL) {
        guint i;
        for (i = 0; i < G_N_ELEMENTS(view->priv->db_signal_handlers); ++i)
            g_signal_handler_disconnect(view->priv->database,
                    view->priv->db_signal_handlers[i]);
        g_object_unref(view->priv->database);
        view->priv->database = NULL;
    }

    if (database != NULL) {
        view->priv->database = g_object_ref(database);
        view->priv->db_signal_handlers[0] =
            g_signal_connect(view->priv->database, "geocache-updated",
                    G_CALLBACK(ph_detail_view_geocache_updated), view);
        view->priv->db_signal_handlers[1] =
            g_signal_connect(view->priv->database, "bulk-updated",
                    G_CALLBACK(ph_detail_view_bulk_updated), view);
    }
}

/*
 * A single geocache has been changed in the database.  If it is the one
 * displayed by this detail view, reload it.
 */
static void
ph_detail_view_geocache_updated(PHDatabase *database,
                                gchar *id,
                                gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);

    if (view->priv->updating)
        /* our own update is being thrown back at us */
        return;

    if (strcmp(id, view->priv->geocache_note->id) == 0)
        (void) ph_detail_view_load(view, id, NULL);
}

/*
 * There has been a large change to the database.  Reload the displayed data.
 */
static void
ph_detail_view_bulk_updated(PHDatabase *database,
                            gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);

    if (view->priv->updating)
        /* our own update is being thrown back at us */
        return;

    (void) ph_detail_view_load(view, view->priv->geocache_note->id, NULL);
}

/*
 * Load geocache, waypoint, log and trackable information from the database and
 * display it.
 */
static gboolean
ph_detail_view_load(PHDetailView *view,
                    const gchar *id,
                    GError **error)
{
    gboolean success = FALSE;
    PHDatabase *database = view->priv->database;
    PHGeocache *geocache;
    GList *waypoints = NULL, *trackables = NULL, *logs = NULL;

    geocache = ph_geocache_load_by_id(database, id, TRUE, error);
    if (geocache != NULL)
        success = ph_waypoints_load_by_geocache_id(&waypoints, database,
                id, TRUE, error);
    if (success)
        success = ph_logs_load_by_geocache_id(&logs, database, id, error);
    if (success)
        success = ph_trackables_load_by_geocache_id(&trackables, database,
                id, error);

    if (success) {
        PHWaypoint *primary = (PHWaypoint *) waypoints->data;

        ph_detail_view_show_geocache(view, geocache, primary);
        ph_detail_view_show_waypoints(view, waypoints);
        ph_detail_view_show_logs(view, logs);
        ph_detail_view_show_trackables(view, trackables);
    }

    ph_geocache_free(geocache);
    ph_waypoints_free(waypoints);
    ph_logs_free(logs);
    ph_trackables_free(trackables);

    return success;
}

/*
 * Store a geocache note in the database and notify.
 */
static void
ph_detail_view_store_geocache_note(PHDetailView *view)
{
    PHGeocacheNote *note = view->priv->geocache_note;
    PHDatabase *database = view->priv->database;
    GError *error = NULL;

    if (ph_geocache_note_store(note, database, &error)) {
        view->priv->updating = TRUE;
        ph_database_notify_geocache_update(database, note->id);
        view->priv->updating = FALSE;
    }
    else {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(view));
        if (GTK_IS_WINDOW(toplevel)) {
            GtkWidget *message = gtk_message_dialog_new(
                GTK_WINDOW(toplevel), GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_CANCEL,
                _("Cannot write the geocache note to the database."));
            gtk_message_dialog_format_secondary_text(
                GTK_MESSAGE_DIALOG(message), "%s.", error->message);
            gtk_dialog_run(GTK_DIALOG(message));
            gtk_widget_destroy(message);
        }
        g_error_free(error);
    }
}

/*
 * Store a waypoint note in the database and notify.
 */
static void
ph_detail_view_store_waypoint_note(PHDetailView *view,
                                   const PHWaypointNote *note)
{
    PHDatabase *database = view->priv->database;
    gchar *geocache_id = view->priv->geocache_note->id;
    GError *error = NULL;

    if (ph_waypoint_note_store(note, database, &error)) {
        view->priv->updating = TRUE;
        ph_database_notify_geocache_update(database, geocache_id);
        view->priv->updating = FALSE;
    }
    else {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(view));
        if (GTK_IS_WINDOW(toplevel)) {
            GtkWidget *message = gtk_message_dialog_new(
                GTK_WINDOW(toplevel), GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_CANCEL,
                _("Cannot write the waypoint note to the database."));
            gtk_message_dialog_format_secondary_text(
                GTK_MESSAGE_DIALOG(message), "%s.", error->message);
            gtk_dialog_run(GTK_DIALOG(message));
            gtk_widget_destroy(message);
        }
        g_error_free(error);
    }
}

/* Show a geocache {{{1 */

static const gchar *ph_detail_view_css =
    "body { background: #657b83 }\n"
    "#plastichunt-summary, #plastichunt-description { border: 1px solid black; "
    "margin: 1ex; padding: 1ex }\n"
    "#plastichunt-description { background: white }\n"
    "#plastichunt-summary { background: #fdf6e3 }\n"
    ".plastichunt-h1 { font-size: 100%; color: white }\n"
    "pre { white-space: pre-wrap }\n";

/*
 * Display some geocache information, such as its name, size, ratings, short
 * and long description texts.
 */
static void
ph_detail_view_show_geocache(PHDetailView *view,
                             const PHGeocache *geocache,
                             const PHWaypoint *primary)
{
    guint sprite_value;
    gchar *markup;
    GString *html;

    sprite_value = ph_sprite_value_for_geocache(geocache);

    gtk_label_set_text(GTK_LABEL(view->priv->tab_name), geocache->name);
    ph_sprite_image_set_value(PH_SPRITE_IMAGE(view->priv->tab_image),
            sprite_value);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view->priv->found),
            geocache->logged || geocache->note.found);
    if (geocache->logged)
        gtk_widget_set_sensitive(view->priv->found, FALSE);

    markup = g_markup_printf_escaped(
            _("<span size=\"large\" weight=\"bold\">%s</span>\n"
                "<span color=\"#333333\">%s by %s</span>"),
            geocache->name, geocache->id, geocache->owner);
    gtk_label_set_markup(GTK_LABEL(view->priv->name), markup);
    g_free(markup);

    ph_sprite_image_set_value(PH_SPRITE_IMAGE(view->priv->type_image),
            sprite_value);
    ph_sprite_image_set_value(PH_SPRITE_IMAGE(view->priv->size_image),
            geocache->size);
    ph_sprite_image_set_value(PH_SPRITE_IMAGE(view->priv->difficulty_image),
            geocache->difficulty);
    ph_sprite_image_set_value(PH_SPRITE_IMAGE(view->priv->terrain_image),
            geocache->terrain);

    html = g_string_new("<html><head><style>");
    g_string_append(html, ph_detail_view_css);
    g_string_append(html, "</style></head><body>");

    if (geocache->summary != NULL &&
        !ph_util_all_whitespace(geocache->summary)) {

        g_string_append(html, _("<h1 class=\"plastichunt-h1\">Summary</h1>"));
        g_string_append(html, "<div id=\"plastichunt-summary\">");
        if (geocache->summary_html)
            g_string_append(html, geocache->summary);
        else {
            markup = g_markup_escape_text(geocache->summary, -1);
            g_string_append_printf(html, "<pre>%s</pre>", markup);
            g_free(markup);
        }
        g_string_append(html, "</div>");
    }

    g_string_append(html, _("<h1 class=\"plastichunt-h1\">Description</h1>"));
    g_string_append(html, "<div id=\"plastichunt-description\">");
    if (geocache->description_html)
        g_string_append(html, geocache->description);
    else {
        markup = g_markup_escape_text(geocache->description, -1);
        g_string_append_printf(html, "<pre>%s</pre>", markup);
        g_free(markup);
    }

    g_string_append(html, "</div></body></html>");

    webkit_web_view_load_string(WEBKIT_WEB_VIEW(view->priv->description),
            html->str, NULL, NULL, primary->url);

    gtk_text_buffer_set_text(
            gtk_text_view_get_buffer(GTK_TEXT_VIEW(view->priv->note_editor)),
            (geocache->note.note != NULL) ? geocache->note.note : "", -1);

    gtk_text_buffer_set_text(
            gtk_text_view_get_buffer(GTK_TEXT_VIEW(view->priv->hint)),
            geocache->hint, -1);

    g_string_free(html, TRUE);

    ph_geocache_note_free(view->priv->geocache_note);
    view->priv->geocache_note = ph_geocache_note_copy(&geocache->note);
}

/*
 * Build the list model for the waypoints.
 */
static void
ph_detail_view_show_waypoints(PHDetailView *view,
                              const GList *waypoints)
{
    GtkListStore *list_store = view->priv->waypoints;

    gtk_list_store_clear(list_store);

    for (; waypoints != NULL; waypoints = waypoints->next) {
        const PHWaypoint *current = (const PHWaypoint *) waypoints->data;
        GtkTreeIter iter;

        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
                0, current->id,
                1, current->name,
                2, current->type,
                3, &current->note,
                4, current->longitude,
                5, current->latitude,
                -1);
    }
}

/*
 * Build the list model for the logs.
 */
static void
ph_detail_view_show_logs(PHDetailView *view,
                         const GList *logs)
{
    const gchar *message;
    gchar *markup;
    GtkWidget *label, *details, *frame;
    gchar date[100];
    GtkTextBuffer *buffer;
    gboolean first = TRUE;
    GDate logged;

    gtk_container_foreach(GTK_CONTAINER(view->priv->logs),
            (GtkCallback) gtk_widget_destroy, NULL);

    for (; logs != NULL; logs = logs->next) {
        const PHLog *current = (const PHLog *) logs->data;

        switch (current->type) {
        case PH_LOG_TYPE_FOUND:
            message = _("On %s, %s <b>found</b> it:");
            break;
        case PH_LOG_TYPE_NOT_FOUND:
            message = _("On %s, %s <b>did not find</b> it:");
            break;
        case PH_LOG_TYPE_NOTE:
            message = _("On %s, %s <b>remarked</b>:");
            break;
        case PH_LOG_TYPE_REVIEWER:
            message = _("On %s, %s posted a <b>reviewer note</b>:");
            break;
        case PH_LOG_TYPE_PUBLISH:
            message = _("On %s, %s <b>published</b> it:");
            break;
        case PH_LOG_TYPE_ENABLE:
            message = _("On %s, %s <b>enabled</b> it:");
            break;
        case PH_LOG_TYPE_DISABLE:
            message = _("On %s, %s <b>disabled</b> it:");
            break;
        case PH_LOG_TYPE_UPDATE:
            message = _("On %s, %s <b>updated</b> the coordinates:");
            break;
        case PH_LOG_TYPE_WILL_ATTEND:
            message = _("On %s, %s <b>will attend</b>:");
            break;
        case PH_LOG_TYPE_ATTENDED:
            message = _("On %s, %s <b>was there</b>:");
            break;
        case PH_LOG_TYPE_WEBCAM:
            message = _("On %s, %s posted a <b>webcam picture</b>:");
            break;
        case PH_LOG_TYPE_NEEDS_MAINTENANCE:
            message = _("On %s, %s <b>asks for maintenance</b>:");
            break;
        case PH_LOG_TYPE_MAINTENANCE:
            message = _("On %s, %s <b>took care</b> of it:");
            break;
        case PH_LOG_TYPE_NEEDS_ARCHIVING:
            message = _("On %s, %s thinks it <b>should be archived</b>:");
            break;
        case PH_LOG_TYPE_ARCHIVED:
            message = _("On %s, %s <b>archived</b> it:");
            break;
        case PH_LOG_TYPE_UNARCHIVED:
            message = _("On %s, %s <b>unarchived</b> it:");
            break;
        default:
            message = _("On %s, %s said:");
        }

        g_date_clear(&logged, 1);
        g_date_set_time_t(&logged, current->logged);
        (void) g_date_strftime(date, sizeof(date), "%x", &logged);
        markup = g_markup_printf_escaped(message, date, current->logger);

        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), markup);
        g_free(markup);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        buffer = gtk_text_buffer_new(NULL);
        gtk_text_buffer_set_text(buffer, current->details, -1);
        details = gtk_text_view_new_with_buffer(buffer);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(details), GTK_WRAP_WORD_CHAR);
        gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(details), 5);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(details), FALSE);

        frame = gtk_frame_new(NULL);
        gtk_container_add(GTK_CONTAINER(frame), details);
        gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

        if (first)
            first = FALSE;
        else
            gtk_box_pack_start(GTK_BOX(view->priv->logs),
                    gtk_hseparator_new(), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(view->priv->logs),
                label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(view->priv->logs),
                frame, FALSE, FALSE, 0);
    }
}

/*
 * Build the list model for the trackables.
 */
static void
ph_detail_view_show_trackables(PHDetailView *view,
                               const GList *trackables)
{
    GtkListStore *list_store = view->priv->trackables;

    gtk_list_store_clear(list_store);

    for (; trackables != NULL; trackables = trackables->next) {
        const PHTrackable *current = (const PHTrackable *) trackables->data;
        GtkTreeIter iter;

        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
                0, current->id,
                1, current->name,
                2, current->geocache_id,
                -1);
    }
}

/* Cell data functions {{{1 */

/*
 * Render a waypoint name (plus a couple of details) in the tree view.
 */
static void
ph_detail_view_cell_waypoint_name(GtkTreeViewColumn *column,
                                  GtkCellRenderer *renderer,
                                  GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  gpointer data)
{
    guint type;
    gchar *id, *short_id, *name;
    gchar *markup;
    gchar *coords;
    PHWaypointNote *note;

    gtk_tree_model_get(model, iter,
            0, &id, 1, &name, 2, &type,
            3, &note, -1);

    coords = ph_geo_minfrac_to_string(note->new_longitude, note->new_latitude);
    short_id = strchr(id, ',');
    if (short_id == NULL)
        short_id = id;
    else
        ++short_id;

    markup = g_markup_printf_escaped("<b>%s</b> <small>(%s)</small>\n"
            "<small>%s%s</small>",
            name,
            (type == PH_WAYPOINT_TYPE_GEOCACHE)
                ? _("header coordinates")
                : short_id,
            coords,
            note->custom ? _(" (changed)") : "");
    g_object_set(renderer, "markup", markup, NULL);

    g_free(id);
    g_free(name);
    g_free(coords);
    g_free(markup);
}


/* Altering notes {{{1 */

/*
 * The custom note text has changed.  Activate the save and cancel buttons.
 */
static void
ph_detail_view_change_note(GtkTextBuffer *buffer,
                           gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);

    gtk_widget_set_sensitive(view->priv->note_edit_buttons,
            gtk_text_buffer_get_modified(buffer));
}

/*
 * The user wants to undo their changes to the custom note text.
 */
static void
ph_detail_view_cancel_note(GtkButton *button,
                           gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);

    if (view->priv->geocache_note != NULL) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(
                GTK_TEXT_VIEW(view->priv->note_editor));
        const gchar *note = view->priv->geocache_note->note;

        gtk_text_buffer_set_text(buffer, (note != NULL) ? note : "", -1);
        gtk_text_buffer_set_modified(buffer, FALSE);
    }
}

/*
 * The user wants to save the custom note text.
 */
static void
ph_detail_view_save_note(GtkButton *button,
                         gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);

    if (view->priv->geocache_note != NULL) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(
                GTK_TEXT_VIEW(view->priv->note_editor));
        GtkTextIter start, end;
        gchar *note;
        guint new_value = ph_sprite_image_get_value(
                PH_SPRITE_IMAGE(view->priv->tab_image));

        if (view->priv->geocache_note->note != NULL)
            g_free(view->priv->geocache_note->note);

        gtk_text_buffer_get_start_iter(buffer, &start);
        gtk_text_buffer_get_end_iter(buffer, &end);
        note = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
        if (note[0] == '\0') {
            g_free(note);
            view->priv->geocache_note->note = NULL;
            new_value &= ~PH_SPRITE_GEOCACHE_NOTES;
        }
        else {
            view->priv->geocache_note->note = note;
            new_value |= PH_SPRITE_GEOCACHE_NOTES;
        }

        ph_sprite_image_set_value(PH_SPRITE_IMAGE(view->priv->tab_image),
                new_value);
        ph_sprite_image_set_value(PH_SPRITE_IMAGE(view->priv->type_image),
                new_value);

        ph_detail_view_store_geocache_note(view);

        gtk_text_buffer_set_modified(buffer, FALSE);
    }
}

/*
 * The "found" state of the displayed geocache has changed.
 */
static void
ph_detail_view_toggle_found(GtkToggleButton *found_button,
                            gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);

    if (view->priv->geocache_note != NULL) {
        gboolean found = gtk_toggle_button_get_active(found_button);
        guint new_value;

        view->priv->geocache_note->found = found;

        new_value = ph_sprite_image_get_value(
                PH_SPRITE_IMAGE(view->priv->tab_image));
        if (found)
            new_value |= PH_SPRITE_GEOCACHE_FOUND;
        else
            new_value &= ~PH_SPRITE_GEOCACHE_FOUND;

        ph_sprite_image_set_value(PH_SPRITE_IMAGE(view->priv->tab_image),
                new_value);
        ph_sprite_image_set_value(PH_SPRITE_IMAGE(view->priv->type_image),
                new_value);

        ph_detail_view_store_geocache_note(view);
    }
}

/*
 * Begin editing a waypoint.
 */
static void
ph_detail_view_waypoint_activated(GtkTreeView *tree_view,
                                  GtkTreePath *path,
                                  GtkTreeViewColumn *column,
                                  gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);
    GtkTreeModel *model = GTK_TREE_MODEL(view->priv->waypoints);
    GtkTreeIter iter;
    PHWaypointNote *note;

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, 3, &note, -1);

    ph_waypoint_editor_start(PH_WAYPOINT_EDITOR(view->priv->waypoint_editor),
            PH_GEO_MINFRAC_TO_DEG(note->new_latitude),
            PH_GEO_MINFRAC_TO_DEG(note->new_longitude));
    gtk_widget_set_sensitive(view->priv->waypoint_edit_buttons, TRUE);
    ph_waypoint_note_free(note);
    gtk_tree_path_free(view->priv->current_waypoint);
    view->priv->current_waypoint = gtk_tree_path_copy(path);
}

/*
 * Stop editing a waypoint, reverting it to its original state (as imported).
 */
static void
ph_detail_view_discard_waypoint(GtkButton *button,
                                gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);
    GtkTreeModel *model = GTK_TREE_MODEL(view->priv->waypoints);
    GtkTreeIter iter;
    PHWaypointNote *note;
    gint old_latitude, old_longitude;

    gtk_tree_model_get_iter(model, &iter, view->priv->current_waypoint);
    gtk_tree_model_get(model, &iter, 3, &note,
            4, &old_longitude, 5, &old_latitude, -1);

    note->custom = FALSE;
    note->new_latitude = old_latitude;
    note->new_longitude = old_longitude;
    gtk_list_store_set(view->priv->waypoints, &iter, 3, note, -1);

    ph_detail_view_store_waypoint_note(view, note);

    ph_waypoint_note_free(note);
    ph_waypoint_editor_end(PH_WAYPOINT_EDITOR(view->priv->waypoint_editor),
            NULL, NULL);
    ph_detail_view_cancel_waypoint(button, data);
}

/*
 * Cancel editing a waypoint without saving.
 */
static void
ph_detail_view_cancel_waypoint(GtkButton *button,
                               gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);

    ph_waypoint_editor_end(PH_WAYPOINT_EDITOR(view->priv->waypoint_editor),
            NULL, NULL);
    gtk_widget_set_sensitive(view->priv->waypoint_edit_buttons, FALSE);
    gtk_tree_path_free(view->priv->current_waypoint);
    view->priv->current_waypoint = NULL;
}

/*
 * Commit changes on a waypoint and stop editing.
 */
static void
ph_detail_view_save_waypoint(GtkButton *button,
                             gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);
    GtkTreeModel *model = GTK_TREE_MODEL(view->priv->waypoints);
    GtkTreeIter iter;
    PHWaypointNote *note;
    gdouble latitude, longitude;

    gtk_tree_model_get_iter(model, &iter, view->priv->current_waypoint);
    gtk_tree_model_get(model, &iter, 3, &note, -1);

    ph_waypoint_editor_end(PH_WAYPOINT_EDITOR(view->priv->waypoint_editor),
            &latitude, &longitude);

    note->custom = TRUE;
    note->new_latitude = PH_GEO_DEG_TO_MINFRAC(latitude);
    note->new_longitude = PH_GEO_DEG_TO_MINFRAC(longitude);
    gtk_list_store_set(view->priv->waypoints, &iter, 3, note, -1);

    ph_detail_view_store_waypoint_note(view, note);

    ph_waypoint_note_free(note);
    ph_detail_view_cancel_waypoint(button, data);
}

/* WebKit policy {{{1 */

/*
 * Do not allow any kind of navigation (following links, going
 * backward/forward, etc.) in the description view and open links in an external
 * browser instead.
 */
static gboolean
ph_detail_view_handle_link(WebKitWebView *view,
                           WebKitWebFrame *frame,
                           WebKitNetworkRequest *request,
                           WebKitWebNavigationAction *action,
                           WebKitWebPolicyDecision *decision,
                           gpointer data)
{
    WebKitWebNavigationReason reason =
        webkit_web_navigation_action_get_reason(action);

    if (reason != WEBKIT_WEB_NAVIGATION_REASON_OTHER) {
        const gchar *uri = webkit_network_request_get_uri(request);
        ph_util_open_in_browser(uri);
        webkit_web_policy_decision_ignore(decision);
        return TRUE;
    }

    return FALSE;
}

/*
 * The close button on the tab has been clicked: Pass it on as the "close"
 * signal.
 */
static void
ph_detail_view_close_button_clicked(GtkButton *button,
                                    gpointer data)
{
    PHDetailView *view = PH_DETAIL_VIEW(data);

    g_signal_emit(view,
            ph_detail_view_signals[PH_DETAIL_VIEW_SIGNAL_CLOSED],
            0);
}

/* Public interface {{{1 */

/*
 * Create a new PHDetailView which will load and display the geocache with the
 * specified ID.
 */
GtkWidget *
ph_detail_view_new(PHDatabase *database,
                   const gchar *id,
                   GError **error)
{
    PHDetailView *view;

    g_return_val_if_fail(database != NULL && PH_IS_DATABASE(database), NULL);

    view = PH_DETAIL_VIEW(g_object_new(PH_TYPE_DETAIL_VIEW,
                "database", database, NULL));
    ph_detail_view_load(view, id, error);
    return GTK_WIDGET(view);
}

/*
 * Get the notebook tab label for this view.
 */
GtkWidget *
ph_detail_view_get_label(PHDetailView *view)
{
    g_return_val_if_fail(view != NULL && PH_IS_DETAIL_VIEW(view), NULL);

    return view->priv->tab_label;
}

/*
 * Get the waypoint ID of the geocache being displayed through this detail view.
 */
const gchar *
ph_detail_view_get_geocache_id(PHDetailView *view)
{
    g_return_val_if_fail(view != NULL && PH_IS_DETAIL_VIEW(view), NULL);

    return view->priv->geocache_note->id;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
