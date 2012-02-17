/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_MAIN_WINDOW_H
#define PH_MAIN_WINDOW_H

/* Includes {{{1 */

#include "ph-database.h"
#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_MAIN_WINDOW \
    (ph_main_window_get_type())
#define PH_MAIN_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_MAIN_WINDOW, PHMainWindow))
#define PH_IS_MAIN_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_MAIN_WINDOW))
#define PH_MAIN_WINDOW_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST((cls), PH_TYPE_MAIN_WINDOW, PHMainWindowClass))
#define PH_IS_MAIN_WINDOW_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE((cls), PH_TYPE_MAIN_WINDOW))
#define PH_MAIN_WINDOW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_MAIN_WINDOW, PHMainWindowClass))

GType ph_main_window_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHMainWindow PHMainWindow;
typedef struct _PHMainWindowClass PHMainWindowClass;
typedef struct _PHMainWindowPrivate PHMainWindowPrivate;

struct _PHMainWindow {
    GtkWindow parent;
    PHMainWindowPrivate *priv;
};

struct _PHMainWindowClass {
    GtkWindowClass parent_class;
};

/* Public interface {{{1 */

GtkWidget *ph_main_window_new(PHDatabase *database);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
