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

#include "ph-geo.h"
#include <glib/gi18n.h>
#include <stdlib.h>
#include <langinfo.h>

/* Forward declarations {{{1 */

static gdouble ph_geo_string_to_deg(const gchar *string, const gchar *positive,
                                    const gchar *negative);

/* Coordinate presentation {{{1 */

/*
 * Create a human-readable string showing the latitude of a waypoint.  The
 * caller is responsible for freeing the return value.
 */
gchar *
ph_geo_latitude_deg_to_string(gdouble latitude)
{
    gint degrees = (gint) latitude;
    gdouble minutes = (latitude - degrees) * 60;

    return g_strdup_printf("%s %d° %.03f",
            (latitude < 0) ? _("S") : _("N"),
            degrees, minutes);
}

/*
 * As above, but for latitude.
 */
gchar *
ph_geo_longitude_deg_to_string(gdouble longitude)
{
    gint degrees = (gint) longitude;
    gdouble minutes = (longitude - degrees) * 60;

    return g_strdup_printf("%s %d° %.03f",
            (longitude < 0) ? _("W") : _("E"),
            degrees, minutes);
}

/*
 * As above, but show both latitude and longitude, separated by a comma.
 */
gchar *
ph_geo_deg_to_string(gdouble longitude,
                     gdouble latitude)
{
    gchar *lonstr, *latstr;
    gchar *result;

    lonstr = ph_geo_longitude_deg_to_string(longitude);
    latstr = ph_geo_latitude_deg_to_string(latitude);

    result = g_strdup_printf("%s, %s", latstr, lonstr);

    g_free(lonstr);
    g_free(latstr);

    return result;
}

/*
 * Convenience function if the coordinates exist in 1/1000s of minutes.
 */
gchar *
ph_geo_minfrac_to_string(gint longitude,
                         gint latitude)
{
    return ph_geo_deg_to_string(
            PH_GEO_MINFRAC_TO_DEG(longitude),
            PH_GEO_MINFRAC_TO_DEG(latitude));
}

/* Coordinate parsing {{{1 */

/*
 * Parse a string like "N 12° 34.567'" to a decimal number.
 */
static gdouble
ph_geo_string_to_deg(const gchar *string,
                     const gchar *pos_string,
                     const gchar *neg_string)
{
    static GRegex *number_regex = NULL, *direction_regex = NULL;
    GMatchInfo *match_info = NULL;
    gdouble numbers[3] = {0, 0, 0};
    gdouble result;
    gint number_count = 0;
    gboolean negative = FALSE;

    if (number_regex == NULL) {
        const char *radix = nl_langinfo(RADIXCHAR);     /* decimal separator */
        gchar *regex = g_strdup_printf("[%c.[:digit:]]+", radix[0]);
        number_regex = g_regex_new(regex, 0, 0, NULL);
        g_free(regex);
        g_assert(number_regex != NULL);

        regex = g_strdup_printf("[^%c[:digit:][:punct:][:space:]]+", radix[0]);
        direction_regex = g_regex_new(regex, 0, 0, NULL);
        g_free(regex);
        g_assert(direction_regex != NULL);
    }

    g_regex_match(number_regex, string, 0, &match_info);
    while (g_match_info_matches(match_info)) {
        gchar *match = g_match_info_fetch(match_info, 0);
        numbers[number_count++] = strtod(match, NULL);
        g_free(match);
        if (number_count == 3)
            break;
        g_match_info_next(match_info, NULL);
    }
    g_match_info_free(match_info);

    g_regex_match(direction_regex, string, 0, &match_info);
    while (g_match_info_matches(match_info)) {
        gchar *match = g_match_info_fetch(match_info, 0);
        if (strcasecmp(pos_string, match) == 0)
            break;
        else if (strcasecmp(neg_string, match) == 0) {
            negative = TRUE;
            break;
        }
        g_free(match);
        g_match_info_next(match_info, NULL);
    }
    g_match_info_free(match_info);

    result = numbers[0];
    if (number_count > 1)
        result += numbers[1] / 60;
    if (number_count > 2)
        result += numbers[2] / 3600;

    return negative ? -result : result;
}

/*
 * Parse a latitude string.
 */
gdouble
ph_geo_latitude_string_to_deg(const gchar *string)
{
    return ph_geo_string_to_deg(string, _("N"), _("S"));
}

/*
 * Parse a longitude string.
 */

gdouble
ph_geo_longitude_string_to_deg(const gchar *string)
{
    return ph_geo_string_to_deg(string, _("E"), _("W"));
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
