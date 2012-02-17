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

#include "ph-common.h"
#include "ph-sprite.h"
#include "ph-util.h"
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>
#include <glib/gstdio.h>
#include <math.h>

/* Forward declarations {{{1 */
static cairo_surface_t *ph_sprite_render(PHSprite sprite, gdouble scale);

static void ph_sprite_draw_single(
    cairo_t *cairo, cairo_surface_t *surface, PHSprite sprite,
    guint value, gdouble alpha, gdouble x, gdouble y, gint width, gint height);

/* Filenames {{{1 */

/*
 * Sprite filenames.
 */
static const gchar *ph_sprite_filenames[PH_SPRITE_COUNT] = {
    "geocache.svg",     /* PH_SPRITE_GEOCACHE */
    "size.svg",         /* PH_SPRITE_SIZE */
    "difficulty.svg",   /* PH_SPRITE_DIFFICULTY */
    "terrain.svg",      /* PH_SPRITE_TERRAIN */
    "waypoint.svg"      /* PH_SPRITE_WAYPOINT */
};

/* Sprite cache {{{1 */

/*
 * Loaded sprites.
 */
static cairo_surface_t *ph_sprites[4][PH_SPRITE_COUNT] = {
    { NULL }, { NULL }, { NULL }, { NULL }
};

/* Scaling and dimensions {{{1 */

/*
 * Get the scaling factor if the sprite is rendered at the specified size.
 */
gdouble
ph_sprite_get_scale(PHSprite sprite,
                    PHSpriteSize size)
{
    switch (sprite) {
    case PH_SPRITE_GEOCACHE:
    case PH_SPRITE_WAYPOINT:
        return pow(2, size - 1);
    default:
        return pow(2, size);
    }
}

/*
 * Get the pixel measurements of a sprite at a given size.
 */
void
ph_sprite_get_dimensions(PHSprite sprite,
                         PHSpriteSize size,
                         gint *width,
                         gint *height)
{
    gint w, h;
    gdouble scale = ph_sprite_get_scale(sprite, size);

    switch (sprite) {
    case PH_SPRITE_GEOCACHE:
    case PH_SPRITE_WAYPOINT:
        w = h = 16;
        break;
    case PH_SPRITE_SIZE:
    case PH_SPRITE_DIFFICULTY:
    case PH_SPRITE_TERRAIN:
        w = 29; h = 8;
        break;
    default:
        w = h = 0;
        g_warning("Unknown sprite type: %d", sprite);
    }

    w *= scale; h *= scale;

    if (width != NULL) *width = w;
    if (height != NULL) *height = h;
}

/* Rendering SVG {{{1 */

/*
 * Create a sprite for the given file using the rsvg library.
 */
static cairo_surface_t *
ph_sprite_render(PHSprite sprite,
                 gdouble scale)
{
    const gchar *filename = ph_sprite_filenames[sprite];
    gchar *path;
    FILE *fin;
    cairo_surface_t *result = NULL;

    path = ph_util_find_data_file(PH_SPRITES_LOCATION, filename);
    fin = g_fopen(path, "rb");
    g_free(path);

    if (fin != NULL) {
        gboolean success = TRUE;
        RsvgHandle *rsvg = rsvg_handle_new();
        guchar buf[1024];
        gsize nbytes;

        while (success && !feof(fin)) {
            nbytes = fread(buf, sizeof(buf[0]), sizeof(buf) / sizeof(buf[0]),
                    fin);
            if (!rsvg_handle_write(rsvg, buf, nbytes, NULL))
                success = FALSE;
        }

        fclose(fin);
        if (!rsvg_handle_close(rsvg, NULL))
            success = FALSE;

        if (success) {
            cairo_t *cr;
            RsvgDimensionData dim;

            rsvg_handle_get_dimensions(rsvg, &dim);
            result = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                    dim.width * scale, dim.height * scale);
            cr = cairo_create(result);
            cairo_scale(cr, scale, scale);

            if (!rsvg_handle_render_cairo(rsvg, cr)) {
                cairo_surface_destroy(result);
                result = NULL;
            }

            cairo_destroy(cr);
        }

        rsvg_handle_free(rsvg);
    }

    return result;
}

/* Drawing {{{1 */

/*
 * Draw a sprite in a certain size at (x, y) via the specified cairo context and
 * at the given opacity.  value is handled depending on the value of sprite; it
 * is expected to be rating times 10 for difficulty and terrain and an
 * appropriate enum value otherwise.  For PH_SPRITE_GEOCACHE, some extra bits
 * are recognized, see the enum in the header file.  The width and height of the
 * sprite are returned in the eponymous arguments.
 */
void
ph_sprite_draw(PHSprite sprite,
               PHSpriteSize size,
               guint value,
               cairo_t *cairo,
               gdouble alpha,
               gdouble x,
               gdouble y,
               gint *width,
               gint *height)
{
    cairo_surface_t *surface;
    gdouble scale = ph_sprite_get_scale(sprite, size);
    gint w, h;

    ph_sprite_get_dimensions(sprite, size, &w, &h);

    surface = ph_sprites[size][sprite];
    if (surface == NULL)
        ph_sprites[size][sprite] = surface = ph_sprite_render(sprite, scale);

    if (surface == NULL)
        return;

    ph_sprite_draw_single(cairo, surface, sprite, value & 0xff,
            alpha, x, y, w, h);
    if (sprite == PH_SPRITE_GEOCACHE) {
        gint i;
        for (i = PH_SPRITE_GEOCACHE_FIRST; i <= PH_SPRITE_GEOCACHE_LAST;
                i <<= 1) {
            if ((value & i) != 0)
                ph_sprite_draw_single(cairo, surface, sprite, i,
                        alpha, x, y, w, h);
        }
    }

    if (width != NULL) *width = w;
    if (height != NULL) *height = h;
}

/*
 * Draw a single sprite from a surface to a cairo context.
 */
static void
ph_sprite_draw_single(cairo_t *cairo,
                      cairo_surface_t *surface,
                      PHSprite sprite,
                      guint value,
                      gdouble alpha,
                      gdouble x,
                      gdouble y,
                      gint width,
                      gint height)
{
    gint xoff = 0, yoff = 0;

    switch (sprite) {
    case PH_SPRITE_GEOCACHE:
        if (value < 0x100)
            xoff = width * value;
        else {
            yoff = height;
            value >>= 8;
            while ((value & 1) == 0) {
                value >>= 1;
                xoff += width;
            }
        }
        break;
    case PH_SPRITE_WAYPOINT:
        xoff = width * value;
        break;
    case PH_SPRITE_SIZE:
        yoff = MAX(0, height * ((gint) value - 1));
        break;
    case PH_SPRITE_DIFFICULTY:
    case PH_SPRITE_TERRAIN:
        yoff = height * (value - 10) / 5;
        break;
    default:
        return;
    }

    cairo_set_source_surface(cairo, surface, x - xoff, y - yoff);
    cairo_rectangle(cairo, x, y, width, height);
    cairo_clip(cairo);
    cairo_paint_with_alpha(cairo, alpha);
    cairo_reset_clip(cairo);
}

/* Value calculation {{{1 */

/*
 * Calculate the sprite value for a geocache.
 */
guint
ph_sprite_value_for_geocache(const PHGeocache *geocache)
{
    gboolean note = FALSE;

    if (geocache->note.id != NULL)
        note = (geocache->note.note != NULL);

    return ph_sprite_value_for_geocache_details(geocache->type,
            geocache->note.found, geocache->logged, geocache->available,
            geocache->archived, note);
}

/*
 * Calculate the sprite value for a geocache given its relevant details.
 */
guint
ph_sprite_value_for_geocache_details(PHGeocacheType type,
                                     gboolean found,
                                     gboolean logged,
                                     gboolean available,
                                     gboolean archived,
                                     gboolean note)
{
    guint result = type;

    if (found && !logged)
        result |= PH_SPRITE_GEOCACHE_FOUND;
    else if (logged)
        result |= PH_SPRITE_GEOCACHE_LOGGED;
    if (!available)
        result |= PH_SPRITE_GEOCACHE_UNAVAILABLE;
    if (archived)
        result |= PH_SPRITE_GEOCACHE_ARCHIVED;
    if (note)
        result |= PH_SPRITE_GEOCACHE_NOTES;

    return result;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
