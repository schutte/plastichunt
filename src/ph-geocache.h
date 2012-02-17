/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_GEOCACHE_H
#define PH_GEOCACHE_H

/* Includes {{{1 */

#include "ph-database.h"
#include <glib.h>
#include <glib-object.h>

/* Listing sites {{{1 */

/*
 * Known geocache listing websites.
 */
typedef enum _PHGeocacheSite {
    PH_GEOCACHE_SITE_UNKNOWN,
    PH_GEOCACHE_SITE_GC_COM,
    PH_GEOCACHE_SITE_OC_DE,
    PH_GEOCACHE_SITE_COUNT
} PHGeocacheSite;

#define PH_GEOCACHE_SITE_PREFIX_LENGTH 2
const gchar *ph_geocache_site_prefix(PHGeocacheSite site);

/* Geocache types {{{1 */

/*
 * Known types of geocaches.
 */
typedef enum _PHGeocacheType {
    PH_GEOCACHE_TYPE_UNKNOWN,
    PH_GEOCACHE_TYPE_TRADITIONAL,
    PH_GEOCACHE_TYPE_MULTI,
    PH_GEOCACHE_TYPE_MYSTERY,
    PH_GEOCACHE_TYPE_LETTERBOX,
    PH_GEOCACHE_TYPE_WHERIGO,
    PH_GEOCACHE_TYPE_EVENT,
    PH_GEOCACHE_TYPE_MEGA_EVENT,
    PH_GEOCACHE_TYPE_CITO,
    PH_GEOCACHE_TYPE_EARTH,
    PH_GEOCACHE_TYPE_VIRTUAL,
    PH_GEOCACHE_TYPE_WEBCAM,
    PH_GEOCACHE_TYPE_COUNT
} PHGeocacheType;

/* Geocache sizes {{{1 */

/*
 * Known geocache container sizes.
 */
typedef enum _PHGeocacheSize {
    PH_GEOCACHE_SIZE_UNKNOWN,
    PH_GEOCACHE_SIZE_UNSPECIFIED,
    PH_GEOCACHE_SIZE_MICRO,
    PH_GEOCACHE_SIZE_SMALL,
    PH_GEOCACHE_SIZE_REGULAR,
    PH_GEOCACHE_SIZE_LARGE,
    PH_GEOCACHE_SIZE_VIRTUAL,
    PH_GEOCACHE_SIZE_OTHER,
    PH_GEOCACHE_SIZE_COUNT
} PHGeocacheSize;

/* Geocache attributes {{{1 */

/*
 * Geocache attributes as found in Groundspeak's so-called GPX 1.0.1 files.
 */
typedef enum _PHGeocacheAttrID {
    PH_GEOCACHE_ATTR_UNKNOWN = 0,
    PH_GEOCACHE_ATTR_DOGS = 1,
    PH_GEOCACHE_ATTR_FEE = 2,
    PH_GEOCACHE_ATTR_CLIMBING_GEAR = 3,
    PH_GEOCACHE_ATTR_BOAT = 4,
    PH_GEOCACHE_ATTR_SCUBA_GEAR = 5,
    PH_GEOCACHE_ATTR_KIDS = 6,
    PH_GEOCACHE_ATTR_ONE_HOUR = 7,
    PH_GEOCACHE_ATTR_SCENIC = 8,
    PH_GEOCACHE_ATTR_HIKE = 9,
    PH_GEOCACHE_ATTR_CLIMBING = 10,
    PH_GEOCACHE_ATTR_WADING = 11,
    PH_GEOCACHE_ATTR_SWIMMING = 12,
    PH_GEOCACHE_ATTR_ALWAYS = 13,
    PH_GEOCACHE_ATTR_NIGHT = 14,
    PH_GEOCACHE_ATTR_WINTER = 15,
    PH_GEOCACHE_ATTR_UNUSED_1 = 16,
    PH_GEOCACHE_ATTR_POISONOUS = 17,
    PH_GEOCACHE_ATTR_DANGER_ANIMALS = 18,
    PH_GEOCACHE_ATTR_TICKS = 19,
    PH_GEOCACHE_ATTR_MINES = 20,
    PH_GEOCACHE_ATTR_CLIFF = 21,
    PH_GEOCACHE_ATTR_HUNTING = 22,
    PH_GEOCACHE_ATTR_DANGER_AREA = 23,
    PH_GEOCACHE_ATTR_WHEELCHAIR = 24,
    PH_GEOCACHE_ATTR_PARKING = 25,
    PH_GEOCACHE_ATTR_PUBLIC_TRANSPORT = 26,
    PH_GEOCACHE_ATTR_DRINKING_WATER = 27,
    PH_GEOCACHE_ATTR_RESTROOMS = 28,
    PH_GEOCACHE_ATTR_TELEPHONE = 29,
    PH_GEOCACHE_ATTR_PICNIC_TABLES = 30,
    PH_GEOCACHE_ATTR_CAMPING = 31,
    PH_GEOCACHE_ATTR_BICYCLES = 32,
    PH_GEOCACHE_ATTR_MOTORCYCLES = 33,
    PH_GEOCACHE_ATTR_QUADS = 34,
    PH_GEOCACHE_ATTR_OFFROAD = 35,
    PH_GEOCACHE_ATTR_SNOWMOBILES = 36,
    PH_GEOCACHE_ATTR_HORSES = 37,
    PH_GEOCACHE_ATTR_CAMPFIRES = 38,
    PH_GEOCACHE_ATTR_THORNS = 39,
    PH_GEOCACHE_ATTR_STEALTH = 40,
    PH_GEOCACHE_ATTR_STROLLER = 41,
    PH_GEOCACHE_ATTR_MAINTENANCE = 42,
    PH_GEOCACHE_ATTR_LIVESTOCK = 43,
    PH_GEOCACHE_ATTR_FLASHLIGHT = 44,
    PH_GEOCACHE_ATTR_LOST_AND_FOUND = 45,
    PH_GEOCACHE_ATTR_RV = 46,
    PH_GEOCACHE_ATTR_FIELD_PUZZLE = 47,
    PH_GEOCACHE_ATTR_UV = 48,
    PH_GEOCACHE_ATTR_SNOWSHOES = 49,
    PH_GEOCACHE_ATTR_XC_SKIS = 50,
    PH_GEOCACHE_ATTR_SPECIAL_TOOL = 51,
    PH_GEOCACHE_ATTR_NIGHT_CACHE = 52,
    PH_GEOCACHE_ATTR_PARK_AND_GRAB = 53,
    PH_GEOCACHE_ATTR_ABANDONED = 54,
    PH_GEOCACHE_ATTR_SHORT_HIKE = 55,
    PH_GEOCACHE_ATTR_MEDIUM_HIKE = 56,
    PH_GEOCACHE_ATTR_LONG_HIKE = 57,
    PH_GEOCACHE_ATTR_FUEL = 58,
    PH_GEOCACHE_ATTR_FOOD = 59,
    PH_GEOCACHE_ATTR_BEACON = 60,
    PH_GEOCACHE_ATTR_COUNT
} PHGeocacheAttrID;

typedef struct _PHGeocacheAttrs PHGeocacheAttrs;

/*
 * Geocache attribute settings as a singly-linked list.
 */
struct _PHGeocacheAttrs {
    PHGeocacheAttrs *next;      /* rest pointer */
    PHGeocacheAttrID id;        /* number of the attribute */
    gboolean value;             /* true or false? */
};

PHGeocacheAttrs *ph_geocache_attrs_from_string(const gchar *input);
gchar *ph_geocache_attrs_to_string(const PHGeocacheAttrs *attrs);
PHGeocacheAttrs *ph_geocache_attrs_find(PHGeocacheAttrs *attrs,
        PHGeocacheAttrID id);
PHGeocacheAttrs *ph_geocache_attrs_set(PHGeocacheAttrs *attrs,
        PHGeocacheAttrID id, gboolean value);
PHGeocacheAttrs *ph_geocache_attrs_prepend(PHGeocacheAttrs *attrs,
        PHGeocacheAttrID id, gboolean value);
PHGeocacheAttrs *ph_geocache_attrs_unset(PHGeocacheAttrs *attrs,
        PHGeocacheAttrID id);
void ph_geocache_attrs_free(PHGeocacheAttrs *attrs);

/* Data structures {{{1 */

/*
 * Mutable information about a geocache, stored in the "geocache_notes" table.
 */
typedef struct _PHGeocacheNote {
    gchar *id;                  /* waypoint ID or NULL if note wasn't loaded */
    gboolean found;             /* has the user marked the cache found? */
    gchar *note;                /* custom text */
} PHGeocacheNote;

#define PH_TYPE_GEOCACHE_NOTE (ph_geocache_note_get_type())
GType ph_geocache_note_get_type();

/*
 * Representation of a row in the "geocaches" table.
 */
typedef struct _PHGeocache {
    gchar *id;                  /* waypoint ID of the geocache */
    gchar *name;                /* human-readable name */
    gchar *creator;             /* user who placed it */
    gchar *owner;               /* current owner */
    PHGeocacheType type;        /* type of the geocache */
    PHGeocacheSize size;        /* container size */
    guint8 difficulty;          /* D rating times 10 */
    guint8 terrain;             /* T rating times 10 */
    PHGeocacheAttrs *attributes; /* list of geocache attributes */
    gboolean summary_html;      /* is summary in HTML format? */
    gchar *summary;             /* short description */
    gboolean description_html;  /* is description in HTML format? */
    gchar *description;         /* long description */
    gchar *hint;                /* hint (decoded) */
    gboolean logged;            /* has the user logged the geocache? */
    gboolean available;         /* is the geocache active? */
    gboolean archived;          /* has it been permanently archived? */
    PHGeocacheNote note;        /* user-provided information */
} PHGeocache;

/* Public interface {{{1 */

PHGeocache *ph_geocache_load_by_id(PHDatabase *database,
                                   const gchar *id,
                                   gboolean full,
                                   GError **error);
gboolean ph_geocache_store(const PHGeocache *geocache,
                           PHDatabase *database,
                           GError **error);
void ph_geocache_free(PHGeocache *geocache);

PHGeocacheNote *ph_geocache_note_copy(const PHGeocacheNote *note);
gboolean ph_geocache_note_store(const PHGeocacheNote *note,
                                PHDatabase *database,
                                GError **error);
void ph_geocache_note_free(PHGeocacheNote *note);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
