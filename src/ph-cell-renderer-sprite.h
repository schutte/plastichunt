/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_CELL_RENDERER_SPRITE_H
#define PH_CELL_RENDERER_SPRITE_H

/* Includes {{{1 */

#include "ph-sprite.h"
#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_CELL_RENDERER_SPRITE (ph_cell_renderer_sprite_get_type())
#define PH_CELL_RENDERER_SPRITE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_CELL_RENDERER_SPRITE, \
				PHCellRendererSprite))
#define PH_CELL_RENDERER_SPRITE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), PH_TYPE_CELL_RENDERER_SPRITE, \
			     PHCellRendererSpriteClass))
#define PH_IS_CELL_RENDERER_SPRITE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_CELL_RENDERER_SPRITE))
#define PH_IS_CELL_RENDERER_SPRITE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), PH_TYPE_CELL_RENDERER_SPRITE))
#define PH_CELL_RENDERER_SPRITE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_CELL_RENDERER_SPRITE, \
			       PHCellRendererSpriteClass))

GType ph_cell_renderer_sprite_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHCellRendererSprite PHCellRendererSprite;
typedef struct _PHCellRendererSpriteClass PHCellRendererSpriteClass;
typedef struct _PHCellRendererSpritePrivate PHCellRendererSpritePrivate;

struct _PHCellRendererSprite {
    GtkCellRenderer parent;
    PHCellRendererSpritePrivate *priv;
};

struct _PHCellRendererSpriteClass {
    GtkCellRendererClass parent_class;
};

/* Public interface {{{1 */

GtkCellRenderer *ph_cell_renderer_sprite_new(PHSprite sprite,
                                             PHSpriteSize size);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
