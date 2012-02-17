/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_GEO_H
#define PH_GEO_H

/* Includes {{{1 */

#include <glib.h>

/* Coordinate conversion {{{1 */

/*
 * Convert between degrees and 1/1000s of minutes.
 */
#define PH_GEO_DEG_TO_MINFRAC(deg) ((gint) ((deg) * 60000))
#define PH_GEO_MINFRAC_TO_DEG(minfrac) ((minfrac) / 60000.0)

/*
 * Limits of the world in degrees.
 */
#define PH_GEO_MAX_NORTH_DEG 90.0
#define PH_GEO_MAX_SOUTH_DEG -90.0
#define PH_GEO_MAX_EAST_DEG 180.0
#define PH_GEO_MAX_WEST_DEG -180.0

/*
 * And in 1/1000s of minutes.
 */
#define PH_GEO_MAX_NORTH_MINFRAC 5400000
#define PH_GEO_MAX_SOUTH_MINFRAC -5400000
#define PH_GEO_MAX_EAST_MINFRAC 10800000
#define PH_GEO_MAX_WEST_MINFRAC -10800000

/*
 * Ensure that a coordinate is within the limits.
 */
#define PH_GEO_CLAMP_LATITUDE_DEG(deg) \
	CLAMP((deg), PH_GEO_MAX_SOUTH_DEG, PH_GEO_MAX_NORTH_DEG)
#define PH_GEO_CLAMP_LONGITUDE_DEG(deg) \
	CLAMP((deg), PH_GEO_MAX_WEST_DEG, PH_GEO_MAX_EAST_DEG)
#define PH_GEO_CLAMP_LATITUDE_MINFRAC(minfrac) \
	CLAMP((minfrac), PH_GEO_MAX_SOUTH_MINFRAC, PH_GEO_MAX_NORTH_MINFRAC)
#define PH_GEO_CLAMP_LONGITUDE_MINFRAC(minfrac) \
	CLAMP((minfrac), PH_GEO_MAX_WEST_MINFRAC, PH_GEO_MAX_EAST_MINFRAC)

/* Coordinate presentation {{{1 */

gchar *ph_geo_latitude_deg_to_string(gdouble latitude);
gchar *ph_geo_longitude_deg_to_string(gdouble longitude);
gchar *ph_geo_deg_to_string(gdouble longitude,
                            gdouble latitude);
gchar *ph_geo_minfrac_to_string(gint longitude,
                                gint latitude);

/* Coordinate parsing {{{1 */

gdouble ph_geo_latitude_string_to_deg(const gchar *string);
gdouble ph_geo_longitude_string_to_deg(const gchar *string);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
