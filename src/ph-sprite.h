/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_SPRITE_H
#define PH_SPRITE_H

/* Includes {{{1 */

#include "ph-geocache.h"
#include <cairo.h>

/* Types {{{1 */

/*
 * Sprite types.
 */
typedef enum {
    PH_SPRITE_GEOCACHE,         /* geocache type */
    PH_SPRITE_SIZE,             /* container size */
    PH_SPRITE_DIFFICULTY,       /* difficulty rating */
    PH_SPRITE_TERRAIN,          /* terrain rating */
    PH_SPRITE_WAYPOINT,         /* waypoint type */
    PH_SPRITE_COUNT
} PHSprite;

/*
 * Extra values that can be ORed for PH_SPRITE_GEOCACHE sprites.
 */
enum {
    PH_SPRITE_GEOCACHE_FIRST = 0x100,
    PH_SPRITE_GEOCACHE_UNAVAILABLE = 0x100, /* temporarily unavailable */
    PH_SPRITE_GEOCACHE_ARCHIVED = 0x200,    /* permanently unavailable */
    PH_SPRITE_GEOCACHE_NOTES = 0x400,       /* user notes available */
    PH_SPRITE_GEOCACHE_FOUND = 0x800,       /* geocache found, but not logged */
    PH_SPRITE_GEOCACHE_LOGGED = 0x1000,     /* geocache found and logged */
    PH_SPRITE_GEOCACHE_LAST = 0x1000
};

/* Sizes {{{1 */

/*
 * Possible size values for sprites.
 */
typedef enum {
    PH_SPRITE_SIZE_TINY = 0,    /* height: 8px */
    PH_SPRITE_SIZE_SMALL = 1,   /* 16px */
    PH_SPRITE_SIZE_MEDIUM = 2,  /* 32px */
    PH_SPRITE_SIZE_LARGE = 3,   /* 64px */
    PH_SPRITE_SIZE_COUNT
} PHSpriteSize;

/* Public interface {{{1 */

gdouble ph_sprite_get_scale(PHSprite sprite,
                            PHSpriteSize size);
void ph_sprite_get_dimensions(PHSprite sprite,
                              PHSpriteSize size,
                              gint *width,
                              gint *height);
void ph_sprite_draw(PHSprite sprite,
                    PHSpriteSize size,
                    guint value,
                    cairo_t *cairo,
                    gdouble alpha,
                    gdouble x,
                    gdouble y,
                    gint *width,
                    gint *height);

guint ph_sprite_value_for_geocache(const PHGeocache *geocache);
guint ph_sprite_value_for_geocache_details(PHGeocacheType type,
                                           gboolean found,
                                           gboolean logged,
                                           gboolean available,
                                           gboolean archived,
                                           gboolean note);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
