/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_TRACKABLE_H
#define PH_TRACKABLE_H

/* Includes {{{1 */

#include "ph-database.h"

/* Structure {{{1 */

/*
 * Representation of a row in the "trackables" table.
 */
typedef struct _PHTrackable {
    gchar *id;                  /* something like TB123 */
    gchar *name;                /* human-readable name */
    gchar *geocache_id;         /* where it is logged */
} PHTrackable;

/* Public interface {{{1 */

gboolean ph_trackables_load_by_geocache_id(GList **list,
                                           PHDatabase *database,
                                           const gchar *id,
                                           GError **error);
gboolean ph_trackable_store(const PHTrackable *trackable,
                            PHDatabase *database,
                            GError **error);

void ph_trackable_free(PHTrackable *trackable);
void ph_trackables_free(GList *list);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
