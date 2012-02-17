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

#include "ph-common.h"
#include "ph-config.h"
#include "ph-database.h"
#include "ph-geocache-list.h"
#include "ph-import-process.h"
#include "ph-main-window.h"
#include "ph-map-tile-cache.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libxml/xmlversion.h>
#include <errno.h>

/* Forward declarations {{{1 */

static PHDatabase *ph_main_open_database(const gchar *path, GError **error);

static gboolean ph_main_import(PHDatabase *database, const gchar *path,
                               GError **error_out);
static void ph_main_import_filename(PHImportProcess *process, gchar *filename,
                                    gpointer data);
static void ph_main_import_error(PHProcess *process, GError *error_in,
                                 gpointer data);
static void ph_main_import_stop(PHProcess *process, gpointer data);

static gboolean ph_main_query(PHDatabase *database, const gchar *query,
                              GError **error);

static void ph_main_log(const gchar *log_domain, GLogLevelFlags log_level,
                        const gchar *message, gpointer data);

int main(int argc, char **argv);

/* Database {{{1 */

/*
 * Open the named database, or the default one if path is NULL.
 */
static PHDatabase *
ph_main_open_database(const gchar *path,
                      GError **error)
{
    PHDatabase *result = NULL;

    if (path == NULL) {
        gchar *dir = g_build_filename(g_get_user_data_dir(),
                g_get_prgname(), NULL);
        gchar *real_path = NULL;

        if (g_mkdir_with_parents(dir, 0700) == 0) {
            real_path = g_build_filename(dir, "geocaches.phdb", NULL);
            result = ph_database_new(real_path, TRUE, error);
        }
        else
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    _("Could not create data directory `%s': %s"),
                    dir, g_strerror(errno));

        g_free(dir);
        g_free(real_path);
    }
    else
        result = ph_database_new(path, TRUE, error);

    return result;
}

/* Importing {{{1 */

/*
 * Import the given GPX file into the database.
 */
static gboolean
ph_main_import(PHDatabase *database,
               const gchar *path,
               GError **error_out)
{
    GError *error_in = NULL;
    PHProcess *process;
    GMainLoop *loop;

    loop = g_main_loop_new(NULL, FALSE);

    process = ph_import_process_new(database, path);
    g_signal_connect(process, "filename-notify",
            G_CALLBACK(ph_main_import_filename), NULL);
    g_signal_connect(process, "error-notify",
            G_CALLBACK(ph_main_import_error), &error_in);
    g_signal_connect(process, "stop-notify",
            G_CALLBACK(ph_main_import_stop), loop);

    ph_process_start(process);

    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    g_object_unref(process);

    if (error_in != NULL) {
        g_propagate_error(error_out, error_in);
        return FALSE;
    }
    else
        return TRUE;
}

/*
 * Print the name of the file which will be imported next.
 */
static void
ph_main_import_filename(PHImportProcess *process,
                        gchar *filename,
                        gpointer data)
{
    fprintf(stderr, _("Importing `%s'...\n"), filename);
}

/*
 * Propagate an error while importing a file.
 */
static void
ph_main_import_error(PHProcess *process,
                     GError *error_in,
                     gpointer data)
{
    GError **error_out = (GError **) data;

    g_propagate_error(error_out, g_error_copy(error_in));
}

/*
 * Stop the main loop when the importing process has finished.
 */
static void
ph_main_import_stop(PHProcess *process,
                    gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;

    g_main_loop_quit(loop);
}

/* Geocache query {{{1 */

/*
 * Run a query and display the result set.
 */
static gboolean
ph_main_query(PHDatabase *database,
              const gchar *query,
              GError **error)
{
    PHGeocacheList *list = NULL;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean success;
    gboolean valid;

    list = ph_geocache_list_new();
    ph_geocache_list_set_database(list, database);
    ph_geocache_list_set_global_range(list);
    success = ph_geocache_list_set_query(list, query, error);

    if (success) {
        model = GTK_TREE_MODEL(list);
        valid = gtk_tree_model_get_iter_first(model, &iter);
        while (valid) {
            gchar *id = NULL, *name = NULL;
            gtk_tree_model_get(model, &iter,
                    PH_GEOCACHE_LIST_COLUMN_ID, &id,
                    PH_GEOCACHE_LIST_COLUMN_NAME, &name,
                    -1);
            g_print("%s: %s\n", id, name);
            g_free(id);
            g_free(name);
            valid = gtk_tree_model_iter_next(model, &iter);
        }
    }

    g_object_unref(list);

    return success;
}

/* Logging {{{1 */

/*
 * Log handler for messages generated by plastichunt.
 */
static void
ph_main_log(const gchar *log_domain,
            GLogLevelFlags log_level,
            const gchar *message,
            gpointer data)
{
    GLogLevelFlags threshold = GPOINTER_TO_INT(data);
    const gchar *prefix;

    log_level &= G_LOG_LEVEL_MASK;

    if (log_level > threshold)
        return;

    switch (log_level) {
    case G_LOG_LEVEL_ERROR:
        prefix = "ERROR: ";
        break;
    case G_LOG_LEVEL_CRITICAL:
        prefix = "CRITICAL: ";
        break;
    case G_LOG_LEVEL_WARNING:
        prefix = "WARNING: ";
        break;
    case G_LOG_LEVEL_MESSAGE:
        prefix = "Message: ";
        break;
    case G_LOG_LEVEL_INFO:
        prefix = "Info: ";
        break;
    case G_LOG_LEVEL_DEBUG:
        prefix = "Debug: ";
        break;
    default:
        prefix = "";
    }

    g_printerr("** (%s:%d): %s%s\n",
            g_get_prgname(), getpid(), prefix, message);
}

/* main() {{{1 */

/*
 * Main entry point of the application.
 */
int
main(int argc,
     char **argv)
{
    GError *error = NULL;
    gboolean success;
    gboolean start_gui = FALSE;
    gchar *database_filename = NULL;
    gchar *query = NULL;
    gchar **import_filenames = NULL;
    PHDatabase *database = NULL;
    gboolean verbose = FALSE;
    gboolean debug = FALSE;
    GLogLevelFlags log_threshold;

    GOptionEntry options[] = {
        { "database", 'd', 0, G_OPTION_ARG_FILENAME,
            &database_filename,
            N_("Name of the geocache database to use "
                    "(will be created if necessary)."),
            N_("FILENAME") },
        { "import", 'i', 0, G_OPTION_ARG_FILENAME_ARRAY,
            &import_filenames,
            N_("Import a GPX file (and do not start the GUI)."),
            N_("FILENAME") },
        { "query", 'q', 0, G_OPTION_ARG_STRING,
            &query,
            N_("Search for geocaches matching certain attributes "
                    "(and do not start the GUI)."),
            N_("QUERY") },
        { "gui", 'g', 0, G_OPTION_ARG_NONE,
            &start_gui,
            N_("Force the start of the GUI even if -i/-q are used."),
            NULL },
        { "verbose", 'v', 0, G_OPTION_ARG_NONE,
            &verbose,
            N_("Log some important events."),
            NULL },
        { "debug", 'D', 0, G_OPTION_ARG_NONE,
            &debug,
            N_("Activate debugging output."),
            NULL },
        { NULL }
    };

    g_set_prgname(PH_PROGRAM_NAME);
    g_set_application_name(PH_APPLICATION_NAME);

    LIBXML_TEST_VERSION
    success = gtk_init_with_args(&argc, &argv, NULL, options, NULL, &error);

    if (debug)
        log_threshold = G_LOG_LEVEL_DEBUG;
    else if (verbose)
        log_threshold = G_LOG_LEVEL_MESSAGE;
    else
        log_threshold = G_LOG_LEVEL_WARNING;

    (void) g_log_set_handler(NULL,
            G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
            ph_main_log, GINT_TO_POINTER(log_threshold));

    start_gui = start_gui || (import_filenames == NULL && query == NULL);

    if (success)
        success = ph_config_init(&error);

    if (success) {
        database = ph_main_open_database(database_filename, &error);
        success = (database != NULL);
    }
    g_free(database_filename);

    if (success && import_filenames != NULL) {
        gchar **import_filename = import_filenames;
        while (success && *import_filename != NULL) {
            success = ph_main_import(database, *import_filename, &error);
            ++import_filename;
        }
    }
    g_strfreev(import_filenames);

    if (success && query != NULL)
        success = ph_main_query(database, query, &error);
    g_free(query);

    if (success && start_gui) {
        GtkWidget *window = ph_main_window_new(database);
        gtk_widget_show_all(window);
        g_object_unref(database);
        database = NULL;

        ph_map_tile_cache_restart();

        gtk_main();
    }

    if (!success)
        g_print("%s: %s\n", g_get_prgname(), error->message);

    if (database != NULL)
        g_object_unref(database);

    return success ? 0 : 1;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
