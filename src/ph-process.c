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

#include "ph-process.h"

/* Signals {{{1 */

/*
 * Signals.
 */
enum {
    PH_PROCESS_SIGNAL_PROGRESS_NOTIFY,
    PH_PROCESS_SIGNAL_ERROR_NOTIFY,
    PH_PROCESS_SIGNAL_STOP_NOTIFY,
    PH_PROCESS_SIGNAL_COUNT
};

static guint ph_process_signals[PH_PROCESS_SIGNAL_COUNT];

/* Private data {{{1 */

#define PH_PROCESS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
            PH_TYPE_PROCESS, PHProcessPrivate))

/*
 * Private data.
 */
struct _PHProcessPrivate {
    PHProcessState state;       /* current state of the process */
};

/* Forward declarations {{{1 */

static void ph_process_class_init(PHProcessClass *cls);
static void ph_process_init(PHProcess *process);

static void ph_process_step(PHProcess *process);
static gboolean ph_process_do_setup(gpointer data);
static gboolean ph_process_do_step(gpointer data);
static gboolean ph_process_do_finish(gpointer data);

/* Standard GObject code {{{1 */

G_DEFINE_ABSTRACT_TYPE(PHProcess, ph_process, G_TYPE_OBJECT)

/*
 * Class initialization code.
 */
static void
ph_process_class_init(PHProcessClass *cls)
{
    g_type_class_add_private(cls, sizeof(PHProcessPrivate));

    ph_process_signals[PH_PROCESS_SIGNAL_PROGRESS_NOTIFY] =
        g_signal_new("progress-notify", PH_TYPE_PROCESS,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET(PHProcessClass, progress_notify),
                NULL, NULL,
                g_cclosure_marshal_VOID__DOUBLE,
                G_TYPE_NONE, 1, G_TYPE_DOUBLE);
    ph_process_signals[PH_PROCESS_SIGNAL_ERROR_NOTIFY] =
        g_signal_new("error-notify", PH_TYPE_PROCESS,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET(PHProcessClass, error_notify),
                NULL, NULL,
                g_cclosure_marshal_VOID__BOXED,
                G_TYPE_NONE, 1, G_TYPE_ERROR);
    ph_process_signals[PH_PROCESS_SIGNAL_STOP_NOTIFY] =
        g_signal_new("stop-notify", PH_TYPE_PROCESS,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET(PHProcessClass, stop_notify),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);
}

/*
 * Instance initialization code.
 */
static void
ph_process_init(PHProcess *process)
{
    PHProcessPrivate *priv = PH_PROCESS_GET_PRIVATE(process);

    process->priv = priv;
}

/* Starting and running {{{1 */

/*
 * Add a step to the idle queue.
 */
static void
ph_process_step(PHProcess *process)
{
    GSourceFunc func;

    switch (process->priv->state) {
    case PH_PROCESS_STATE_BEFORE_SETUP:
        func = ph_process_do_setup;
        break;
    case PH_PROCESS_STATE_RUNNING:
        func = ph_process_do_step;
        break;
    case PH_PROCESS_STATE_BEFORE_FINISH:
        func = ph_process_do_finish;
        break;
    default:
        func = NULL;
        break;
    }

    if (func != NULL)
        (void) g_idle_add(func, process);
}

/*
 * Run the setup stage of the process.
 */
static gboolean
ph_process_do_setup(gpointer data)
{
    PHProcess *process = PH_PROCESS(data);
    PHProcessClass *cls = PH_PROCESS_GET_CLASS(process);
    GError *error = NULL;
    gboolean success;

    success = cls->setup(process, &error);

    if (success)
        process->priv->state = PH_PROCESS_STATE_RUNNING;
    else {
        process->priv->state = PH_PROCESS_STATE_BEFORE_FINISH;
        g_signal_emit(process,
                ph_process_signals[PH_PROCESS_SIGNAL_ERROR_NOTIFY],
                0, error);
        g_error_free(error);
    }

    ph_process_step(process);
    return FALSE;
}

/*
 * Run a single step of the process.
 */
static gboolean
ph_process_do_step(gpointer data)
{
    PHProcess *process = PH_PROCESS(data);
    PHProcessClass *cls = PH_PROCESS_GET_CLASS(process);
    gdouble fraction = 0.0;
    GError *error = NULL;
    gboolean still_running;

    still_running = (process->priv->state == PH_PROCESS_STATE_RUNNING);
    still_running = still_running && cls->step(process, &fraction, &error);

    if (still_running) {
        g_signal_emit(process,
                ph_process_signals[PH_PROCESS_SIGNAL_PROGRESS_NOTIFY],
                0, fraction);
        return TRUE;
    }
    else {
        process->priv->state = PH_PROCESS_STATE_BEFORE_FINISH;

        if (error != NULL) {
            g_signal_emit(process,
                    ph_process_signals[PH_PROCESS_SIGNAL_ERROR_NOTIFY],
                    0, error);
            g_error_free(error);
        }

        ph_process_step(process);
        return FALSE;
    }
}

/*
 * Run the cleanup stage of the process.
 */
static gboolean
ph_process_do_finish(gpointer data)
{
    PHProcess *process = PH_PROCESS(data);
    PHProcessClass *cls = PH_PROCESS_GET_CLASS(process);
    GError *error = NULL;
    gboolean success;

    success = cls->finish(process, &error);
    process->priv->state = PH_PROCESS_STATE_STOPPED;

    if (!success) {
        g_signal_emit(process,
                ph_process_signals[PH_PROCESS_SIGNAL_ERROR_NOTIFY],
                0, error);
        g_error_free(error);
    }

    g_signal_emit(process,
            ph_process_signals[PH_PROCESS_SIGNAL_STOP_NOTIFY],
            0);

    return FALSE;
}

/* Public interface {{{1 */

/*
 * Start running the process.  Does nothing if it is running or has already
 * finished.
 */
void
ph_process_start(PHProcess *process)
{
    g_return_if_fail(process != NULL && PH_IS_PROCESS(process));

    if (process->priv->state == PH_PROCESS_STATE_CREATED) {
        process->priv->state = PH_PROCESS_STATE_BEFORE_SETUP;
        ph_process_step(process);
    }
}

/*
 * Cancel the process as soon as possible.
 */
void
ph_process_stop(PHProcess *process)
{
    g_return_if_fail(process != NULL && PH_IS_PROCESS(process));

    if (process->priv->state != PH_PROCESS_STATE_STOPPED)
        process->priv->state = PH_PROCESS_STATE_BEFORE_FINISH;
}

/*
 * Get the current state of the process.
 */
PHProcessState
ph_process_get_state(const PHProcess *process)
{
    g_return_val_if_fail(process != NULL && PH_IS_PROCESS(process), 0);

    return process->priv->state;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
