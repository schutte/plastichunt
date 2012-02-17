/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#ifndef PH_XML_H
#define PH_XML_H

/* Includes {{{1 */

#include <glib.h>
#include <libxml/xmlreader.h>

/* Mapping strings to numbers {{{1 */

typedef struct _PHXmlStringTable {
    gint value;             /* numeric value */
    gboolean check_primary; /* while searching, check if needle == primary? */
    const gchar *primary;   /* canonical string value */
    const gchar *alt;       /* alternative string value to look for; alt[0] =
                             * '^': alt is prefix,
                             * '/': alt can be anywhere in needle,
                             * '=': alt should be == needle */
} PHXmlStringTable;

gint ph_xml_find_string(const PHXmlStringTable *haystack,
                        const xmlChar *needle);

/* Processing XML text {{{1 */

gboolean ph_xml_extract_text(xmlTextReaderPtr reader,
                             xmlChar **result,
                             GError **error);
gboolean ph_xml_extract_double(xmlTextReaderPtr reader,
                               gdouble *result,
                               GError **error);
gboolean ph_xml_extract_value(xmlTextReaderPtr reader,
                              const PHXmlStringTable *haystack,
                              gint *result,
                              GError **error);
gboolean ph_xml_extract_time(xmlTextReaderPtr reader,
                             glong *result,
                             GError **error);

/* Processing XML attributes {{{1 */

gboolean ph_xml_attrib_compare(xmlTextReaderPtr reader,
                               const xmlChar *attrib,
                               const xmlChar *value);
gboolean ph_xml_attrib_text(xmlTextReaderPtr reader,
                            const xmlChar *attrib,
                            xmlChar **result,
                            GError **error);
gboolean ph_xml_attrib_int(xmlTextReaderPtr reader,
                           const xmlChar *attrib,
                           gint *result,
                           GError **error);
gboolean ph_xml_attrib_double(xmlTextReaderPtr reader,
                              const xmlChar *attrib,
                              gdouble *result,
                              GError **error);

/* Error reporting {{{1 */

#define PH_XML_ERROR (ph_xml_error_quark())
GQuark ph_xml_error_quark();
typedef enum {
    PH_XML_ERROR_PARSE
} PHXmlError;

gboolean ph_xml_set_last_error(GError **error);

/* }}} */

#endif

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
