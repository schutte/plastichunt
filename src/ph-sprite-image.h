/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_SPRITE_IMAGE_H
#define PH_SPRITE_IMAGE_H

/* Includes {{{1 */

#include "ph-sprite.h"
#include <gtk/gtk.h>

/* GObject boilerplate {{{1 */

#define PH_TYPE_SPRITE_IMAGE (ph_sprite_image_get_type())
#define PH_SPRITE_IMAGE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), PH_TYPE_SPRITE_IMAGE, PHSpriteImage))
#define PH_IS_SPRITE_IMAGE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), PH_TYPE_SPRITE_IMAGE))
#define PH_SPRITE_IMAGE_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST((cls), PH_TYPE_SPRITE_IMAGE, PHSpriteImageClass))
#define PH_IS_SPRITE_IMAGE_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE((cls), PH_TYPE_SPRITE_IMAGE))
#define PH_SPRITE_IMAGE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), PH_TYPE_SPRITE_IMAGE, PHSpriteImageClass))

GType ph_sprite_image_get_type();

/* Instance and class structure {{{1 */

typedef struct _PHSpriteImage PHSpriteImage;
typedef struct _PHSpriteImageClass PHSpriteImageClass;
typedef struct _PHSpriteImagePrivate PHSpriteImagePrivate;

struct _PHSpriteImage {
    GtkWidget parent;
    PHSpriteImagePrivate *priv;
};

struct _PHSpriteImageClass {
    GtkWidgetClass parent_class;
};

/* Public interface {{{1 */

GtkWidget *ph_sprite_image_new(PHSprite sprite, PHSpriteSize size);

PHSprite ph_sprite_image_get_sprite(PHSpriteImage *image);
void ph_sprite_image_set_sprite(PHSpriteImage *image,
                                PHSprite sprite);
PHSpriteSize ph_sprite_image_get_size(PHSpriteImage *image);
void ph_sprite_image_set_size(PHSpriteImage *image,
                              PHSpriteSize size);
guint ph_sprite_image_get_value(PHSpriteImage *image);
void ph_sprite_image_set_value(PHSpriteImage *image,
                               guint value);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
