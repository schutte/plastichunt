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

#include "ph-geocache.h"
#include <stdlib.h>
#include <glib/gi18n.h>

/* Forward declarations {{{1 */

static void ph_geocache_from_row(PHGeocache *gc, sqlite3_stmt *stmt,
                                 gboolean full);

/* Listing sites {{{1 */

/*
 * Get the prefix used by a geocache listing site, such as GC on
 * geocaching.com.  Returns "" for unknown sites.
 */
const gchar *
ph_geocache_site_prefix(PHGeocacheSite site)
{
    switch (site) {
    case PH_GEOCACHE_SITE_GC_COM:
        return "GC";
    case PH_GEOCACHE_SITE_OC_DE:
        return "OC";
    default:
        return "";
    }
}

/* Database retrieval {{{1 */

/*
 * Fill in a geocache structure from a database row.
 */
static void
ph_geocache_from_row(PHGeocache *gc,
                     sqlite3_stmt *stmt,
                     gboolean full)
{
    gc->id = g_strdup((const gchar *) sqlite3_column_text(stmt, 0));
    gc->name = g_strdup((const gchar *) sqlite3_column_text(stmt, 1));
    gc->creator = g_strdup((const gchar *) sqlite3_column_text(stmt, 2));
    gc->owner = g_strdup((const gchar *) sqlite3_column_text(stmt, 3));
    gc->type = sqlite3_column_int(stmt, 4);
    gc->size = sqlite3_column_int(stmt, 5);
    gc->difficulty = sqlite3_column_int(stmt, 6);
    gc->terrain = sqlite3_column_int(stmt, 7);
    gc->attributes = ph_geocache_attrs_from_string(
            (const gchar *) sqlite3_column_text(stmt, 8));
    gc->summary_html = (sqlite3_column_int(stmt, 9) != 0);
    gc->summary = g_strdup((const gchar *) sqlite3_column_text(stmt, 10));
    gc->description_html = (sqlite3_column_int(stmt, 11));
    gc->description = g_strdup((const gchar *) sqlite3_column_text(stmt, 12));
    gc->hint = g_strdup((const gchar *) sqlite3_column_text(stmt, 13));
    gc->logged = (sqlite3_column_int(stmt, 14) != 0);
    gc->archived = (sqlite3_column_int(stmt, 15) != 0);
    gc->available = (sqlite3_column_int(stmt, 16) != 0);

    if (full) {
        gc->note.id = gc->id;
        gc->note.found = (sqlite3_column_int(stmt, 17) != 0);
        gc->note.note = g_strdup((const gchar *) sqlite3_column_text(stmt, 18));
    }
    else
        memset(&gc->note, 0, sizeof(gc->note));
}

/*
 * Find a geocache by its waypoint ID.
 */
PHGeocache *
ph_geocache_load_by_id(PHDatabase *database,
                       const gchar *id,
                       gboolean full,
                       GError **error)
{
    PHGeocache *result = NULL;
    char *query;
    sqlite3_stmt *stmt;
    gint status;

    g_return_val_if_fail(database != NULL, NULL);
    g_return_val_if_fail(id != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (full)
        query = sqlite3_mprintf("SELECT id, name, creator, owner, type, size, "
                "difficulty, terrain, attributes, summary_html, summary, "
                "description_html, description, hint, logged, archived, "
                "available, found, note FROM geocaches_full WHERE id = %Q", id);
    else
        query = sqlite3_mprintf("SELECT id, name, creator, owner, type, size, "
                "difficulty, terrain, attributes, summary_html, summary, "
                "description_html, description, hint, logged, archived, "
                "available FROM geocaches WHERE id = %Q", id);
    stmt = ph_database_prepare(database, query, error);
    sqlite3_free(query);
    if (stmt == NULL)
        return FALSE;

    status = ph_database_step(database, stmt, error);
    if (status == SQLITE_DONE)
        g_set_error(error, PH_DATABASE_ERROR, PH_DATABASE_ERROR_INCONSISTENT,
                _("Geocache `%s' not present in database"), id);
    else if (status == SQLITE_ROW) {
        result = g_new(PHGeocache, 1);
        ph_geocache_from_row(result, stmt, full);
    }

    sqlite3_finalize(stmt);

    return result;
}

/* Memory management {{{1 */

/*
 * Free memory associated with a geocache.  No-op for NULL.
 */
void
ph_geocache_free(PHGeocache *gc)
{
    if (gc == NULL)
        return;

    g_free(gc->id);
    g_free(gc->name);
    g_free(gc->creator);
    g_free(gc->owner);
    g_free(gc->summary);
    g_free(gc->description);
    g_free(gc->hint);
    ph_geocache_attrs_free(gc->attributes);
    /* do not free gc->note.id (== gc->id) */
    g_free(gc->note.note);
    g_free(gc);
}

/*
 * Copy a geocache note into newly allocated memory.
 */
PHGeocacheNote *
ph_geocache_note_copy(const PHGeocacheNote *note)
{
    PHGeocacheNote *result = g_new(PHGeocacheNote, 1);

    result->id = g_strdup(note->id);
    result->found = note->found;
    result->note = g_strdup(note->note);

    return result;
}

/*
 * Free memory associated with a geocache note.  No-op for NULL.
 */
void
ph_geocache_note_free(PHGeocacheNote *note)
{
    if (note == NULL)
        return;

    g_free(note->id);
    g_free(note->note);
    g_free(note);
}

/* Database storage {{{1 */

/*
 * Store the given geocache in the database.  Uses an INSERT OR REPLACE
 * statement to avoid duplicates.  Returns FALSE on error.
 */
gboolean
ph_geocache_store(const PHGeocache *gc,
                  PHDatabase *database,
                  GError **error)
{
    char *query;
    gboolean success;
    gchar *attributes;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    attributes = ph_geocache_attrs_to_string(gc->attributes);
    query = sqlite3_mprintf("INSERT OR REPLACE INTO geocaches "
            "(id, name, creator, owner, type, size, difficulty, terrain, "
            "attributes, summary_html, summary, description_html, description, "
            "hint, logged, archived, available) VALUES "
            "(%Q, %Q, %Q, %Q, %d, %d, %d, %d, %Q, %d, %Q, %d, %Q, %Q, "
            "%d, %d, %d)",
            gc->id, gc->name, gc->creator, gc->owner, gc->type,
            gc->size, gc->difficulty, gc->terrain, attributes,
            gc->summary_html ? 1 : 0, gc->summary,
            gc->description_html ? 1 : 0, gc->description,
            gc->hint, gc->logged ? 1 : 0,
            gc->archived ? 1 : 0, gc->available ? 1 : 0);
    success = ph_database_exec(database, query, error);
    g_free(attributes);
    sqlite3_free(query);

    return success;
}

/*
 * Store a geocache note via an INSERT OR REPLACE statement.  Returns FALSE on
 * error.
 */
gboolean
ph_geocache_note_store(const PHGeocacheNote *note,
                       PHDatabase *database,
                       GError **error)
{
    char *query;
    gboolean success;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    query = sqlite3_mprintf("INSERT OR REPLACE INTO geocache_notes "
            "(id, found, note) VALUES (%Q, %s, %Q)",
            note->id, note->found ? "1" : "NULL", note->note);
    success = ph_database_exec(database, query, error);
    sqlite3_free(query);

    return success;
}

/* Attribute handling {{{1 */

/*
 * Create a list of geocache attributes from a string stored in the database.
 */
PHGeocacheAttrs *
ph_geocache_attrs_from_string(const gchar *input)
{
    PHGeocacheAttrs *result = NULL;
    static GRegex *regex = NULL;
    GMatchInfo *match;
    gboolean matched;

    if (regex == NULL) {
        regex = g_regex_new("([+-])(\\d+);", 0, 0, NULL);
        g_return_val_if_fail(regex != NULL, NULL);
    }

    matched = g_regex_match(regex, input, 0, &match);
    while (matched) {
        PHGeocacheAttrs *attr = g_new(PHGeocacheAttrs, 1);
        gchar *temp;

        attr->next = result;
        attr->id = atoi(temp = g_match_info_fetch(match, 2));
        g_free(temp);
        attr->value = ((temp = g_match_info_fetch(match, 1))[0] == '+');
        g_free(temp);
        result = attr;
        matched = g_match_info_next(match, NULL);
    }
    g_match_info_free(match);

    return result;
}

/*
 * Create a parseable and searchable string representing the geocache
 * attributes.
 */
gchar *
ph_geocache_attrs_to_string(const PHGeocacheAttrs *attrs)
{
    GString *result = g_string_new("");

    for (; attrs != NULL; attrs = attrs->next)
        g_string_append_printf(result, "%c%d; ",
                attrs->value ? '+' : '-', attrs->id);

    if (result->len > 0)
        /* remove trailing space */
        g_string_truncate(result, result->len - 1);

    return g_string_free(result, FALSE);
}

/*
 * Search the attribute list for the given ID.  NULL will be returned if no
 * match could be found.
 */
PHGeocacheAttrs *
ph_geocache_attrs_find(PHGeocacheAttrs *attrs,
                       PHGeocacheAttrID id)
{
    while (attrs != NULL && attrs->id != id)
        attrs = attrs->next;
    return attrs;
}

/*
 * Set the entry from the attribute list with the given id to value.  If
 * necessary, create the entry.  Returns the new head of the list.
 */
PHGeocacheAttrs *
ph_geocache_attrs_set(PHGeocacheAttrs *attrs,
                      PHGeocacheAttrID id,
                      gboolean value)
{
    PHGeocacheAttrs *cur = attrs, *prev = NULL;

    while (cur != NULL && cur->id != id) {
        prev = cur;
        cur = cur->next;
    }

    if (cur == NULL) {
        cur = ph_geocache_attrs_prepend(NULL, id, value);
        if (prev == NULL)
            return cur;
        else {
            prev->next = cur;
            return attrs;
        }
    }
    else {
        cur->value = value;
        return attrs;
    }
}

/*
 * Allocate a PHGeocacheAttrs with the given id and value and adds it at the
 * head of the list.  Returns the new head.
 */
PHGeocacheAttrs *
ph_geocache_attrs_prepend(PHGeocacheAttrs *attrs,
                          PHGeocacheAttrID id,
                          gboolean value)
{
    PHGeocacheAttrs *result = g_new(PHGeocacheAttrs, 1);

    result->next = attrs;
    result->id = id;
    result->value = value;

    return result;
}

/*
 * Search attrs for an entry for the given id, remove this entry from the list
 * and deallocate its memory.  Returns the new head of the list.  This is
 * different from ph_geocache_attrs_set() with a FALSE value in that it
 * signifies an undefined attribute.
 */
PHGeocacheAttrs *
ph_geocache_attrs_unset(PHGeocacheAttrs *attrs,
                        PHGeocacheAttrID id)
{
    PHGeocacheAttrs *cur = attrs, *prev = NULL;

    while (cur != NULL && cur->id != id) {
        prev = cur;
        cur = cur->next;
    }

    if (cur == NULL)
        return attrs;
    else {
        PHGeocacheAttrs *next = cur->next;
        g_free(cur);
        if (prev != NULL) {
            prev->next = next;
            return attrs;
        }
        else
            return next;
    }
}

/*
 * Traverse the list of attributes and free all entries.
 */
void
ph_geocache_attrs_free(PHGeocacheAttrs *attrs)
{
    PHGeocacheAttrs *next;

    while (attrs != NULL) {
        next = attrs->next;
        g_free(attrs);
        attrs = next;
    }
}

/* Boxed type registration {{{1 */

GType
ph_geocache_note_get_type()
{
    static GType type = 0;

    if (type == 0)
        type = g_boxed_type_register_static("PHGeocacheNote",
                (GBoxedCopyFunc) ph_geocache_note_copy,
                (GBoxedFreeFunc) ph_geocache_note_free);

    return type;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
