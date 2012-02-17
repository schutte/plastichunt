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
#include "ph-gpx.h"
#include "ph-import-process.h"
#include "ph-log.h"
#include "ph-trackable.h"
#include "ph-waypoint.h"
#include "ph-xml.h"
#include <glib/gi18n.h>
#include <libxml/xmlreader.h>
#include <math.h>
#include <sys/stat.h>

/* Properties {{{1 */

enum {
    PH_IMPORT_PROCESS_PROP_0,
    PH_IMPORT_PROCESS_PROP_PATH,
    PH_IMPORT_PROCESS_PROP_DATABASE
};

/* Signals {{{1 */

enum {
    PH_IMPORT_PROCESS_SIGNAL_FILENAME_NOTIFY,
    PH_IMPORT_PROCESS_SIGNAL_COUNT
};

static guint ph_import_process_signals[PH_IMPORT_PROCESS_SIGNAL_COUNT];

/* Private data {{{1 */

#define PH_IMPORT_PROCESS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
            PH_TYPE_IMPORT_PROCESS, PHImportProcessPrivate))

/*
 * Private data.
 */
struct _PHImportProcessPrivate {
    PHDatabase *database;           /* target database */
    gchar *path;                    /* path specified at instantiation */

    GDir *dir;                      /* directory cursor */
    gchar *filename;                /* file currently being imported */
    goffset total;                  /* length of the current file in bytes */
    PHGeocacheSite site;            /* originating listing site */
    xmlTextReaderPtr xml_reader;    /* XML reader for the file */

    gboolean success;               /* has the entire process succeeded? */
};

/* Forward declarations {{{1 */

static void ph_import_process_class_init(PHImportProcessClass *cls);
static void ph_import_process_init(PHImportProcess *process);
static void ph_import_process_dispose(GObject *object);
static void ph_import_process_finalize(GObject *object);
static void ph_import_process_set_property(
    GObject *object, guint id, const GValue *value, GParamSpec *spec);
static void ph_import_process_get_property(
    GObject *object, guint id, GValue *value, GParamSpec *spec);

static gboolean ph_import_process_setup(PHProcess *parent_process,
                                        GError **error);
static gboolean ph_import_process_step(PHProcess *parent_process,
                                       gdouble *fraction, GError **error);
static gboolean ph_import_process_next_file(PHImportProcess *process,
                                            GError **error);
static gboolean ph_import_process_finish(PHProcess *parent_process,
                                         GError **error);

static void ph_import_process_prefix_error(PHImportProcess *process,
                                           GError **error);

static gboolean ph_import_process_gpx_wpt(PHImportProcess *process,
                                          GError **error);
static gboolean ph_import_process_gpx_cache(PHImportProcess *process,
                                            const PHWaypoint *wpt,
                                            gboolean logged, GError **error);
static gboolean ph_import_process_gpx_logs(PHImportProcess *process,
                                           const PHGeocache *gc,
                                           GError **error);
static gboolean ph_import_process_gpx_travelbugs(PHImportProcess *process,
                                                 const PHGeocache *gc,
                                                 GError **error);
static gboolean ph_import_process_gpx_author(PHImportProcess *process,
                                             GError **error);

/* Standard GObject code {{{1 */

G_DEFINE_TYPE(PHImportProcess, ph_import_process, PH_TYPE_PROCESS)

/*
 * Class initialization code.
 */
static void
ph_import_process_class_init(PHImportProcessClass *cls)
{
    GObjectClass *g_obj_cls = G_OBJECT_CLASS(cls);
    PHProcessClass *process_cls = PH_PROCESS_CLASS(cls);

    g_obj_cls->dispose = ph_import_process_dispose;
    g_obj_cls->finalize = ph_import_process_finalize;
    g_obj_cls->set_property = ph_import_process_set_property;
    g_obj_cls->get_property = ph_import_process_get_property;

    process_cls->setup = ph_import_process_setup;
    process_cls->step = ph_import_process_step;
    process_cls->finish = ph_import_process_finish;

    g_type_class_add_private(cls, sizeof(PHImportProcessPrivate));

    g_object_class_install_property(g_obj_cls,
            PH_IMPORT_PROCESS_PROP_PATH,
            g_param_spec_string("path", "import path",
                "filesystem path to be imported",
                "",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(g_obj_cls,
            PH_IMPORT_PROCESS_PROP_DATABASE,
            g_param_spec_object("database", "target database",
                "database to store imported geocache information",
                PH_TYPE_DATABASE,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    ph_import_process_signals[PH_IMPORT_PROCESS_SIGNAL_FILENAME_NOTIFY] =
        g_signal_new("filename-notify", PH_TYPE_IMPORT_PROCESS,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET(PHImportProcessClass, filename_notify),
                NULL, NULL,
                g_cclosure_marshal_VOID__STRING,
                G_TYPE_NONE, 1, G_TYPE_STRING);
}

/*
 * Instance initialization code.
 */
static void
ph_import_process_init(PHImportProcess *process)
{
    PHImportProcessPrivate *priv = PH_IMPORT_PROCESS_GET_PRIVATE(process);

    process->priv = priv;
}

/*
 * Drop references to other objects.
 */
static void
ph_import_process_dispose(GObject *object)
{
    PHImportProcess *process = PH_IMPORT_PROCESS(object);

    if (process->priv->database != NULL) {
        g_object_unref(process->priv->database);
        process->priv->database = NULL;
    }

    if (G_OBJECT_CLASS(ph_import_process_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(ph_import_process_parent_class)->dispose(object);
}

/*
 * Instance destruction code.
 */
static void
ph_import_process_finalize(GObject *object)
{
    PHImportProcess *process = PH_IMPORT_PROCESS(object);

    if (process->priv->path != NULL)
        g_free(process->priv->path);
    if (process->priv->dir != NULL)
        g_dir_close(process->priv->dir);
    if (process->priv->filename != NULL)
        g_free(process->priv->filename);
    if (process->priv->xml_reader != NULL)
        xmlFreeTextReader(process->priv->xml_reader);

    if (G_OBJECT_CLASS(ph_import_process_parent_class)->finalize != NULL)
        G_OBJECT_CLASS(ph_import_process_parent_class)->finalize(object);
}

/*
 * Property mutator.
 */
static void
ph_import_process_set_property(GObject *object,
                               guint id,
                               const GValue *value,
                               GParamSpec *spec)
{
    PHImportProcess *process = PH_IMPORT_PROCESS(object);

    switch (id) {
    case PH_IMPORT_PROCESS_PROP_PATH:
        if (process->priv->path != NULL)
            g_free(process->priv->path);
        process->priv->path = g_value_dup_string(value);
        break;
    case PH_IMPORT_PROCESS_PROP_DATABASE:
        if (process->priv->database != NULL)
            g_object_unref(process->priv->database);
        process->priv->database = PH_DATABASE(g_value_dup_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
    }
}

/*
 * Property accessor.
 */
static void
ph_import_process_get_property(GObject *object,
                               guint id,
                               GValue *value,
                               GParamSpec *spec)
{
    PHImportProcess *process = PH_IMPORT_PROCESS(object);

    switch (id) {
    case PH_IMPORT_PROCESS_PROP_PATH:
        g_value_set_string(value, process->priv->path);
        break;
    case PH_IMPORT_PROCESS_PROP_DATABASE:
        g_value_set_object(value, process->priv->database);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, spec);
    }
}

/* Setup {{{1 */

/*
 * Prepare the process for reading the (first) file.
 */
static gboolean
ph_import_process_setup(PHProcess *parent_process,
                        GError **error)
{
    PHImportProcess *process = PH_IMPORT_PROCESS(parent_process);

    if (!ph_database_begin(process->priv->database, error))
        return FALSE;

    if (g_file_test(process->priv->path, G_FILE_TEST_IS_DIR)) {
        /* open as a directory */
        process->priv->dir = g_dir_open(process->priv->path, 0, error);
        if (process->priv->dir == NULL)
            return FALSE;
    }

    if (ph_import_process_next_file(process, error))
        return TRUE;
    else {
        ph_import_process_prefix_error(process, error);
        return FALSE;
    }
}

/* Step {{{1 */

static gboolean
ph_import_process_step(PHProcess *parent_process,
                       gdouble *fraction,
                       GError **error)
{
    PHImportProcess *process = PH_IMPORT_PROCESS(parent_process);
    int rc;
    const xmlChar *name;
    gboolean success = TRUE;

    if (process->priv->xml_reader == NULL) {
        /* finished successfully */
        *fraction = 1.0;
        process->priv->success = TRUE;
        return FALSE;
    }

    rc = xmlTextReaderRead(process->priv->xml_reader);
    if (rc == 0) {
        /* end of file */
        *fraction = 0.0;
        if (ph_import_process_next_file(process, error))
            return TRUE;
        else {
            ph_import_process_prefix_error(process, error);
            return FALSE;
        }
    }
    else if (rc == -1) {
        /* an error has occurred */
        (void) ph_xml_set_last_error(error);
        ph_import_process_prefix_error(process, error);
        return FALSE;
    }

    name = xmlTextReaderConstLocalName(process->priv->xml_reader);
    if (xmlStrcmp(name, (xmlChar *) "wpt") == 0)
        success = ph_import_process_gpx_wpt(process, error);
    else if (xmlStrcmp(name, (xmlChar *) "author") == 0)
        success = ph_import_process_gpx_author(process, error);

    if (success) {
        long nread = xmlTextReaderByteConsumed(process->priv->xml_reader);
        if (process->priv->total != 0)
            *fraction = ((gdouble) nread) / process->priv->total;
        else
            *fraction = 0.0;
    }
    else
        ph_import_process_prefix_error(process, error);

    return success;
}

/*
 * Open the next (or only) file for reading.  Returns FALSE on error and TRUE in
 * all other cases, including the case where there are no more files; in the
 * latter case, priv->xml_reader will be NULL.
 */
static gboolean
ph_import_process_next_file(PHImportProcess *process,
                            GError **error)
{
    /* close the old reader */
    if (process->priv->xml_reader != NULL) {
        xmlFreeTextReader(process->priv->xml_reader);
        process->priv->xml_reader = NULL;
    }

    process->priv->site = PH_GEOCACHE_SITE_UNKNOWN;

    if (process->priv->dir != NULL) {
        /* if operating on a directory, get the next file */
        const gchar *name = g_dir_read_name(process->priv->dir);

        g_free(process->priv->filename);
        if (name != NULL)
            process->priv->filename = g_build_filename(process->priv->path,
                    name, NULL);
        else
            process->priv->filename = NULL;
    }
    else if (process->priv->filename == NULL)
        /* single file, not imported yet */
        process->priv->filename = g_strdup(process->priv->path);
    else {
        /* single file, already processed */
        g_free(process->priv->filename);
        process->priv->filename = NULL;
    }

    if (process->priv->filename != NULL) {
        struct stat info;

        g_signal_emit(process, ph_import_process_signals[
                    PH_IMPORT_PROCESS_SIGNAL_FILENAME_NOTIFY],
                0, process->priv->filename);

        if (stat(process->priv->filename, &info) == 0)
            process->priv->total = info.st_size;
        else
            process->priv->total = 0;

        process->priv->xml_reader = xmlReaderForFile(process->priv->filename,
                NULL, 0);
        if (process->priv->xml_reader == NULL)
            return ph_xml_set_last_error(error);
    }

    return TRUE;
}

/* Cleanup {{{1 */

/*
 * Perform the final steps.  In particular, close open files and commit or
 * rollback the database transaction.
 */
static gboolean
ph_import_process_finish(PHProcess *parent_process,
                         GError **error)
{
    PHImportProcess *process = PH_IMPORT_PROCESS(parent_process);

    if (process->priv->xml_reader != NULL) {
        xmlFreeTextReader(process->priv->xml_reader);
        process->priv->xml_reader = NULL;
    }

    if (process->priv->dir != NULL) {
        g_dir_close(process->priv->dir);
        process->priv->dir = NULL;
    }

    if (process->priv->success)
        return ph_database_commit_notify(process->priv->database, error);
    else
        return ph_database_rollback(process->priv->database, error);
}

/* Error reporting {{{1 */

/*
 * If the process is currently dealing with a file, prepend its name to the
 * error message to provide some context to the user.
 */
static void
ph_import_process_prefix_error(PHImportProcess *process,
                               GError **error)
{
    if (process->priv->filename != NULL)
        g_prefix_error(error, _("Importing file “%s” failed: "),
                process->priv->filename);
}

/* Importing GPX {{{1 */

/* Waypoint {{{2 */

/*
 * Interpret a <wpt> element and store the information in the "waypoints",
 * "geocaches" and "logs" tables of the database.  Returns FALSE on error.
 */
static gboolean
ph_import_process_gpx_wpt(PHImportProcess *process,
                          GError **error)
{
    PHWaypoint wpt = {0};
    xmlTextReaderPtr reader = process->priv->xml_reader;
    gboolean success = TRUE;
    int rc = 1, depth;
    const xmlChar *elname;
    double coord;
    const gchar *prefix = ph_geocache_site_prefix(process->priv->site);
    gboolean logged = FALSE;

    /* we save lat and lon in 1/1000s of minutes */
    if (!ph_xml_attrib_double(reader, (xmlChar *) "lat", &coord, error))
        return FALSE;
    wpt.latitude = (gint) round(60000 * coord);
    if (!ph_xml_attrib_double(reader, (xmlChar *) "lon", &coord, error))
        return FALSE;
    wpt.longitude = (gint) round(60000 * coord);

    depth = xmlTextReaderDepth(reader);
    do {
        rc = xmlTextReaderRead(reader);
        if (rc != 1)
            break;
        else if (xmlTextReaderNodeType(reader) != 1)
            continue;       /* only consider element nodes */
        elname = xmlTextReaderConstLocalName(reader);

        if (xmlStrcmp(elname, (xmlChar *) "name") == 0) {
            xmlChar *tmp = NULL;
            success = ph_xml_extract_text(reader, &tmp, error);
            if (!success)
                ;
            else if (xmlStrncmp(tmp, (xmlChar *) prefix,
                        PH_GEOCACHE_SITE_PREFIX_LENGTH) != 0) {
                /* not a geocache (extra waypoint) */
                wpt.id = g_strdup_printf("%s,%s", prefix, tmp);
                wpt.geocache_id = g_strdup_printf("%s%s", prefix, tmp + 2);
            }
            else {
                /* waypoint representing an actual geocache */
                wpt.id = g_strdup((gchar *) tmp);
                wpt.geocache_id = NULL;
            }
            xmlFree(tmp);
        }
        else if (xmlStrcmp(elname, (xmlChar *) "time") == 0)
            success = ph_xml_extract_time(reader, &wpt.placed, error);
        else if (xmlStrcmp(elname, (xmlChar *) "url") == 0)
            success = ph_xml_extract_text(reader, (xmlChar **) &wpt.url,
                    error);
        else if (xmlStrcmp(elname, (xmlChar *) "urlname") == 0)
            success = ph_xml_extract_text(reader, (xmlChar **) &wpt.name,
                    error);
        else if (xmlStrcmp(elname, (xmlChar *) "sym") == 0) {
            xmlChar *tmp = NULL;
            success = ph_xml_extract_text(reader, &tmp, error);
            if (success) {
                wpt.type = ph_xml_find_string(ph_gpx_waypoint_types, tmp);
                if (wpt.type == PH_WAYPOINT_TYPE_GEOCACHE)
                    /* this will be used to process <cache> */
                    logged = (xmlStrcasestr(tmp, (xmlChar *) "found") != NULL);
            }
            xmlFree(tmp);
        }
        else if (xmlStrcmp(elname, (xmlChar *) "desc") == 0)
            success = ph_xml_extract_text(reader, (xmlChar **) &wpt.summary,
                    error);
        else if (xmlStrcmp(elname, (xmlChar *) "cmt") == 0)
            success = ph_xml_extract_text(reader,
                    (xmlChar **) &wpt.description, error);
        else if (xmlStrcmp(elname, (xmlChar *) "cache") == 0)
            success = ph_import_process_gpx_cache(process, &wpt, logged, error);
    } while (success && xmlTextReaderDepth(reader) > depth);

    if (rc == -1)
        success = ph_xml_set_last_error(error);

    if (success)
        success = ph_waypoint_store(&wpt, process->priv->database, error);

    g_free(wpt.id);
    g_free(wpt.geocache_id);
    xmlFree(wpt.name);
    xmlFree(wpt.url);
    xmlFree(wpt.summary);
    xmlFree(wpt.description);

    return success;
}

/* Geocache {{{2 */

/*
 * Import a <groundspeak:cache> element into the "geocaches" database.
 * Returns FALSE on error.
 */
static gboolean
ph_import_process_gpx_cache(PHImportProcess *process,
                            const PHWaypoint *wpt,
                            gboolean logged,
                            GError **error)
{
    PHGeocache gc = {0};
    xmlTextReaderPtr reader = process->priv->xml_reader;
    gboolean success = TRUE;
    const xmlChar *elname;
    int rc, depth;

    gc.id = wpt->id;
    gc.logged = logged;

    gc.available = ph_xml_attrib_compare(reader, (xmlChar *) "available",
            (xmlChar *) "true");
    gc.archived = ph_xml_attrib_compare(reader, (xmlChar *) "archived",
            (xmlChar *) "true");

    depth = xmlTextReaderDepth(reader);
    do {
        rc = xmlTextReaderRead(reader);
        if (rc != 1)
            break;
        else if (xmlTextReaderNodeType(reader) != 1)
            continue;       /* only consider element nodes */
        elname = xmlTextReaderConstLocalName(reader);

        if (xmlStrcmp(elname, (xmlChar *) "name") == 0)
            success = ph_xml_extract_text(reader,
                    (xmlChar **) &gc.name, error);
        else if (xmlStrcmp(elname, (xmlChar *) "placed_by") == 0)
            success = ph_xml_extract_text(reader,
                    (xmlChar **) &gc.creator, error);
        else if (xmlStrcmp(elname, (xmlChar *) "owner") == 0)
            success = ph_xml_extract_text(reader,
                    (xmlChar **) &gc.owner, error);
        else if (xmlStrcmp(elname, (xmlChar *) "type") == 0)
            success = ph_xml_extract_value(reader, ph_gpx_geocache_types,
                    (gint *) &gc.type, error);
        else if (xmlStrcmp(elname, (xmlChar *) "container") == 0)
            success = ph_xml_extract_value(reader, ph_gpx_geocache_sizes,
                    (gint *) &gc.size, error);
        else if (xmlStrcmp(elname, (xmlChar *) "difficulty") == 0) {
            double tmp;
            success = ph_xml_extract_double(reader, &tmp, error);
            if (success)
                gc.difficulty = (guint8) round(tmp * 10);
        }
        else if (xmlStrcmp(elname, (xmlChar *) "terrain") == 0) {
            double tmp;
            success = ph_xml_extract_double(reader, &tmp, error);
            if (success)
                gc.terrain = (guint8) round(tmp * 10);
        }
        else if (xmlStrcmp(elname, (xmlChar *) "short_description") == 0) {
            gc.summary_html = ph_xml_attrib_compare(reader,
                    (xmlChar *) "html", (xmlChar *) "true");
            success = ph_xml_extract_text(reader,
                    (xmlChar **) &gc.summary, error);
        }
        else if (xmlStrcmp(elname, (xmlChar *) "long_description") == 0) {
            gc.description_html = ph_xml_attrib_compare(reader,
                    (xmlChar *) "html", (xmlChar *) "true");
            success = ph_xml_extract_text(reader,
                    (xmlChar **) &gc.description, error);
        }
        else if (xmlStrcmp(elname, (xmlChar *) "encoded_hints") == 0)
            success = ph_xml_extract_text(reader, (xmlChar **) &gc.hint,
                    error);
        else if (xmlStrcmp(elname, (xmlChar *) "logs") == 0)
            success = ph_import_process_gpx_logs(process, &gc, error);
        else if (xmlStrcmp(elname, (xmlChar *) "attribute") == 0) {
            gint id, value;
            success = ph_xml_attrib_int(reader, (xmlChar *) "id", &id, error) &&
                ph_xml_attrib_int(reader, (xmlChar *) "inc", &value, error);
            if (success)
                gc.attributes = ph_geocache_attrs_prepend(gc.attributes,
                        id, value != 0);
        }
        else if (xmlStrcmp(elname, (xmlChar *) "travelbugs") == 0)
            success = ph_import_process_gpx_travelbugs(process, &gc, error);
    } while (success && xmlTextReaderDepth(reader) > depth);

    if (rc == -1)
        success = ph_xml_set_last_error(error);

    if (success)
        success = ph_geocache_store(&gc, process->priv->database, error);

    ph_geocache_attrs_free(gc.attributes);
    xmlFree(gc.name);
    xmlFree(gc.creator);
    xmlFree(gc.owner);
    xmlFree(gc.summary);
    xmlFree(gc.description);
    xmlFree(gc.hint);

    return success;
}

/* Logs {{{2 */

/*
 * Scan all <log> elements inside a <logs>...</logs> section of a geocache
 * listing and store them in the database.  Returns FALSE on error.
 */
static gboolean
ph_import_process_gpx_logs(PHImportProcess *process,
                           const PHGeocache *gc,
                           GError **error)
{
    PHLog log = {0};
    xmlTextReaderPtr reader = process->priv->xml_reader;
    gboolean success = TRUE;
    int rc, depth;
    const xmlChar *elname;
    gboolean in_log = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (xmlTextReaderIsEmptyElement(reader))
        return TRUE;
    depth = xmlTextReaderDepth(reader);
    do {
        rc = xmlTextReaderRead(reader);
        if (rc != 1)
            break;
        elname = xmlTextReaderConstLocalName(reader);

        if (in_log && xmlTextReaderNodeType(reader) == 15 &&
                xmlStrcmp(elname, (xmlChar *) "log") == 0) {
            /* </log>: store log in database */
            log.geocache_id = gc->id;
            success = ph_log_store(&log, process->priv->database, error);
            /* prepare for the next log */
            xmlFree(log.logger); log.logger = NULL;
            xmlFree(log.details); log.details = NULL;
            in_log = FALSE;
        }
        else if (xmlTextReaderNodeType(reader) != 1)
            continue;       /* ignore all other non-element nodes */
        else if (!in_log && xmlStrcmp(elname, (xmlChar *) "log") == 0) {
            /* <log>: reset everything for the next log */
            memset(&log, 0, sizeof(log));
            in_log = TRUE;
            success = ph_xml_attrib_int(reader, (xmlChar *) "id",
                    &log.id, error);
        }
        else if (!in_log)
            continue;       /* ignore stuff outside <log>...</log> */
        else if (xmlStrcmp(elname, (xmlChar *) "date") == 0)
            success = ph_xml_extract_time(reader, &log.logged, error);
        else if (xmlStrcmp(elname, (xmlChar *) "type") == 0)
            success = ph_xml_extract_value(reader, ph_gpx_log_types,
                    (gint *) &log.type, error);
        else if (xmlStrcmp(elname, (xmlChar *) "finder") == 0)
            success = ph_xml_extract_text(reader,
                    (xmlChar **) &log.logger, error);
        else if (xmlStrcmp(elname, (xmlChar *) "text") == 0)
            success = ph_xml_extract_text(reader,
                    (xmlChar **) &log.details, error);
    } while (success && xmlTextReaderDepth(reader) > depth);

    if (rc == -1)
        success = ph_xml_set_last_error(error);

    if (in_log) {
        xmlFree(log.logger);
        xmlFree(log.details);
    }

    return success;
}

/* Travelbugs {{{2 */

/*
 * First remove all trackables associated with the given geocache; then read
 * and store the content of a <travelbugs> element to determine the current
 * status.
 */
static gboolean
ph_import_process_gpx_travelbugs(PHImportProcess *process,
                                 const PHGeocache *gc,
                                 GError **error)
{
    PHTrackable trackable = {0};
    xmlTextReaderPtr reader = process->priv->xml_reader;
    gboolean success;
    int depth, rc;
    const xmlChar *elname;
    gboolean in_travelbug = FALSE;
    char *query;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    /* remove current trackables */
    query = sqlite3_mprintf("DELETE FROM trackables WHERE geocache_id = %Q",
            gc->id);
    success = ph_database_exec(process->priv->database, query, error);
    sqlite3_free(query);
    if (!success)
        return FALSE;

    trackable.geocache_id = gc->id;

    /* get list of new trackables */
    if (xmlTextReaderIsEmptyElement(reader))
        return TRUE;
    depth = xmlTextReaderDepth(reader);
    do {
        rc = xmlTextReaderRead(reader);
        if (rc != 1)
            break;
        elname = xmlTextReaderConstLocalName(reader);

        if (xmlStrcmp(elname, (xmlChar *) "travelbug") == 0) {
            if (!in_travelbug && xmlTextReaderNodeType(reader) == 1) {
                /* <travelbug>: prepare for next trackable */
                trackable.name = trackable.id = NULL;
                success = ph_xml_attrib_text(reader, (xmlChar *) "ref",
                        (xmlChar **) &trackable.id, error);
                if (success)
                    in_travelbug = TRUE;
            }
            else if (in_travelbug && xmlTextReaderNodeType(reader) == 15) {
                /* </travelbug>: store in DB */
                success = ph_trackable_store(&trackable,
                        process->priv->database, error);
                xmlFree(trackable.id); trackable.id = NULL;
                xmlFree(trackable.name); trackable.name = NULL;
                in_travelbug = FALSE;
            }
        }
        else if (xmlTextReaderNodeType(reader) != 1)
            continue;       /* other than that, consider element nodes */
        else if (in_travelbug && xmlStrcmp(elname, (xmlChar *) "name") == 0)
            success = ph_xml_extract_text(reader, (xmlChar **) &trackable.name,
                    error);
    } while (success && xmlTextReaderDepth(reader) > depth);

    if (rc == -1)
        success = ph_xml_set_last_error(error);

    if (in_travelbug) {
        xmlFree(trackable.id);
        xmlFree(trackable.name);
    }

    return TRUE;
}

/* Author {{{2 */

/*
 * Interpret the geocache listing site information from an <author> element.
 * This is needed to determine the ID of the geocache each extra waypoint
 * belongs to.
 */
static gboolean
ph_import_process_gpx_author(PHImportProcess *process,
                             GError **error)
{
    xmlTextReaderPtr reader = process->priv->xml_reader;
    gint value;

    if (ph_xml_extract_value(reader, ph_gpx_geocache_sites, &value, error)) {
        process->priv->site = (PHGeocacheSite) value;
        return TRUE;
    }
    else
        return FALSE;
}

/* }}} */

/* Public interface {{{1 */

/*
 * Create a new process importing from a given path (which can refer to a file
 * or directory) into the database.
 */
PHProcess *
ph_import_process_new(PHDatabase *database,
                      const gchar *path)
{
    g_return_val_if_fail(database != NULL, NULL);
    g_return_val_if_fail(path != NULL, NULL);

    return PH_PROCESS(g_object_new(PH_TYPE_IMPORT_PROCESS,
                "path", path,
                "database", database,
                NULL));
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
