/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_IMPORT_PROCESS_H
#define PH_IMPORT_PROCESS_H

/* Includes {{{1 */

#include "ph-database.h"
#include "ph-process.h"

/* GObject boilerplate {{{1 */

#define PH_TYPE_IMPORT_PROCESS (ph_import_process_get_type())
#define PH_IMPORT_PROCESS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
            PH_TYPE_IMPORT_PROCESS, PHImportProcess))
#define PH_IS_IMPORT_PROCESS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
            PH_TYPE_IMPORT_PROCESS))
#define PH_IMPORT_PROCESS_CLASS(cls) (G_TYPE_CHECK_CLASS_CAST((cls), \
            PH_TYPE_IMPORT_PROCESS, PHImportProcessClass))
#define PH_IS_IMPORT_PROCESS_CLASS(cls) (G_TYPE_CHECK_CLASS_TYPE((cls), \
            PH_TYPE_IMPORT_PROCESS))
#define PH_IMPORT_PROCESS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            PH_TYPE_IMPORT_PROCESS, PHImportProcessClass))

GType ph_import_process_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHImportProcess PHImportProcess;
typedef struct _PHImportProcessClass PHImportProcessClass;
typedef struct _PHImportProcessPrivate PHImportProcessPrivate;

struct _PHImportProcess {
    PHProcess parent;
    PHImportProcessPrivate *priv;
};

struct _PHImportProcessClass {
    PHProcessClass parent_class;

    /* signals */
    void (*filename_notify)(PHImportProcess *process, gchar *filename);
};

/* Public interface {{{1 */

PHProcess *ph_import_process_new(PHDatabase *database,
                                 const gchar *path);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
