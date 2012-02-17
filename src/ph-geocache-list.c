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

#include "ph-geocache-list.h"
#include "ph-geo.h"
#include <math.h>
#include <string.h>
#include <sqlite3.h>

/* Signals {{{1 */

enum {
    PH_GEOCACHE_LIST_SIGNAL_UPDATED,
    PH_GEOCACHE_LIST_SIGNAL_COUNT
};

static guint ph_geocache_list_signals[PH_GEOCACHE_LIST_SIGNAL_COUNT];

/* Properties {{{1 */

enum {
    PH_GEOCACHE_LIST_PROP_0,
    PH_GEOCACHE_LIST_PROP_DATABASE
};

/* Private data {{{1 */

#define PH_GEOCACHE_LIST_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), PH_TYPE_GEOCACHE_LIST, \
                                 PHGeocacheListPrivate))

/*
 * Implementation details.
 */
struct _PHGeocacheListPrivate {
    gint stamp;

    PHDatabase *database;
    gulong db_signal_handlers[2];
    gchar *sql;

    PHGeocacheListRange loaded_range;
    GList *loaded_list;
    PHGeocacheListRange visible_range;
    GList *visible_list;
    gint visible_length;
};

/* Forward declarations {{{1 */

static void ph_geocache_list_class_init(PHGeocacheListClass *klass);
static void ph_geocache_list_tree_model_init(GtkTreeModelIface *iface);
static void ph_geocache_list_init(PHGeocacheList *list);
static void ph_geocache_list_dispose(GObject *obj);
static void ph_geocache_list_finalize(GObject *obj);
static void ph_geocache_list_set_property(
    GObject *obj, guint id, const GValue *value, GParamSpec *spec);
static void ph_geocache_list_get_property(
    GObject *obj, guint id, GValue *value, GParamSpec *spec);

static GtkTreeModelFlags ph_geocache_list_get_flags(GtkTreeModel *model);
static gint ph_geocache_list_get_n_columns(GtkTreeModel *model);
static GType ph_geocache_list_get_column_type(GtkTreeModel *model, gint n);
static gboolean ph_geocache_list_get_iter(GtkTreeModel *model,
                                          GtkTreeIter *iter, GtkTreePath *path);
static GtkTreePath *ph_geocache_list_get_path(GtkTreeModel *model,
                                              GtkTreeIter *iter);
static void ph_geocache_list_get_value(GtkTreeModel *model, GtkTreeIter *iter,
                                       gint column, GValue *value);
static gboolean ph_geocache_list_iter_next(GtkTreeModel *model,
                                           GtkTreeIter *iter);
static gboolean ph_geocache_list_iter_children(
    GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent);
static gboolean ph_geocache_list_iter_has_child(GtkTreeModel *model,
                                                GtkTreeIter *iter);
static gint ph_geocache_list_iter_n_children(GtkTreeModel *model,
                                             GtkTreeIter *iter);
static gboolean ph_geocache_list_iter_nth_child(
    GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent, gint n);
static gboolean ph_geocache_list_iter_parent(
    GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *child);

static PHGeocacheListEntry *ph_geocache_list_entry_new(sqlite3_stmt *stmt);
static void ph_geocache_list_entry_free(PHGeocacheListEntry *entry);

static void ph_geocache_list_insert_visible(
    PHGeocacheList *list, GList *before, gint pos, PHGeocacheListEntry *entry);
static void ph_geocache_list_update_visible(
    PHGeocacheList *list, GList *target, gint pos, PHGeocacheListEntry *entry);
static void ph_geocache_list_delete_visible(PHGeocacheList *list, GList *remove,
                                            gint pos);
static gint ph_geocache_list_entry_compare(const PHGeocacheListEntry *first,
                                           const PHGeocacheListEntry *second);
static gboolean ph_geocache_list_locate_entry(GList *list,
                                              const PHGeocacheListEntry *entry,
                                              GList **node, gint *pos);
static void ph_geocache_list_update_entry(PHGeocacheList *list,
                                          PHGeocacheListEntry *entry);
static void ph_geocache_list_delete_entry_by_id(PHGeocacheList *list,
                                                const gchar *id);

static gchar *ph_geocache_list_sql_from_query(const gchar *query,
                                              GError **error);
static gchar *ph_geocache_list_sql_constrain(PHGeocacheList *list,
                                             const gchar *geocache_id);

static void ph_geocache_list_run_query(PHGeocacheList *list, gboolean update);
static void ph_geocache_list_filter(PHGeocacheList *list);

static void ph_geocache_list_geocache_updated(PHDatabase *database, gchar *id,
                                              gpointer data);
static void ph_geocache_list_bulk_updated(PHDatabase *database, gpointer data);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE_WITH_CODE(PHGeocacheList, ph_geocache_list, G_TYPE_OBJECT,
        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
            ph_geocache_list_tree_model_init))

/*
 * Class initialization code.
 */
static void
ph_geocache_list_class_init(PHGeocacheListClass *cls)
{
    GObjectClass *g_obj_cls = G_OBJECT_CLASS(cls);

    g_obj_cls->dispose = ph_geocache_list_dispose;
    g_obj_cls->finalize = ph_geocache_list_finalize;
    g_obj_cls->set_property = ph_geocache_list_set_property;
    g_obj_cls->get_property = ph_geocache_list_get_property;

    ph_geocache_list_signals[PH_GEOCACHE_LIST_SIGNAL_UPDATED] =
        g_signal_new("updated", PH_TYPE_GEOCACHE_LIST,
                G_SIGNAL_RUN_FIRST,
                G_STRUCT_OFFSET(PHGeocacheListClass, updated),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

    g_object_class_install_property(g_obj_cls,
            PH_GEOCACHE_LIST_PROP_DATABASE,
            g_param_spec_object("database", "database",
                "database to load from",
                PH_TYPE_DATABASE,
                G_PARAM_READWRITE));

    g_type_class_add_private(cls, sizeof(PHGeocacheListPrivate));
}

/*
 * Implement the GtkTreeModel interface.
 */
static void
ph_geocache_list_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = ph_geocache_list_get_flags;
    iface->get_n_columns = ph_geocache_list_get_n_columns;
    iface->get_column_type = ph_geocache_list_get_column_type;
    iface->get_iter = ph_geocache_list_get_iter;
    iface->get_path = ph_geocache_list_get_path;
    iface->get_value = ph_geocache_list_get_value;
    iface->iter_next = ph_geocache_list_iter_next;
    iface->iter_children = ph_geocache_list_iter_children;
    iface->iter_has_child = ph_geocache_list_iter_has_child;
    iface->iter_n_children = ph_geocache_list_iter_n_children;
    iface->iter_nth_child = ph_geocache_list_iter_nth_child;
    iface->iter_parent = ph_geocache_list_iter_parent;
}

/*
 * Instance initialization code.
 */
static void
ph_geocache_list_init(PHGeocacheList *list)
{
    PHGeocacheListPrivate *priv = PH_GEOCACHE_LIST_GET_PRIVATE(list);

    list->priv = priv;
    priv->stamp = g_random_int();

    /* create tail elements for the linked lists */
    priv->loaded_list = g_list_append(priv->loaded_list, NULL);
    priv->visible_list = g_list_append(priv->visible_list, NULL);
}

/*
 * Drop references to other objects.
 */
static void
ph_geocache_list_dispose(GObject *obj)
{
    PHGeocacheList *list = PH_GEOCACHE_LIST(obj);

    if (list->priv->database != NULL) {
        guint i;
        for (i = 0; i < G_N_ELEMENTS(list->priv->db_signal_handlers); ++i)
            g_signal_handler_disconnect(list->priv->database,
                    list->priv->db_signal_handlers[i]);
        g_object_unref(list->priv->database);
        list->priv->database = NULL;
    }

    if (G_OBJECT_CLASS(ph_geocache_list_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(ph_geocache_list_parent_class)->dispose(obj);
}

/*
 * Instance destruction code.
 */
static void
ph_geocache_list_finalize(GObject *obj)
{
    PHGeocacheList *list = PH_GEOCACHE_LIST(obj);

    if (list->priv->visible_list != NULL)
        g_list_free(list->priv->visible_list);
    if (list->priv->loaded_list != NULL)
        g_list_free_full(list->priv->loaded_list,
                (GDestroyNotify) ph_geocache_list_entry_free);
    if (list->priv->sql != NULL)
        g_free(list->priv->sql);

    if (G_OBJECT_CLASS(ph_geocache_list_parent_class)->finalize != NULL)
        G_OBJECT_CLASS(ph_geocache_list_parent_class)->finalize(obj);
}

/*
 * Set a property to a new value.
 */
static void
ph_geocache_list_set_property(GObject *obj,
                              guint id,
                              const GValue *value,
                              GParamSpec *spec)
{
    PHGeocacheList *list = PH_GEOCACHE_LIST(obj);

    switch (id) {
    case PH_GEOCACHE_LIST_PROP_DATABASE:
        ph_geocache_list_set_database(list, g_value_get_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
    }
}

/*
 * Retrieve the current value of a property.
 */
static void
ph_geocache_list_get_property(GObject *obj,
                              guint id,
                              GValue *value,
                              GParamSpec *spec)
{
    PHGeocacheList *list = PH_GEOCACHE_LIST(obj);

    switch (id) {
    case PH_GEOCACHE_LIST_PROP_DATABASE:
        g_value_set_object(value, list->priv->database);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
    }
}

/* GtkTreeModel interface {{{1 */

/*
 * Geocache lists are always flat.
 */
static GtkTreeModelFlags
ph_geocache_list_get_flags(GtkTreeModel *model)
{
    return GTK_TREE_MODEL_LIST_ONLY;
}

/*
 * Get the number of columns.
 */
static gint
ph_geocache_list_get_n_columns(GtkTreeModel *model)
{
    return PH_GEOCACHE_LIST_COLUMN_COUNT;
}

/*
 * Get the data type of the specified column.
 */
static GType
ph_geocache_list_get_column_type(GtkTreeModel *model,
                                 gint n)
{
    switch (n) {
    case PH_GEOCACHE_LIST_COLUMN_ID:
    case PH_GEOCACHE_LIST_COLUMN_NAME:
    case PH_GEOCACHE_LIST_COLUMN_OWNER:
        return G_TYPE_STRING;
    case PH_GEOCACHE_LIST_COLUMN_TYPE:
    case PH_GEOCACHE_LIST_COLUMN_SIZE:
    case PH_GEOCACHE_LIST_COLUMN_DIFFICULTY:
    case PH_GEOCACHE_LIST_COLUMN_TERRAIN:
        return G_TYPE_UINT;
    case PH_GEOCACHE_LIST_COLUMN_LOGGED:
    case PH_GEOCACHE_LIST_COLUMN_AVAILABLE:
    case PH_GEOCACHE_LIST_COLUMN_ARCHIVED:
    case PH_GEOCACHE_LIST_COLUMN_NOTE:
    case PH_GEOCACHE_LIST_COLUMN_FOUND:
    case PH_GEOCACHE_LIST_COLUMN_NEW_COORDINATES:
        return G_TYPE_BOOLEAN;
    case PH_GEOCACHE_LIST_COLUMN_LATITUDE:
    case PH_GEOCACHE_LIST_COLUMN_LONGITUDE:
    case PH_GEOCACHE_LIST_COLUMN_NEW_LATITUDE:
    case PH_GEOCACHE_LIST_COLUMN_NEW_LONGITUDE:
        return G_TYPE_INT;
    default:
        g_return_val_if_reached(G_TYPE_INVALID);
    }
}

/*
 * Check an iterator for validity.
 */
#define PH_GEOCACHE_LIST_ITER_VALID(model, iter) \
    (((iter)->stamp == PH_GEOCACHE_LIST(model)->priv->stamp) && \
     ((iter)->user_data == (model)) && \
     ((iter)->user_data2 != NULL))

/*
 * Make the given iterator point to the list node specified by path.
 */
static gboolean
ph_geocache_list_get_iter(GtkTreeModel *model,
                          GtkTreeIter *iter,
                          GtkTreePath *path)
{
    PHGeocacheList *list = PH_GEOCACHE_LIST(model);
    GList *nth = NULL;
    gint *indices, depth;

    indices = gtk_tree_path_get_indices_with_depth(path, &depth);

    if (depth > 0 && indices[0] < list->priv->visible_length)
        nth = g_list_nth(list->priv->visible_list, indices[0]);

    if (nth != NULL && nth->data != NULL) {
        iter->stamp = list->priv->stamp;
        iter->user_data = model;
        iter->user_data2 = nth;
        return TRUE;
    }
    else
        return FALSE;
}

/*
 * Determine the location of the given iterator as a GtkTreePath.
 */
static GtkTreePath *
ph_geocache_list_get_path(GtkTreeModel *model,
                          GtkTreeIter *iter)
{
    PHGeocacheList *list = PH_GEOCACHE_LIST(model);
    gint i;

    g_return_val_if_fail(PH_GEOCACHE_LIST_ITER_VALID(model, iter), NULL);

    i = g_list_index(list->priv->visible_list,
            ((GList *) iter->user_data2)->data);
    if (i < 0)
        return NULL;
    else
        return gtk_tree_path_new_from_indices(i, -1);
}

/*
 * Get the content of a column at the position referred to by the iterator.
 */
static void
ph_geocache_list_get_value(GtkTreeModel *model,
                           GtkTreeIter *iter,
                           gint column,
                           GValue *value)
{
    PHGeocacheListEntry *entry;

    g_return_if_fail(PH_GEOCACHE_LIST_ITER_VALID(model, iter));

    entry = (PHGeocacheListEntry *) ((GList *) iter->user_data2)->data;

    g_value_init(value, ph_geocache_list_get_column_type(model, column));

    switch (column) {
    case PH_GEOCACHE_LIST_COLUMN_ID:
        g_value_set_string(value, entry->id);
        break;
    case PH_GEOCACHE_LIST_COLUMN_NAME:
        g_value_set_string(value, entry->name);
        break;
    case PH_GEOCACHE_LIST_COLUMN_OWNER:
        g_value_set_string(value, entry->owner);
        break;
    case PH_GEOCACHE_LIST_COLUMN_TYPE:
        g_value_set_uint(value, entry->type);
        break;
    case PH_GEOCACHE_LIST_COLUMN_SIZE:
        g_value_set_uint(value, entry->size);
        break;
    case PH_GEOCACHE_LIST_COLUMN_DIFFICULTY:
        g_value_set_uint(value, entry->difficulty);
        break;
    case PH_GEOCACHE_LIST_COLUMN_TERRAIN:
        g_value_set_uint(value, entry->terrain);
        break;
    case PH_GEOCACHE_LIST_COLUMN_LOGGED:
        g_value_set_boolean(value, entry->logged);
        break;
    case PH_GEOCACHE_LIST_COLUMN_AVAILABLE:
        g_value_set_boolean(value, entry->available);
        break;
    case PH_GEOCACHE_LIST_COLUMN_ARCHIVED:
        g_value_set_boolean(value, entry->archived);
        break;
    case PH_GEOCACHE_LIST_COLUMN_LATITUDE:
        g_value_set_int(value, entry->latitude);
        break;
    case PH_GEOCACHE_LIST_COLUMN_LONGITUDE:
        g_value_set_int(value, entry->longitude);
        break;
    case PH_GEOCACHE_LIST_COLUMN_FOUND:
        g_value_set_boolean(value, entry->found);
        break;
    case PH_GEOCACHE_LIST_COLUMN_NOTE:
        g_value_set_boolean(value, entry->note);
        break;
    case PH_GEOCACHE_LIST_COLUMN_NEW_COORDINATES:
        g_value_set_boolean(value, entry->new_coordinates);
        break;
    case PH_GEOCACHE_LIST_COLUMN_NEW_LATITUDE:
        g_value_set_int(value, entry->new_latitude);
        break;
    case PH_GEOCACHE_LIST_COLUMN_NEW_LONGITUDE:
        g_value_set_int(value, entry->new_longitude);
        break;
    default:
        g_return_if_reached();
    }
}

/*
 * Move the iterator forward through the list.
 */
static gboolean
ph_geocache_list_iter_next(GtkTreeModel *model,
                           GtkTreeIter *iter)
{
    GList *next;

    g_return_val_if_fail(PH_GEOCACHE_LIST_ITER_VALID(model, iter), FALSE);

    next = ((GList *) iter->user_data2)->next;
    if (next != NULL && next->data != NULL) {
        iter->user_data2 = next;
        return TRUE;
    }
    else {
        iter->user_data2 = NULL;
        return FALSE;
    }
}

/*
 * Move the iterator to the first child of the given parent iterator.  As
 * geocache lists are flat, this is only implemented for the NULL iterator
 * which points to an imaginary root element.
 */
static gboolean
ph_geocache_list_iter_children(GtkTreeModel *model,
                               GtkTreeIter *iter,
                               GtkTreeIter *parent)
{
    PHGeocacheList *list = PH_GEOCACHE_LIST(model);

    if (parent != NULL || list->priv->visible_list == NULL ||
            list->priv->visible_list->data == NULL) {
        iter->user_data2 = NULL;
        return FALSE;
    }
    else {
        iter->stamp = list->priv->stamp;
        iter->user_data = model;
        iter->user_data2 = list->priv->visible_list;
        return TRUE;
    }
}

/*
 * Find out if the node the iterator points to has any children.  This returns
 * true if and only if iter == NULL.
 */
static gboolean
ph_geocache_list_iter_has_child(GtkTreeModel *model,
                                GtkTreeIter *iter)
{
    return (iter == NULL);
}

/*
 * Count the children of an iterator.  For iter == NULL, this returns the
 * length of the list; in all other cases, the result is 0.
 */
static gint
ph_geocache_list_iter_n_children(GtkTreeModel *model,
                                 GtkTreeIter *iter)
{
    if (iter == NULL)
        return 0;
    else
        return PH_GEOCACHE_LIST(model)->priv->visible_length;
}

/*
 * Move iter to the n-th child of parent.  Again, this only works for parent
 * == NULL, because all other nodes do not have children.
 */
static gboolean
ph_geocache_list_iter_nth_child(GtkTreeModel *model,
                                GtkTreeIter *iter,
                                GtkTreeIter *parent,
                                gint n)
{
    PHGeocacheList *list = PH_GEOCACHE_LIST(model);

    if (parent != NULL || n >= list->priv->visible_length) {
        iter->user_data2 = NULL;
        return FALSE;
    }
    else {
        iter->stamp = list->priv->stamp;
        iter->user_data = model;
        iter->user_data2 = g_list_nth(list->priv->visible_list, n);
        return TRUE;
    }
}

/*
 * Move iter to child's parent node.  Simply invalidates the iterator.
 */
static gboolean
ph_geocache_list_iter_parent(GtkTreeModel *model,
                             GtkTreeIter *iter,
                             GtkTreeIter *child)
{
    iter->user_data2 = NULL;
    return FALSE;
}

/* Entry creation and destruction {{{1 */

static PHGeocacheListEntry *
ph_geocache_list_entry_new(sqlite3_stmt *stmt)
{
    PHGeocacheListEntry *result = g_slice_new(PHGeocacheListEntry);

    result->id = g_strdup((const gchar *) sqlite3_column_text(stmt, 0));
    result->name = g_strdup((const gchar *) sqlite3_column_text(stmt, 1));
    result->owner = g_strdup((const gchar *) sqlite3_column_text(stmt, 2));
    result->type = (PHGeocacheType) sqlite3_column_int(stmt, 3);
    result->size = (PHGeocacheSize) sqlite3_column_int(stmt, 4);
    result->difficulty = (guint8) sqlite3_column_int(stmt, 5);
    result->terrain = (guint8) sqlite3_column_int(stmt, 6);
    result->logged = (sqlite3_column_int(stmt, 7) != 0);
    result->available = (sqlite3_column_int(stmt, 8) != 0);
    result->archived = (sqlite3_column_int(stmt, 9) != 0);
    result->latitude = sqlite3_column_int(stmt, 10);
    result->longitude = sqlite3_column_int(stmt, 11);
    result->found = (sqlite3_column_int(stmt, 12) != 0);
    result->note = (sqlite3_column_int(stmt, 13) != 0);
    result->new_coordinates = (sqlite3_column_type(stmt, 14) != SQLITE_NULL);
    if (result->new_coordinates) {
        result->new_latitude = sqlite3_column_int(stmt, 14);
        result->new_longitude = sqlite3_column_int(stmt, 15);
    }
    else {
        result->new_latitude = result->latitude;
        result->new_longitude = result->longitude;
    }

    return result;
}

/*
 * Free a PHGeocacheListEntry using the slice allocator.
 */
static void
ph_geocache_list_entry_free(PHGeocacheListEntry *entry)
{
    if (entry == NULL)
        return;

    g_free(entry->id);
    g_free(entry->name);
    g_free(entry->owner);
    g_slice_free(PHGeocacheListEntry, entry);
}

/* List manipulation {{{1 */

/*
 * Handle the insertion of a geocache entry into the visible list, including
 * signal emission.
 */
static void
ph_geocache_list_insert_visible(PHGeocacheList *list,
                                GList *before,
                                gint pos,
                                PHGeocacheListEntry *entry)
{
    GtkTreeIter iter;
    GtkTreePath *path;

    list->priv->visible_list = g_list_insert_before(list->priv->visible_list,
            before, entry);

    iter.stamp = list->priv->stamp;
    iter.user_data = list;
    iter.user_data2 = before->prev;     /* inserted node */

    path = gtk_tree_path_new_from_indices(pos, -1);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(list), path, &iter);
    gtk_tree_path_free(path);
}

/*
 * Handle the update of a geocache entry in the visible list.  This does not
 * free the old entry.
 */
static void
ph_geocache_list_update_visible(PHGeocacheList *list,
                                GList *target,
                                gint pos,
                                PHGeocacheListEntry *entry)
{
    GtkTreeIter iter;
    GtkTreePath *path;

    target->data = entry;

    iter.stamp = list->priv->stamp;
    iter.user_data = list;
    iter.user_data2 = target;           /* updated node */

    path = gtk_tree_path_new_from_indices(pos, -1);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(list), path, &iter);
    gtk_tree_path_free(path);
}

/*
 * Handle the removal of a geocache entry, including signal emission.
 */
static void
ph_geocache_list_delete_visible(PHGeocacheList *list,
                                GList *remove,
                                gint pos)
{
    GtkTreePath *path;

    list->priv->visible_list = g_list_delete_link(list->priv->visible_list,
            remove);

    path = gtk_tree_path_new_from_indices(pos, -1);
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(list), path);
    gtk_tree_path_free(path);
}

/*
 * Compare two list entries.
 */
static gint
ph_geocache_list_entry_compare(const PHGeocacheListEntry *first,
                               const PHGeocacheListEntry *second)
{
    int cmp = strcmp(first->name, second->name);
    if (cmp == 0)
        cmp = strcmp(first->id, second->id);

    return cmp;
}

/*
 * Check if a list entry is within the given bounds.
 */
#define PH_GEOCACHE_LIST_ENTRY_WITHIN(entry, range) \
    (((entry).new_latitude >= (range).south) && \
     ((entry).new_latitude <= (range).north) && \
     ((entry).new_longitude >= (range).west) && \
     ((entry).new_longitude <= (range).east))

/*
 * Tries to find the geocache list entry by name and ID.  If the retrieval
 * succeeds, node will be set to the match and the function returns TRUE.
 * Otherwise, node is the first list node sorted after the given entry, i.e.,
 * the point where it can be inserted; the return value is FALSE in this case.
 *
 * If pos is not NULL, its target will be set to the index of the returned
 * node.
 */
static gboolean
ph_geocache_list_locate_entry(GList *list,
                              const PHGeocacheListEntry *entry,
                              GList **node,
                              gint *pos)
{
    GList *current = list;
    gboolean found = FALSE;
    gint i = 0;

    while (current != NULL && current->data != NULL) {
        int cmp = ph_geocache_list_entry_compare(entry,
                (PHGeocacheListEntry *) current->data);
        if (cmp <= 0) {
            found = (cmp == 0);
            break;
        }
        current = current->next;
        ++i;
    }

    if (node != NULL)
        *node = current;
    if (pos != NULL)
        *pos = i;

    return found;
}

/*
 * Update a list entry.  This can cause its update, insertion or deletion,
 * depending on whether it is currently present and is now in the loaded
 * range.  The visible list is updated as well, including emission of the
 * adequate signal.
 *
 * The entry will be inserted into the list and must not be freed by the
 * caller.
 */
static void
ph_geocache_list_update_entry(PHGeocacheList *list,
                              PHGeocacheListEntry *entry)
{
    GList *loaded_node, *visible_node;
    gboolean loaded_match, visible_match;
    gint pos;
    PHGeocacheListEntry *old_entry = NULL;
    gboolean in_loaded, in_visible;

    in_loaded = PH_GEOCACHE_LIST_ENTRY_WITHIN(*entry,
            list->priv->loaded_range);
    in_visible = PH_GEOCACHE_LIST_ENTRY_WITHIN(*entry,
            list->priv->visible_range);

    /* find the current entry (or the insertion point) in both lists */
    loaded_match = ph_geocache_list_locate_entry(list->priv->loaded_list,
            entry, &loaded_node, NULL);
    visible_match = ph_geocache_list_locate_entry(list->priv->visible_list,
            entry, &visible_node, &pos);

    /* update loaded list */
    if (in_loaded) {
        if (loaded_match) {
            /* update */
            old_entry = (PHGeocacheListEntry *) loaded_node->data;
            loaded_node->data = entry;
        }
        else {
            /* insert */
            list->priv->loaded_list = g_list_insert_before(
                    list->priv->loaded_list, loaded_node, entry);
        }
    }
    else if (loaded_match)
        /* delete */
        list->priv->loaded_list = g_list_delete_link(
                list->priv->loaded_list, loaded_node);

    /* update visible list */
    if (in_visible) {
        if (visible_match)
            /* update */
            ph_geocache_list_update_visible(list, visible_node, pos, entry);
        else
            /* insert */
            ph_geocache_list_insert_visible(list, visible_node, pos, entry);
    }
    else if (visible_match)
        /* delete */
        ph_geocache_list_delete_visible(list, visible_node, pos);

    if (old_entry != NULL)
        ph_geocache_list_entry_free(old_entry);
}

/*
 * Remove a geocache from both the loaded and visible lists.  This function
 * can be used if only the ID is known.
 */
static void
ph_geocache_list_delete_entry_by_id(PHGeocacheList *list,
                                    const gchar *id)
{
    GList *current = list->priv->loaded_list;
    gboolean match;

    /* locate the entry in the loaded list */
    while (current != NULL && current->data != NULL) {
        if (strcmp(id, ((PHGeocacheListEntry *) current->data)->id) == 0) {
            match = TRUE;
            break;
        }
        current = current->next;
    }

    if (match) {
        PHGeocacheListEntry *old_entry = (PHGeocacheListEntry *) current->data;
        gint pos = 0;

        list->priv->loaded_list = g_list_delete_link(list->priv->loaded_list,
                current);

        /* locate the entry in the visible list */
        match = FALSE;
        current = list->priv->visible_list;
        while (current != NULL && current->data != NULL) {
            if (strcmp(id, ((PHGeocacheListEntry *) current->data)->id) == 0) {
                match = TRUE;
                break;
            }
            current = current->next;
            ++pos;
        }

        if (match)
            ph_geocache_list_delete_visible(list, current, pos);

        ph_geocache_list_entry_free(old_entry);
    }
}

/* SQL statements {{{1 */

/*
 * Create an SQL statement which retrieves the relevant information from the
 * database, with a WHERE clause according to the given query.
 */
static gchar *
ph_geocache_list_sql_from_query(const gchar *query,
                                GError **error)
{
    return ph_query_compile(query,
            PH_DATABASE_TABLE_WAYPOINTS |
                PH_DATABASE_TABLE_GEOCACHE_NOTES |
                PH_DATABASE_TABLE_WAYPOINT_NOTES,
            "geocaches.id, geocaches.name, geocaches.owner, "
            "geocaches.type, geocaches.size, "
            "geocaches.difficulty, geocaches.terrain, "
            "geocaches.logged, geocaches.available, geocaches.archived, "
            "waypoints.latitude, waypoints.longitude, "
            "geocache_notes.found, geocache_notes.note IS NOT NULL, "
            "waypoint_notes.new_latitude, waypoint_notes.new_longitude",
            error);
}

static gchar *
ph_geocache_list_sql_constrain(PHGeocacheList *list,
                               const gchar *geocache_id)
{
    GString *result = g_string_new(list->priv->sql);

    g_string_append_printf(result, " "
            "AND (COALESCE(waypoint_notes.new_latitude, waypoints.latitude) "
            "BETWEEN %d AND %d) "
            "AND (COALESCE(waypoint_notes.new_longitude, waypoints.longitude) "
            "BETWEEN %d AND %d) ",
            list->priv->loaded_range.south, list->priv->loaded_range.north,
            list->priv->loaded_range.west, list->priv->loaded_range.east);

    if (geocache_id != NULL) {
        char *quoted = sqlite3_mprintf("%Q", geocache_id);
        g_string_append_printf(result, "AND geocaches.id = %s ", quoted);
        sqlite3_free(quoted);
    }

    g_string_append(result, "ORDER BY geocaches.name ASC, geocaches.id ASC");

    return g_string_free(result, FALSE);
}

/* Load from the database {{{1 */

/*
 * Create the loaded list from the database.
 */
static void
ph_geocache_list_run_query(PHGeocacheList *list,
                           gboolean update)
{
    gchar *sql;
    sqlite3_stmt *stmt;
    gint status;
    GList *visible_cur, *loaded_cur;
    gint pos;
    gboolean changed = FALSE;

    sql = ph_geocache_list_sql_constrain(list, NULL);
    stmt = ph_database_prepare(list->priv->database, sql, NULL);
    g_free(sql);

    if (stmt == NULL)
        return;

    status = ph_database_step(list->priv->database, stmt, NULL);
    loaded_cur = list->priv->loaded_list;
    visible_cur = list->priv->visible_list;
    pos = 0;

    while (status == SQLITE_ROW ||
            visible_cur->data != NULL ||
            loaded_cur->data != NULL) {
        PHGeocacheListEntry *visible_entry, *loaded_entry;
        gint cmp;
        const gchar *new_id, *new_name;

        loaded_entry = (PHGeocacheListEntry *) loaded_cur->data;
        visible_entry = (PHGeocacheListEntry *) visible_cur->data;

        if (status == SQLITE_ROW) {
            new_id = (const gchar *) sqlite3_column_text(stmt, 0);
            new_name = (const gchar *) sqlite3_column_text(stmt, 1);
        }

        if (loaded_entry == NULL)
            cmp = 1;
        else if (status != SQLITE_ROW)
            cmp = -1;
        else {
            cmp = strcmp(loaded_entry->name, new_name);
            if (cmp == 0)
                cmp = strcmp(loaded_entry->id, new_id);
        }

        if (cmp < 0) {
            /* loaded entry < database entry:
             * loaded entry not in result set any more, remove */
            GList *next;

            if (visible_entry != NULL) {
                gint visible_cmp;
                if (visible_entry == loaded_entry)
                    visible_cmp = -1;
                else if (status != SQLITE_ROW)
                    visible_cmp = -1;
                else {
                    visible_cmp = strcmp(visible_entry->name, new_name);
                    if (visible_cmp == 0)
                        visible_cmp = strcmp(visible_entry->id, new_id);
                }

                if (visible_cmp < 0) {
                    /* visible entry < database entry */
                    next = visible_cur->next;
                    ph_geocache_list_delete_visible(list, visible_cur, pos);
                    visible_cur = next;
                    changed = TRUE;
                }
            }

            next = loaded_cur->next;
            ph_geocache_list_entry_free(loaded_entry);
            list->priv->loaded_list = g_list_delete_link(
                    list->priv->loaded_list, loaded_cur);
            loaded_cur = next;
        }

        else if (cmp == 0) {
            /* loaded entry = database entry:
             * if necessary, update */
            if (update) {
                PHGeocacheListEntry *new_entry;

                new_entry = ph_geocache_list_entry_new(stmt);
                loaded_cur->data = new_entry;
                if (visible_entry == loaded_entry)
                    ph_geocache_list_update_visible(list, visible_cur,
                            pos, new_entry);
                ph_geocache_list_entry_free(loaded_entry);
            }

            /* then walk forward in database and loaded list */
            if (visible_entry == loaded_entry) {
                visible_cur = visible_cur->next;
                ++pos;
            }
            loaded_cur = loaded_cur->next;
            status = ph_database_step(list->priv->database, stmt, NULL);
        }

        else if (cmp > 0) {
            /* loaded entry > database entry:
             * new item found in result set, insert before current entry */
            PHGeocacheListEntry *new_entry = ph_geocache_list_entry_new(stmt);

            list->priv->loaded_list = g_list_insert_before(
                    list->priv->loaded_list, loaded_cur, new_entry);

            if (PH_GEOCACHE_LIST_ENTRY_WITHIN(
                        *new_entry, list->priv->visible_range)) {
                ph_geocache_list_insert_visible(list, visible_cur, pos,
                        new_entry);
                changed = TRUE;
                ++pos;
            }

            status = ph_database_step(list->priv->database, stmt, NULL);
        }
    }

    (void) sqlite3_finalize(stmt);

    list->priv->visible_length = pos;

    if (changed)
        g_signal_emit(list,
                ph_geocache_list_signals[PH_GEOCACHE_LIST_SIGNAL_UPDATED],
                0);
}

/* Range update without query {{{1 */

/*
 * Update the visible list from the loaded list.
 */
static void
ph_geocache_list_filter(PHGeocacheList *list)
{
    GList *loaded_cur = list->priv->loaded_list;
    GList *visible_cur = list->priv->visible_list;
    gint pos = 0;
    gboolean changed = FALSE;

    while (visible_cur->data != NULL || loaded_cur->data != NULL) {
        PHGeocacheListEntry *loaded_entry, *visible_entry;
        gint cmp;

        loaded_entry = (PHGeocacheListEntry *) loaded_cur->data;
        visible_entry = (PHGeocacheListEntry *) visible_cur->data;

        if (loaded_entry != NULL && !PH_GEOCACHE_LIST_ENTRY_WITHIN(
                    *loaded_entry, list->priv->visible_range)) {
            /* not in visible range, skip */
            loaded_cur = loaded_cur->next;
            continue;
        }

        if (visible_entry == NULL)
            cmp = 1;
        else if (loaded_entry == NULL)
            cmp = -1;
        else
            cmp = ph_geocache_list_entry_compare(visible_entry, loaded_entry);

        if (cmp == 0) {
            /* same item in loaded and visible list, step both */
            loaded_cur = loaded_cur->next;
            visible_cur = visible_cur->next;
            ++pos;
        }
        else if (cmp < 0) {
            /* element in visible list has to be removed */
            GList *next = visible_cur->next;
            ph_geocache_list_delete_visible(list, visible_cur, pos);
            visible_cur = next;
            changed = TRUE;
        }
        else if (cmp > 0) {
            /* insert entry from loaded list into the visible list */
            ph_geocache_list_insert_visible(list, visible_cur, pos,
                    (PHGeocacheListEntry *) loaded_cur->data);
            loaded_cur = loaded_cur->next;
            ++pos;
            changed = TRUE;
        }
    }

    list->priv->visible_length = pos;

    if (changed)
        g_signal_emit(list,
                ph_geocache_list_signals[PH_GEOCACHE_LIST_SIGNAL_UPDATED],
                0);
}


/*
 * Create a new geocache list.
 */
PHGeocacheList *
ph_geocache_list_new()
{
    return g_object_new(PH_TYPE_GEOCACHE_LIST, NULL);
}

/* Database signal handlers {{{1 */

/*
 * Handle the storage of a single geocache.
 */
static void
ph_geocache_list_geocache_updated(PHDatabase *database,
                                  gchar *id,
                                  gpointer data)
{
    PHGeocacheList *list = PH_GEOCACHE_LIST(data);
    gchar *sql;
    sqlite3_stmt *stmt;
    gint status;

    sql = ph_geocache_list_sql_constrain(list, id);
    stmt = ph_database_prepare(list->priv->database, sql, NULL);
    g_free(sql);

    if (stmt == NULL)
        return;

    status = ph_database_step(list->priv->database, stmt, NULL);

    if (status == SQLITE_ROW) {
        PHGeocacheListEntry *entry = ph_geocache_list_entry_new(stmt);
        ph_geocache_list_update_entry(list, entry);
    }
    else if (status == SQLITE_DONE) {
        ph_geocache_list_delete_entry_by_id(list, id);
    }

    (void) sqlite3_finalize(stmt);

    g_signal_emit(list,
            ph_geocache_list_signals[PH_GEOCACHE_LIST_SIGNAL_UPDATED],
            0);
}

/*
 * Handle a large update of the database, for instance after a GPX file has been
 * imported.
 */
static void
ph_geocache_list_bulk_updated(PHDatabase *database,
                              gpointer data)
{
    PHGeocacheList *list = PH_GEOCACHE_LIST(data);

    if (list->priv->sql != NULL)
        ph_geocache_list_run_query(list, TRUE);
}

/* Public interface {{{1 */

/*
 * Set the database used to run queries.
 */
void
ph_geocache_list_set_database(PHGeocacheList *list,
                              PHDatabase *database)
{
    g_return_if_fail(list != NULL && PH_IS_GEOCACHE_LIST(list));
    g_return_if_fail(database != NULL);

    if (list->priv->database != NULL) {
        guint i;
        for (i = 0; i < G_N_ELEMENTS(list->priv->db_signal_handlers); ++i)
            g_signal_handler_disconnect(list->priv->database,
                    list->priv->db_signal_handlers[i]);
        g_object_unref(list->priv->database);
    }

    list->priv->database = g_object_ref(database);
    list->priv->db_signal_handlers[0] =
        g_signal_connect(list->priv->database, "geocache-updated",
            G_CALLBACK(ph_geocache_list_geocache_updated), list);
    list->priv->db_signal_handlers[1] =
        g_signal_connect(list->priv->database, "bulk-updated",
            G_CALLBACK(ph_geocache_list_bulk_updated), list);

    if (list->priv->sql != NULL)
        ph_geocache_list_run_query(list, TRUE);
}

/*
 * Retrieve the underlying database.  The reference count on the returned object
 * does not get increased.
 */
PHDatabase *
ph_geocache_list_get_database(PHGeocacheList *list)
{
    g_return_val_if_fail(list != NULL && PH_IS_GEOCACHE_LIST(list), NULL);

    return list->priv->database;
}

/*
 * How far to extend the range in each cardinal direction (in multiples of the
 * actual visible range).
 */
#define PH_GEOCACHE_LIST_RANGE_EXT 2

/*
 * Set the area in which to look for geocache waypoints.  PHGeocacheList keeps
 * a result set from a larger area in memory so that it doesn't have to access
 * the file system for every tiny amount of scrolling; this function will
 * either draw from this internal list or run a database query.
 */
void
ph_geocache_list_set_range(PHGeocacheList *list,
                           const PHGeocacheListRange *range)
{
    g_return_if_fail(list != NULL && PH_IS_GEOCACHE_LIST(list));
    g_return_if_fail(range != NULL);
    g_return_if_fail(range->north >= range->south &&
            range->east >= range->west);

    memcpy(&list->priv->visible_range, range, sizeof(PHGeocacheListRange));

    /* check if the new range exceeds the loaded range */
    if (range->south < list->priv->loaded_range.south ||
            range->north > list->priv->loaded_range.north ||
            range->west < list->priv->loaded_range.west ||
            range->east > list->priv->loaded_range.east) {
        gint latext = PH_GEOCACHE_LIST_RANGE_EXT *
            (range->north - range->south);
        gint lonext = PH_GEOCACHE_LIST_RANGE_EXT *
            (range->east - range->west);

        list->priv->loaded_range.north =
            PH_GEO_CLAMP_LATITUDE_MINFRAC(range->north + latext);
        list->priv->loaded_range.south =
            PH_GEO_CLAMP_LATITUDE_MINFRAC(range->south - latext);
        list->priv->loaded_range.east =
            PH_GEO_CLAMP_LONGITUDE_MINFRAC(range->east + lonext);
        list->priv->loaded_range.west =
            PH_GEO_CLAMP_LONGITUDE_MINFRAC(range->west - lonext);

        if (list->priv->sql != NULL)
            ph_geocache_list_run_query(list, FALSE);
    }
    else
        ph_geocache_list_filter(list);
}

/*
 * Find geocaches from all over the world.
 */
void
ph_geocache_list_set_global_range(PHGeocacheList *list)
{
    g_return_if_fail(list != NULL && PH_IS_GEOCACHE_LIST(list));

    list->priv->loaded_range.south = PH_GEO_MAX_SOUTH_MINFRAC;
    list->priv->loaded_range.north = PH_GEO_MAX_NORTH_MINFRAC;
    list->priv->loaded_range.west = PH_GEO_MAX_WEST_MINFRAC;
    list->priv->loaded_range.east = PH_GEO_MAX_EAST_MINFRAC;
    memcpy(&list->priv->visible_range, &list->priv->loaded_range,
            sizeof(PHGeocacheListRange));

    if (list->priv->sql != NULL)
        ph_geocache_list_run_query(list, FALSE);
}

/*
 * Set the query without changing the range.
 */
gboolean
ph_geocache_list_set_query(PHGeocacheList *list,
                           const gchar *query,
                           GError **error)
{
    gchar *sql;

    g_return_val_if_fail(list != NULL && PH_IS_GEOCACHE_LIST(list), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    sql = ph_geocache_list_sql_from_query(query, error);

    if (sql != NULL) {
        if (list->priv->sql != NULL)
            g_free(list->priv->sql);
        list->priv->sql = sql;

        ph_geocache_list_run_query(list, FALSE);

        return TRUE;
    }
    else
        return FALSE;
}

/*
 * Traverse the visible list to find the geocache with the given ID.
 */
GtkTreePath *
ph_geocache_list_find_by_id(PHGeocacheList *list,
                            const gchar *geocache_id)
{
    GtkTreePath *result = NULL;
    gint i = 0;
    GList *current = list->priv->visible_list;

    g_return_val_if_fail(list != NULL && PH_IS_GEOCACHE_LIST(list), NULL);

    while (current != NULL && current->data != NULL) {
        PHGeocacheListEntry *entry = (PHGeocacheListEntry *) current->data;

        if (strcmp(entry->id, geocache_id) == 0)
            break;

        current = current->next;
        ++i;
    }

    if (current != NULL && current->data != NULL)
        /* found a match */
        result = gtk_tree_path_new_from_indices(i, -1);

    return result;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
