/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_CELL_RENDERER_FACTS_H
#define PH_CELL_RENDERER_FACTS_H

/* Includes {{{1 */

#include "ph-sprite.h"
#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_CELL_RENDERER_FACTS (ph_cell_renderer_facts_get_type())
#define PH_CELL_RENDERER_FACTS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_CELL_RENDERER_FACTS, \
				PHCellRendererFacts))
#define PH_CELL_RENDERER_FACTS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), PH_TYPE_CELL_RENDERER_FACTS, \
			     PHCellRendererFactsClass))
#define PH_IS_CELL_RENDERER_FACTS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_CELL_RENDERER_FACTS))
#define PH_IS_CELL_RENDERER_FACTS_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), PH_TYPE_CELL_RENDERER_FACTS))
#define PH_CELL_RENDERER_FACTS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_CELL_RENDERER_FACTS, \
			       PHCellRendererFactsClass))

GType ph_cell_renderer_facts_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHCellRendererFacts PHCellRendererFacts;
typedef struct _PHCellRendererFactsClass PHCellRendererFactsClass;
typedef struct _PHCellRendererFactsPrivate PHCellRendererFactsPrivate;

struct _PHCellRendererFacts {
    GtkCellRenderer parent;
    PHCellRendererFactsPrivate *priv;
};

struct _PHCellRendererFactsClass {
    GtkCellRendererClass parent_class;
};

/* Public interface {{{1 */

enum {
    PH_CELL_RENDERER_FACTS_SHOW_SIZE = 0x01,
    PH_CELL_RENDERER_FACTS_SHOW_DIFFICULTY = 0x02,
    PH_CELL_RENDERER_FACTS_SHOW_TERRAIN = 0x04,
    PH_CELL_RENDERER_FACTS_SHOW_ALL = 0x07
};

GtkCellRenderer *ph_cell_renderer_facts_new(guint show,
                                            PHSpriteSize sprite_size);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
