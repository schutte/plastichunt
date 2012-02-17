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

#include "ph-cell-renderer-facts.h"
#include "ph-cell-renderer-sprite.h"
#include "ph-common.h"
#include "ph-config.h"
#include "ph-config-dialog.h"
#include "ph-detail-view.h"
#include "ph-geocache.h"
#include "ph-geocache-list.h"
#include "ph-geocache-map.h"
#include "ph-import-dialog.h"
#include "ph-log.h"
#include "ph-main-window.h"
#include "ph-map-provider-action.h"
#include "ph-trackable.h"
#include "ph-util.h"
#include "ph-waypoint.h"
#include "ph-zoom-action.h"
#include <glib/gi18n.h>

/* Properties {{{1 */

enum {
    PH_MAIN_WINDOW_PROP_0,
    PH_MAIN_WINDOW_PROP_DATABASE
};

/* Private data {{{1 */

#define PH_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
            PH_TYPE_MAIN_WINDOW, PHMainWindowPrivate))

struct _PHMainWindowPrivate {
    PHDatabase *database;               /* database connection */
    PHGeocacheList *geocache_list;      /* data for tree view and map */
    GtkWidget *tree_view;               /* geocache list view */
    GtkWidget *geocache_map;            /* map widget on the first page */
    GtkWidget *filter_entry;            /* filter entry on the top left */

    GtkWidget *view_notebook;           /* main notebook */
    GHashTable *view_table;             /* map geocache IDs to notebook pages */
};

/* Forward declarations {{{1 */

static void ph_main_window_class_init(PHMainWindowClass *cls);
static void ph_main_window_init(PHMainWindow *window);
static void ph_main_window_dispose(GObject *object);
static void ph_main_window_finalize(GObject *object);
static void ph_main_window_set_property(GObject *object, guint id,
                                        const GValue *value, GParamSpec *spec);
static void ph_main_window_get_property(GObject *object, guint id,
                                        GValue *value, GParamSpec *spec);

static void ph_main_window_create_gui(PHMainWindow *window);
static GtkWidget *ph_main_window_create_geocache_map(PHMainWindow *window);
static GtkUIManager *ph_main_window_create_ui_manager(PHMainWindow *window);

static void ph_main_window_filter_entry_activated(GtkEntry *entry,
                                                  gpointer data);
static void ph_main_window_filter_entry_icon_pressed(
    GtkEntry *entry, GtkEntryIconPosition pos, GdkEvent *event, gpointer data);

static void ph_main_window_map_geocache_activated(
    PHGeocacheMap *map, GtkTreePath *path, gpointer data);
static void ph_main_window_list_row_activated(
    GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column,
    gpointer data);
static void ph_main_window_map_geocache_selected(
    PHGeocacheMap *map, GtkTreePath *path, gpointer data);
static void ph_main_window_list_row_selected(GtkTreeSelection *sel,
                                             gpointer data);

static void ph_main_window_change_map_provider(
    PHMapProviderAction *action, GtkTreePath *path, gpointer data);

static void ph_main_window_zoom(GtkAction *action, gpointer data);

static void ph_main_window_close_tab(GtkWidget *widget, gpointer data);

static void ph_main_window_choose_database(GtkAction *action, gpointer data);
static void ph_main_window_open_recent_database(GtkRecentChooser *chooser,
                                                gpointer data);
static void ph_main_window_open_database(PHMainWindow *window,
                                         const gchar *path);
static void ph_main_window_set_database(PHMainWindow *window,
                                        PHDatabase *database);

static void ph_main_window_import(GtkAction *action, gpointer data);

static void ph_main_window_preferences(GtkAction *action, gpointer data);

static void ph_main_window_image_data_func(
    GtkTreeViewColumn *col, GtkCellRenderer *cr, GtkTreeModel *model,
    GtkTreeIter *iter, gpointer data);
static void ph_main_window_name_data_func(
    GtkTreeViewColumn *col, GtkCellRenderer *cr,
    GtkTreeModel *model, GtkTreeIter *iter, gpointer data);

static void ph_main_window_quit(GtkAction *action, gpointer data);
static gboolean ph_main_window_delete_event(GtkWidget *widget,
                                            GdkEventAny *event);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHMainWindow, ph_main_window, GTK_TYPE_WINDOW)

/*
 * Class initialization code.
 */
static void
ph_main_window_class_init(PHMainWindowClass *cls)
{

    GObjectClass *g_obj_cls = G_OBJECT_CLASS(cls);
    GtkWidgetClass *gtk_widget_cls = GTK_WIDGET_CLASS(cls);

    g_obj_cls->dispose = ph_main_window_dispose;
    g_obj_cls->finalize = ph_main_window_finalize;
    g_obj_cls->set_property = ph_main_window_set_property;
    g_obj_cls->get_property = ph_main_window_get_property;

    gtk_widget_cls->delete_event = ph_main_window_delete_event;

    g_type_class_add_private(cls, sizeof(PHMainWindowPrivate));

    g_object_class_install_property(g_obj_cls,
            PH_MAIN_WINDOW_PROP_DATABASE,
            g_param_spec_object("database", "database",
                "database used to load and store geocache information",
                PH_TYPE_DATABASE,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

/*
 * Instance counter.
 */
static guint ph_main_window_instances = 0;

/*
 * Instance initialization code.
 */
static void
ph_main_window_init(PHMainWindow *window)
{
    PHMainWindowPrivate *priv = PH_MAIN_WINDOW_GET_PRIVATE(window);

    ++ph_main_window_instances;

    window->priv = priv;

    window->priv->view_table = g_hash_table_new_full(g_str_hash, g_str_equal,
            g_free, NULL);
    window->priv->geocache_list = ph_geocache_list_new();

    ph_main_window_create_gui(window);
}

/*
 * Drop references to other objects.
 */
static void
ph_main_window_dispose(GObject *object)
{
    PHMainWindow *window = PH_MAIN_WINDOW(object);

    if (window->priv->geocache_list != NULL) {
        g_object_unref(window->priv->geocache_list);
        window->priv->geocache_list = NULL;
    }
    if (window->priv->database != NULL) {
        g_object_unref(window->priv->database);
        window->priv->database = NULL;
    }

    if (G_OBJECT_CLASS(ph_main_window_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(ph_main_window_parent_class)->dispose(object);
}

/*
 * Instance destruction code.
 */
static void
ph_main_window_finalize(GObject *object)
{
    PHMainWindow *window = PH_MAIN_WINDOW(object);

    --ph_main_window_instances;

    if (window->priv->view_table != NULL)
        g_hash_table_destroy(window->priv->view_table);

    if (G_OBJECT_CLASS(ph_main_window_parent_class)->finalize != NULL)
        G_OBJECT_CLASS(ph_main_window_parent_class)->finalize(object);
}

/*
 * Property mutator.
 */
static void
ph_main_window_set_property(GObject *object,
                            guint id,
                            const GValue *value,
                            GParamSpec *spec)
{
    PHMainWindow *window = PH_MAIN_WINDOW(object);

    switch (id) {
    case PH_MAIN_WINDOW_PROP_DATABASE:
        ph_main_window_set_database(window,
                PH_DATABASE(g_value_get_object(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
    }
}

/*
 * Property accessor.
 */
static void
ph_main_window_get_property(GObject *object,
                            guint id,
                            GValue *value,
                            GParamSpec *spec)
{
    PHMainWindow *window = PH_MAIN_WINDOW(object);

    switch (id) {
    case PH_MAIN_WINDOW_PROP_DATABASE:
        g_value_set_object(value, window->priv->database);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
    }
}

/* GUI construction {{{1 */

/*
 * Menu and toolbar entries.
 */
static GtkActionEntry ph_main_window_action_entries[] = {
    { "DatabaseMenu", NULL, N_("_Database"), NULL, NULL, NULL },

    { "OpenDatabase", GTK_STOCK_OPEN, N_("_Open…"), "<Control>o", NULL,
        G_CALLBACK(ph_main_window_choose_database) },
    { "RecentDatabases", NULL, N_("_Recently used"), NULL, NULL, NULL },
    { "ImportFile", NULL, N_("_Import file…"), "<Control>i", NULL,
        G_CALLBACK(ph_main_window_import) },
    { "ExportFile", NULL, N_("_Export file…"), "<Control>e", NULL, NULL },
    { "Quit", GTK_STOCK_QUIT, N_("_Quit"), "<Control>q", NULL,
        G_CALLBACK(ph_main_window_quit) },

    { "EditMenu", NULL, N_("_Edit"), NULL, NULL, NULL },

    { "Preferences", GTK_STOCK_PREFERENCES, N_("Preferences…"), NULL, NULL,
        G_CALLBACK(ph_main_window_preferences) },

    { "MapMenu", NULL, N_("_Map"), NULL, NULL, NULL },

    { "ZoomIn", GTK_STOCK_ZOOM_IN, N_("Zoom _in"), "<Control>plus", NULL,
        G_CALLBACK(ph_main_window_zoom) },
    { "ZoomOut", GTK_STOCK_ZOOM_OUT, N_("Zoom _out"), "<Control>minus", NULL,
        G_CALLBACK(ph_main_window_zoom) }
};

/*
 * Build the graphical user interface.
 */
static void
ph_main_window_create_gui(PHMainWindow *window)
{
    GtkWidget *main_vbox = gtk_vbox_new(FALSE, 0);
    GtkUIManager *ui_manager;
    GtkWidget *hpaned = gtk_hpaned_new();
    GtkWidget *list_vbox = gtk_vbox_new(FALSE, 3);
    GtkWidget *tree_view_scroll;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;
    GtkTreeSelection *sel;
    gint width;

    gtk_window_set_title(GTK_WINDOW(window), g_get_application_name());
    gtk_window_set_default_size(GTK_WINDOW(window),
            width = gdk_screen_get_width(gdk_screen_get_default()) * 0.8,
            gdk_screen_get_height(gdk_screen_get_default()) * 0.8);

    window->priv->filter_entry = gtk_entry_new();
    gtk_entry_set_icon_from_stock(GTK_ENTRY(window->priv->filter_entry),
            GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_FIND);
    g_signal_connect(window->priv->filter_entry, "activate",
            G_CALLBACK(ph_main_window_filter_entry_activated), window);
    g_signal_connect(window->priv->filter_entry, "icon-press",
            G_CALLBACK(ph_main_window_filter_entry_icon_pressed), window);

    window->priv->tree_view = gtk_tree_view_new_with_model(
            GTK_TREE_MODEL(window->priv->geocache_list));
    tree_view_scroll = gtk_scrolled_window_new(
            gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(
                    window->priv->tree_view)),
            gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(
                    window->priv->tree_view)));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(window->priv->tree_view),
            FALSE);

    column = gtk_tree_view_column_new();
    cell = ph_cell_renderer_sprite_new(PH_SPRITE_GEOCACHE,
                PH_SPRITE_SIZE_MEDIUM);
    gtk_tree_view_column_pack_start(column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func(column, cell,
            ph_main_window_image_data_func,
            NULL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(window->priv->tree_view), column);

    column = gtk_tree_view_column_new();
    cell = gtk_cell_renderer_text_new();
    g_object_set(cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_column_pack_start(column, cell, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, cell,
            ph_main_window_name_data_func,
            NULL, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(window->priv->tree_view), column);

    column = gtk_tree_view_column_new();
    cell = ph_cell_renderer_facts_new(PH_CELL_RENDERER_FACTS_SHOW_ALL,
            PH_SPRITE_SIZE_TINY);
    gtk_tree_view_column_pack_start(column, cell, FALSE);
    gtk_tree_view_column_set_attributes(column, cell,
            "geocache-size", PH_GEOCACHE_LIST_COLUMN_SIZE,
            "geocache-difficulty", PH_GEOCACHE_LIST_COLUMN_DIFFICULTY,
            "geocache-terrain", PH_GEOCACHE_LIST_COLUMN_TERRAIN,
            NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(window->priv->tree_view), column);

    g_signal_connect(window->priv->tree_view, "row-activated",
            G_CALLBACK(ph_main_window_list_row_activated), window);
    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->priv->tree_view));
    g_signal_connect(sel, "changed",
            G_CALLBACK(ph_main_window_list_row_selected), window);

    window->priv->geocache_map = ph_main_window_create_geocache_map(window);

    window->priv->view_notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(window->priv->view_notebook),
            TRUE);

    gtk_box_pack_start(GTK_BOX(list_vbox), window->priv->filter_entry,
            FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(tree_view_scroll), window->priv->tree_view);
    gtk_box_pack_start(GTK_BOX(list_vbox), tree_view_scroll, TRUE, TRUE, 0);
    gtk_paned_pack1(GTK_PANED(hpaned), list_vbox, FALSE, FALSE);
    gtk_notebook_append_page(GTK_NOTEBOOK(window->priv->view_notebook),
            window->priv->geocache_map, gtk_label_new(_("Map")));
    gtk_paned_pack2(GTK_PANED(hpaned), window->priv->view_notebook,
            TRUE, FALSE);

    gtk_box_pack_end(GTK_BOX(main_vbox), hpaned, TRUE, TRUE, 0);

    ui_manager = ph_main_window_create_ui_manager(window);

    gtk_window_add_accel_group(GTK_WINDOW(window),
            gtk_ui_manager_get_accel_group(ui_manager));

    gtk_box_pack_start(GTK_BOX(main_vbox),
            gtk_ui_manager_get_widget(ui_manager, "/MainWindowMenuBar"),
            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox),
            gtk_ui_manager_get_widget(ui_manager, "/MainWindowToolBar"),
            FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), main_vbox);
}

/*
 * Create the main geocache map.
 */
static GtkWidget *
ph_main_window_create_geocache_map(PHMainWindow *window)
{
    GtkWidget *result = ph_geocache_map_new();

    ph_map_set_lonlat(PH_MAP(result), 11.392778, 47.267222);
    ph_geocache_map_set_list(PH_GEOCACHE_MAP(result),
            window->priv->geocache_list);
    g_signal_connect(result, "geocache-activated",
            G_CALLBACK(ph_main_window_map_geocache_activated), window);
    g_signal_connect(result, "geocache-selected",
            G_CALLBACK(ph_main_window_map_geocache_selected), window);

    return result;
}

/*
 * Set up menus and toolbars.
 */
static GtkUIManager *
ph_main_window_create_ui_manager(PHMainWindow *window)
{
    GtkUIManager *ui_manager;
    GtkActionGroup *action_group;
    GtkAction *action;
    gchar *ui_path;
    GtkWidget *recent;
    GtkWidget *recent_item;
    GtkRecentFilter *filter;
    GtkAdjustment *zoom;

    action_group = gtk_action_group_new("MainWindowActions");
    gtk_action_group_add_actions(action_group, ph_main_window_action_entries,
            G_N_ELEMENTS(ph_main_window_action_entries), window);

    zoom = ph_map_get_zoom_adjustment(PH_MAP(window->priv->geocache_map));
    action = ph_zoom_action_new("ZoomScale", zoom);
    gtk_action_group_add_action(action_group, action);

    action = ph_map_provider_action_new("MapProviders");
    gtk_action_group_add_action(action_group, action);
    g_signal_connect(action, "changed",
            G_CALLBACK(ph_main_window_change_map_provider), window);
    ph_map_provider_action_set_selected_index(PH_MAP_PROVIDER_ACTION(action),
            0);

    ui_manager = gtk_ui_manager_new();
    gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);
    ui_path = ph_util_find_data_file(PH_UI_LOCATION, "main-window.xml");
    gtk_ui_manager_add_ui_from_file(ui_manager, ui_path, NULL);

    recent_item = gtk_ui_manager_get_widget(ui_manager,
            "/MainWindowMenuBar/DatabaseMenu/RecentDatabases");
    recent = gtk_recent_chooser_menu_new_for_manager(
            gtk_recent_manager_get_default());
    g_signal_connect(recent, "item-activated",
            G_CALLBACK(ph_main_window_open_recent_database), window);
    filter = gtk_recent_filter_new();
    gtk_recent_filter_add_mime_type(filter, PH_DATABASE_MIME_TYPE);
    gtk_recent_chooser_add_filter(GTK_RECENT_CHOOSER(recent), filter);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(recent_item), recent);

    return ui_manager;
}

/* Geocache list filter {{{1 */

/*
 * Filter entry activated: Update the query of the geocache list.
 */
static void
ph_main_window_filter_entry_activated(GtkEntry *entry,
                                      gpointer data)
{
    GError *error = NULL;
    PHMainWindow *window = PH_MAIN_WINDOW(data);

    if (!ph_geocache_list_set_query(window->priv->geocache_list,
            gtk_entry_get_text(entry), &error)) {
        GtkWidget *message = gtk_message_dialog_new(
            GTK_WINDOW(window), GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_CANCEL,
            _("Cannot filter geocache list."));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message),
                                                 "%s.", error->message);
        gtk_dialog_run(GTK_DIALOG(message));
        gtk_widget_destroy(message);
        g_error_free(error);
    }
}

/*
 * Filter entry icon pressed.
 */
static void
ph_main_window_filter_entry_icon_pressed(GtkEntry *entry,
                                         GtkEntryIconPosition pos,
                                         GdkEvent *event,
                                         gpointer data)
{
    ph_main_window_filter_entry_activated(entry, data);
}

/* List and map event handlers {{{1 */

/*
 * The details for a geocache are requested from the map.
 */
static void
ph_main_window_map_geocache_activated(PHGeocacheMap *map,
                                      GtkTreePath *path,
                                      gpointer data)
{
    PHMainWindow *window = PH_MAIN_WINDOW(data);
    GtkTreeModel *model = GTK_TREE_MODEL(window->priv->geocache_list);
    GtkTreeIter iter;
    gchar *gc_id;
    gboolean found;
    gpointer old_page;

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, PH_GEOCACHE_LIST_COLUMN_ID, &gc_id, -1);

    found = g_hash_table_lookup_extended(window->priv->view_table,
            gc_id, NULL, &old_page);
    if (found)
        /* already opened: jump to page */
        gtk_notebook_set_current_page(GTK_NOTEBOOK(window->priv->view_notebook),
                GPOINTER_TO_INT(old_page));
    else {
        /* open new detail view page */
        PHDatabase *database = window->priv->database;
        GError *error = NULL;
        GtkWidget *view;

        view = ph_detail_view_new(database, gc_id, &error);
        if (view != NULL) {
            gint page;

            g_signal_connect(view, "closed",
                    G_CALLBACK(ph_main_window_close_tab), window);

            gtk_widget_show_all(view);
            page = gtk_notebook_append_page(
                    GTK_NOTEBOOK(window->priv->view_notebook),
                    view, ph_detail_view_get_label(PH_DETAIL_VIEW(view)));
            gtk_notebook_set_current_page(
                    GTK_NOTEBOOK(window->priv->view_notebook), page);
            g_hash_table_insert(window->priv->view_table,
                    g_strdup(gc_id), GINT_TO_POINTER(page));
        }
        else {
            GtkWidget *message = gtk_message_dialog_new(
                GTK_WINDOW(window), GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_CANCEL,
                _("Cannot retrieve the geocache from the database."));
            gtk_message_dialog_format_secondary_text(
                GTK_MESSAGE_DIALOG(message), "%s.", error->message);
            gtk_dialog_run(GTK_DIALOG(message));
            gtk_widget_destroy(message);
            g_error_free(error);
        }
    }
}

/*
 * The user has double-clicked or otherwise activated a row in the geocache
 * list.
 */
static void
ph_main_window_list_row_activated(GtkTreeView *tree_view,
                                  GtkTreePath *path,
                                  GtkTreeViewColumn *column,
                                  gpointer data)
{
    ph_main_window_map_geocache_activated(NULL, path, data);
}

/*
 * A geocache has been selected (or deselected) through the map.  Copy the
 * selection to the tree view.
 */
static void
ph_main_window_map_geocache_selected(PHGeocacheMap *map,
                                     GtkTreePath *path,
                                     gpointer data)
{
    PHMainWindow *window = PH_MAIN_WINDOW(data);
    GtkTreeView *tree_view = GTK_TREE_VIEW(window->priv->tree_view);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GList *rows = gtk_tree_selection_get_selected_rows(selection, NULL);
    gboolean changed;

    if (path == NULL && rows == NULL)
        /* no map selection and no list selection */
        changed = FALSE;
    else if (path != NULL && rows != NULL && rows->next == NULL)
        /* map selection and exactly one list selection */
        changed = (gtk_tree_path_compare(path,
                    (GtkTreePath *) rows->data) != 0);
    else
        /* no map selection and number of list selections != 1 */
        changed = TRUE;

    if (changed) {
        gtk_tree_selection_unselect_all(selection);
        if (path != NULL) {
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_view_scroll_to_cell(tree_view, path, NULL, TRUE, 0.5, 0);
        }
    }

    g_list_free_full(rows, (GDestroyNotify) gtk_tree_path_free);
}

/*
 * A geocache has been selected through the list.
 */
static void
ph_main_window_list_row_selected(GtkTreeSelection *sel,
                                 gpointer data)
{
    PHMainWindow *window = PH_MAIN_WINDOW(data);
    PHGeocacheMap *map = PH_GEOCACHE_MAP(window->priv->geocache_map);
    GList *rows = gtk_tree_selection_get_selected_rows(sel, NULL);

    if (rows != NULL) {
        /* do not propagate cleared selections:
         * the geocache map keeps the old selection in case it goes out of
         * sight, so it can reselect it if it comes back */
        ph_geocache_map_select(map, (GtkTreePath *) rows->data);
        g_list_free_full(rows, (GDestroyNotify) gtk_tree_path_free);
    }
}

/* Map provider change {{{1 */

/*
 * Communicate a provider change to the map.
 */
static void
ph_main_window_change_map_provider(PHMapProviderAction *action,
                                   GtkTreePath *path,
                                   gpointer data)
{
    PHMainWindow *window = PH_MAIN_WINDOW(data);
    PHMap *map = PH_MAP(window->priv->geocache_map);
    GtkListStore *map_providers = ph_config_get_map_providers();
    GtkTreeIter iter;
    PHMapProvider *provider;

    gtk_tree_model_get_iter(GTK_TREE_MODEL(map_providers), &iter, path);
    provider = ph_map_provider_new_from_list(map_providers, &iter);
    ph_map_set_provider(map, provider);
    ph_map_provider_free(provider);
}

/* Map zoom {{{1 */

/*
 * Zoom in or out around the center of the map.
 */
static void
ph_main_window_zoom(GtkAction *action,
                    gpointer data)
{
    PHMainWindow *window = PH_MAIN_WINDOW(data);
    GtkAdjustment *zoom;
    gdouble value;
    const gchar *action_name = gtk_action_get_name(action);

    zoom = ph_map_get_zoom_adjustment(PH_MAP(window->priv->geocache_map));
    value = gtk_adjustment_get_value(zoom);
    if (strcmp(action_name, "ZoomIn") == 0)
        value += 1;
    else if (strcmp(action_name, "ZoomOut") == 0)
        value -= 1;
    gtk_adjustment_set_value(zoom, value);
}

/* Main notebook {{{1 */

/*
 * The user has requested the deletion of a tab.
 */
static void
ph_main_window_close_tab(GtkWidget *widget,
                         gpointer data)
{
    PHMainWindow *window = PH_MAIN_WINDOW(data);
    gint num = gtk_notebook_page_num(GTK_NOTEBOOK(window->priv->view_notebook),
            widget);

    if (PH_IS_DETAIL_VIEW(widget))
        g_hash_table_remove(window->priv->view_table,
                ph_detail_view_get_geocache_id(PH_DETAIL_VIEW(widget)));

    gtk_notebook_remove_page(GTK_NOTEBOOK(window->priv->view_notebook), num);
}

/* Open database {{{1 */

/*
 * Let the user pick a database to open.
 */
static void
ph_main_window_choose_database(GtkAction *action,
                               gpointer data)
{
    PHMainWindow *window = PH_MAIN_WINDOW(data);
    GtkWidget *dialog;
    GtkFileFilter *filter;

    dialog = gtk_file_chooser_dialog_new(_("Open database"), GTK_WINDOW(window),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
            NULL);
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),
            ph_database_get_filename(window->priv->database));

    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.phdb");
    gtk_file_filter_set_name(filter, _("Plastichunt database files"));
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        ph_main_window_open_database(window, filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

/*
 * Open a recently used database again.
 */
static void
ph_main_window_open_recent_database(GtkRecentChooser *chooser,
                                    gpointer data)
{
    PHMainWindow *window = PH_MAIN_WINDOW(data);
    GtkRecentInfo *info = gtk_recent_chooser_get_current_item(chooser);
    gchar *path = g_filename_from_uri(gtk_recent_info_get_uri(info),
            NULL, NULL);

    ph_main_window_open_database(window, path);
    g_free(path);
    gtk_recent_info_unref(info);
}

/*
 * Open a database file.
 */
static void
ph_main_window_open_database(PHMainWindow *window,
                             const gchar *path)
{
    GError *error = NULL;
    PHDatabase *database;

    if (strcmp(path, ph_database_get_filename(window->priv->database)) != 0) {
        database = ph_database_new(path, TRUE, &error);
        if (database == NULL) {
            GtkWidget *message = gtk_message_dialog_new(
                GTK_WINDOW(window), GTK_DIALOG_MODAL,
                GTK_MESSAGE_WARNING, GTK_BUTTONS_CANCEL,
                _("Cannot open database file."));
            gtk_message_dialog_format_secondary_text(
                GTK_MESSAGE_DIALOG(message), "%s.", error->message);
            gtk_dialog_run(GTK_DIALOG(message));
            gtk_widget_destroy(message);
            g_error_free(error);
        }
        else {
            GtkRecentData recent_data;
            GtkRecentManager *recent = gtk_recent_manager_get_default();
            gchar *uri;

            ph_main_window_set_database(window, database);
            g_object_unref(database);

            memset(&recent_data, 0, sizeof(recent_data));
            recent_data.mime_type = PH_DATABASE_MIME_TYPE;
            recent_data.app_name = g_strdup(g_get_application_name());
            recent_data.app_exec = g_strdup_printf("%s -d %%f",
                    g_get_prgname());
            uri = g_filename_to_uri(path, NULL, NULL);
            gtk_recent_manager_add_full(recent, uri, &recent_data);
            g_free(uri);
            g_free(recent_data.app_name);
            g_free(recent_data.app_exec);
        }
    }
}

/*
 * Change to another database.
 */
static void
ph_main_window_set_database(PHMainWindow *window,
                            PHDatabase *database)
{
    GtkNotebook *notebook = GTK_NOTEBOOK(window->priv->view_notebook);
    gint i;

    if (window->priv->database != NULL)
        g_object_unref(window->priv->database);
    window->priv->database = g_object_ref(database);

    /* close all detail views */
    for (i = 0; i < gtk_notebook_get_n_pages(notebook);) {
        GtkWidget *widget = gtk_notebook_get_nth_page(notebook, i);
        if (PH_IS_DETAIL_VIEW(widget))
            gtk_notebook_remove_page(notebook, i);
        else
            ++i;
    }
    g_hash_table_remove_all(window->priv->view_table);

    ph_geocache_map_select(PH_GEOCACHE_MAP(window->priv->geocache_map), NULL);
    ph_geocache_list_set_database(window->priv->geocache_list, database);
    ph_geocache_map_set_list(PH_GEOCACHE_MAP(window->priv->geocache_map),
            window->priv->geocache_list);
}

/* Import files {{{1 */

/*
 * Let the user choose a file to import.
 */
static void
ph_main_window_import(GtkAction *action,
                      gpointer data)
{
    PHMainWindow *window = PH_MAIN_WINDOW(data);
    GtkWidget *dialog;

    dialog = ph_import_dialog_new(GTK_WINDOW(window), window->priv->database);
    ph_process_dialog_run(PH_PROCESS_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/* Preferences {{{1 */

/*
 * Show the configuration dialog.
 */
static void
ph_main_window_preferences(GtkAction *action,
                           gpointer data)
{
    GtkWidget *dialog;

    dialog = ph_config_dialog_new();
    (void) gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/* Cell data functions {{{1 */

/*
 * Put together the icon for the leftmost cell of the geocache list.
 */
static void
ph_main_window_image_data_func(GtkTreeViewColumn *col,
                               GtkCellRenderer *cr,
                               GtkTreeModel *model,
                               GtkTreeIter *iter,
                               gpointer data)
{
    PHGeocacheType type;
    gboolean found, logged, available, archived, note;

    gtk_tree_model_get(model, iter,
            PH_GEOCACHE_LIST_COLUMN_TYPE, &type,
            PH_GEOCACHE_LIST_COLUMN_FOUND, &found,
            PH_GEOCACHE_LIST_COLUMN_LOGGED, &logged,
            PH_GEOCACHE_LIST_COLUMN_AVAILABLE, &available,
            PH_GEOCACHE_LIST_COLUMN_ARCHIVED, &archived,
            PH_GEOCACHE_LIST_COLUMN_NOTE, &note,
            -1);
    g_object_set(cr, "value", ph_sprite_value_for_geocache_details(type,
                found, logged, available, archived, note), NULL);
}

/*
 * Same thing for the cache name.
 */
static void
ph_main_window_name_data_func(GtkTreeViewColumn *col,
                              GtkCellRenderer *cr,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gpointer data)
{
    gchar *id, *name, *owner;
    gchar *markup;

    gtk_tree_model_get(model, iter,
            PH_GEOCACHE_LIST_COLUMN_ID, &id,
            PH_GEOCACHE_LIST_COLUMN_NAME, &name,
            PH_GEOCACHE_LIST_COLUMN_OWNER, &owner,
            -1);
    markup = g_markup_printf_escaped(
            _("<span size=\"large\" weight=\"bold\">%s</span>\n"
                "<span size=\"small\">%s by %s</span>"),
            name, id, owner);
    g_object_set(cr, "markup", markup, NULL);
    g_free(markup);
}

/* Termination {{{1 */

/*
 * Close the main window when "Quit" is selected in the menu.
 */
static void
ph_main_window_quit(GtkAction *action,
                    gpointer data)
{
    GtkWidget *widget = GTK_WIDGET(data);
    gboolean quit = (ph_main_window_instances == 1);

    gtk_widget_destroy(widget);

    if (quit)
        /* last main window has been closed */
        gtk_main_quit();
}

/*
 * Same thing when the closing is requested via the window manager.
 */
static gboolean
ph_main_window_delete_event(GtkWidget *widget,
                            GdkEventAny *event)
{
    ph_main_window_quit(NULL, widget);

    return FALSE;
}

/* Public interface {{{1 */

GtkWidget *
ph_main_window_new(PHDatabase *database)
{
    GtkWidget *widget = GTK_WIDGET(g_object_new(PH_TYPE_MAIN_WINDOW,
                "database", database, NULL));
    PHMainWindow *window = PH_MAIN_WINDOW(widget);

    g_signal_emit_by_name(window->priv->filter_entry, "activate");

    return widget;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
