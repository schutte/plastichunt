/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_PROCESS_H
#define PH_PROCESS_H

/* Includes {{{1 */

#include <glib.h>
#include <glib-object.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_PROCESS (ph_process_get_type())
#define PH_PROCESS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_PROCESS, \
            PHProcess))
#define PH_IS_PROCESS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_PROCESS))
#define PH_PROCESS_CLASS(cls) (G_TYPE_CHECK_CLASS_CAST((cls), PH_TYPE_PROCESS, \
            PHProcessClass))
#define PH_IS_PROCESS_CLASS(cls) (G_TYPE_CHECK_CLASS_TYPE((cls), \
            PH_TYPE_PROCESS))
#define PH_PROCESS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            PH_TYPE_PROCESS, PHProcessClass))

GType ph_process_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHProcess PHProcess;
typedef struct _PHProcessClass PHProcessClass;
typedef struct _PHProcessPrivate PHProcessPrivate;

struct _PHProcess {
    GObject parent;
    PHProcessPrivate *priv;
};

struct _PHProcessClass {
    GObjectClass parent_class;

    /* virtual */
    gboolean (*setup)(PHProcess *process, GError **error);
    gboolean (*step)(PHProcess *process, gdouble *fraction, GError **error);
    gboolean (*finish)(PHProcess *process, GError **error);

    /* signals */
    void (*progress_notify)(PHProcess *process, gdouble fraction);
    void (*error_notify)(PHProcess *process, GError *error);
    void (*stop_notify)(PHProcess *process);
};

/* Process states {{{1 */

/*
 * Process states.
 */
typedef enum _PHProcessState {
    PH_PROCESS_STATE_CREATED,
    PH_PROCESS_STATE_BEFORE_SETUP,
    PH_PROCESS_STATE_RUNNING,
    PH_PROCESS_STATE_BEFORE_FINISH,
    PH_PROCESS_STATE_STOPPED
} PHProcessState;

/* Public interface {{{1 */

void ph_process_start(PHProcess *process);
void ph_process_stop(PHProcess *process);

PHProcessState ph_process_get_state(const PHProcess *process);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
