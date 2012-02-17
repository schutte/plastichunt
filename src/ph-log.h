/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_LOG_H
#define PH_LOG_H

/* Includes {{{1 */

#include "ph-database.h"

/* Log type enumeration {{{1 */

/*
 * Known types of log messages.
 */
typedef enum _PHLogType {
    PH_LOG_TYPE_UNKNOWN,
    PH_LOG_TYPE_FOUND,
    PH_LOG_TYPE_NOT_FOUND,
    PH_LOG_TYPE_NOTE,
    PH_LOG_TYPE_REVIEWER,
    PH_LOG_TYPE_PUBLISH,
    PH_LOG_TYPE_ENABLE,
    PH_LOG_TYPE_DISABLE,
    PH_LOG_TYPE_UPDATE,
    PH_LOG_TYPE_WILL_ATTEND,
    PH_LOG_TYPE_ATTENDED,
    PH_LOG_TYPE_WEBCAM,
    PH_LOG_TYPE_NEEDS_MAINTENANCE,
    PH_LOG_TYPE_MAINTENANCE,
    PH_LOG_TYPE_NEEDS_ARCHIVING,
    PH_LOG_TYPE_ARCHIVED,
    PH_LOG_TYPE_UNARCHIVED
} PHLogType;

/* Structure {{{1 */

/*
 * Representation of a row in the "logs" table.
 */
typedef struct _PHLog {
    gint id;                /* numeric ID supplied by listing site */
    gchar *geocache_id;     /* geocache this log belongs to */
    PHLogType type;         /* found, didn't find, etc. */
    gchar *logger;          /* user who wrote this log */
    glong logged;           /* log timestamp */
    gchar *details;         /* logger's message */
} PHLog;

/* Public interface {{{1 */

gboolean ph_logs_load_by_geocache_id(GList **list,
                                     PHDatabase *database,
                                     const gchar *id,
                                     GError **error);
gboolean ph_log_store(const PHLog *log,
                      PHDatabase *database,
                      GError **error);

void ph_log_free(PHLog *log);
void ph_logs_free(GList *list);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
