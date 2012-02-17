/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_GPX_H
#define PH_GPX_H

/* Includes {{{1 */

#include "ph-geocache.h"
#include "ph-log.h"
#include "ph-waypoint.h"
#include "ph-xml.h"

/* Textual values {{{1 */

/*
 * Map strings to PHWaypointType values.
 */
static const PHXmlStringTable ph_gpx_waypoint_types[] = {
    { PH_WAYPOINT_TYPE_GEOCACHE, FALSE, "Geocache", "^geocache" },
    { PH_WAYPOINT_TYPE_TRAILHEAD, TRUE, "Trailhead", NULL },
    { PH_WAYPOINT_TYPE_REFERENCE, FALSE, "Reference Point", "^reference" },
    { PH_WAYPOINT_TYPE_QUESTION, FALSE, "Question to Answer", "^question" },
    { PH_WAYPOINT_TYPE_STAGE, FALSE, "Stages of a Multicache", "^stage" },
    { PH_WAYPOINT_TYPE_FINAL, FALSE, "Final Location", "^final" },
    { PH_WAYPOINT_TYPE_PARKING, FALSE, "Parking Area", "^parking" },
    { 0 }
};

/*
 * Map strings to PHGeocacheType values.
 */
static const PHXmlStringTable ph_gpx_geocache_types[] = {
    { PH_GEOCACHE_TYPE_TRADITIONAL, FALSE, "Traditional Cache", "^tradi" },
    { PH_GEOCACHE_TYPE_MULTI, FALSE, "Multi-cache", "^multi" },
    { PH_GEOCACHE_TYPE_MYSTERY, FALSE, "Unknown Cache", "^unknown" },
    { PH_GEOCACHE_TYPE_MYSTERY, FALSE, NULL, "^other" },
    { PH_GEOCACHE_TYPE_LETTERBOX, FALSE, "Letterbox Hybrid", "^letterbox" },
    { PH_GEOCACHE_TYPE_WHERIGO, FALSE, "Wherigo Cache", "^wherigo" },
    { PH_GEOCACHE_TYPE_EVENT, FALSE, "Event Cache", "^event" },
    { PH_GEOCACHE_TYPE_MEGA_EVENT, FALSE, "Mega-Event Cache", "^mega-event" },
    { PH_GEOCACHE_TYPE_CITO, TRUE, "Cache In Trash Out Event", NULL },
    { PH_GEOCACHE_TYPE_EARTH, FALSE, "Earthcache", "^earth" },
    { PH_GEOCACHE_TYPE_VIRTUAL, FALSE, "Virtual Cache", "^virtual" },
    { PH_GEOCACHE_TYPE_WEBCAM, FALSE, "Webcam Cache", "^webcam" },
    { 0 }
};

/*
 * Map strings to PHGeocacheSize values.
 */
static const PHXmlStringTable ph_gpx_geocache_sizes[] = {
    { PH_GEOCACHE_SIZE_UNSPECIFIED, TRUE, "Not chosen", "=unknown" },
    { PH_GEOCACHE_SIZE_MICRO, TRUE, "Micro", NULL },
    { PH_GEOCACHE_SIZE_SMALL, TRUE, "Small", NULL },
    { PH_GEOCACHE_SIZE_REGULAR, TRUE, "Regular", NULL },
    { PH_GEOCACHE_SIZE_LARGE, TRUE, "Large", NULL },
    { PH_GEOCACHE_SIZE_VIRTUAL, TRUE, "Virtual", NULL },
    { PH_GEOCACHE_SIZE_OTHER, TRUE, "Other", NULL },
    { 0 }
};

/*
 * Map strings to PHLogType values.
 */
static const PHXmlStringTable ph_gpx_log_types[] = {
    { PH_LOG_TYPE_FOUND, FALSE, "Found it", "^found" },
    { PH_LOG_TYPE_NOT_FOUND, TRUE, "Didn't find it", "=not found" },
    { PH_LOG_TYPE_REVIEWER, TRUE, "Post Reviewer Note", NULL },
    { PH_LOG_TYPE_NOTE, FALSE, "Write Note", "/note" },
    { PH_LOG_TYPE_PUBLISH, TRUE, "Publish Listing", NULL },
    { PH_LOG_TYPE_ENABLE, TRUE, "Enable Listing", NULL },
    { PH_LOG_TYPE_DISABLE, FALSE, "Temporarily Disable Listing", "/disable" },
    { PH_LOG_TYPE_UPDATE, TRUE, "Update Coordinates", NULL },
    { PH_LOG_TYPE_WILL_ATTEND, TRUE, "Will Attend", NULL },
    { PH_LOG_TYPE_ATTENDED, TRUE, "Attended", NULL },
    { PH_LOG_TYPE_WEBCAM, TRUE, "Webcam Photo taken", NULL },
    { PH_LOG_TYPE_NEEDS_MAINTENANCE, TRUE, "Needs Maintenance", NULL },
    { PH_LOG_TYPE_MAINTENANCE, TRUE, "Owner Maintenance", NULL },
    { PH_LOG_TYPE_NEEDS_ARCHIVING, TRUE, "Needs Archived", NULL },
    { PH_LOG_TYPE_ARCHIVED, TRUE, "Archive", NULL },
    { PH_LOG_TYPE_UNARCHIVED, TRUE, "Unarchive", "=archive (show)" },
    { 0 }
};

/*
 * Map strings to PHGeocacheSite values.
 */
static const PHXmlStringTable ph_gpx_geocache_sites[] = {
    { PH_GEOCACHE_SITE_GC_COM, TRUE, "Groundspeak", "/geocaching.com" },
    { PH_GEOCACHE_SITE_OC_DE, TRUE, "Opencaching.de", NULL },
    { 0 }
};

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
