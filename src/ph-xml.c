/*
 * plastichunt: desktop geocaching browser
 *
 * Copyright © 2012 Michael Schutte <michi@uiae.at>
 *
 * This program is free software.  You are permitted to use, copy, modify and
 * redistribute it according to the terms of the MIT License.  See the COPYING
 * file for details.
 */

#include "ph-xml.h"
#include <glib/gi18n.h>

/* Forward declarations {{{1 */

static gboolean ph_xml_string_starts(const gchar *haystack,
                                     const gchar *needle);

/* Mapping strings to numbers {{{1 */

/*
 * Check if the haystack string starts with the needle string.  This function
 * is case-insensitive and requires that needle is all lowercase.
 */
static gboolean
ph_xml_string_starts(const gchar *haystack,
                     const gchar *needle)
{
    int i;
    for (i = 0; needle[i] != '\0' && haystack[i] != '\0'; ++i) {
        if (needle[i] != g_ascii_tolower(haystack[i]))
            return FALSE;
    }
    return (needle[i] == '\0' || needle[i] == haystack[i]);
}

/*
 * Try to match needle with primary and alternative strings from haystack to
 * obtain the integer value of the desired property.  Returns 0 if needle
 * cannot be found.
 */
gint
ph_xml_find_string(const PHXmlStringTable *haystack,
                   const xmlChar *needle)
{
    const PHXmlStringTable *cur;

    for (cur = haystack; cur->value != 0; ++cur) {
        if (cur->check_primary &&
                xmlStrcasecmp(needle, (xmlChar *) cur->primary) == 0)
            return cur->value;

        if (cur->alt == NULL)
            continue;
        switch (cur->alt[0]) {
        case '=':
            if (xmlStrcasecmp(needle, (xmlChar *) (cur->alt + 1)) == 0)
                return cur->value;
            break;
        case '/':
            if (xmlStrcasestr(needle, (xmlChar *) (cur->alt + 1)) != NULL)
                return cur->value;
            break;
        case '^':
            if (ph_xml_string_starts((gchar *) needle, cur->alt + 1))
                return cur->value;
        }
    }

    return 0;
}

/* Process XML text {{{1 */

/*
 * Get the text content of an element and return it in result.  Positions the
 * reader on the closing tag.  result will be NULL on error, in which case the
 * return value will be FALSE, and "" for empty elements.  The caller is
 * responsible for xmlFree()ing the string.
 */
gboolean
ph_xml_extract_text(xmlTextReaderPtr reader,
                    xmlChar **result,
                    GError **error)
{
    int rc, depth;

    if (xmlTextReaderIsEmptyElement(reader)) {
        *result = xmlCharStrdup("");
        return TRUE;
    }

    /* overwrite prior values without leaking memory */
    if (*result != NULL) {
        xmlFree(*result);
        *result = NULL;
    }

    depth = xmlTextReaderDepth(reader);
    do {
        rc = xmlTextReaderRead(reader);
        if (rc == -1) {
            xmlFree(*result);
            *result = NULL;
            return ph_xml_set_last_error(error);
        }
        else if (*result == NULL && xmlTextReaderHasValue(reader))
            /* only return content of the first text node */
            *result = xmlTextReaderValue(reader);
    } while (xmlTextReaderDepth(reader) > depth);

    return TRUE;
}

/*
 * Directly obtain a double value from ph_xml_extract_text().  Returns FALSE
 * on error.
 */
gboolean
ph_xml_extract_double(xmlTextReaderPtr reader,
                      gdouble *result,
                      GError **error)
{
    xmlChar *text = NULL;

    if (ph_xml_extract_text(reader, &text, error)) {
        *result = g_ascii_strtod((gchar *) text, NULL);
        xmlFree(text);
        return TRUE;
    }
    else
        return FALSE;
}

/*
 * Run the result of ph_xml_extract_text() through ph_xml_find_string(), using
 * the given haystack and storing the obtained value in result.  Returns FALSE
 * on error.
 */
gboolean
ph_xml_extract_value(xmlTextReaderPtr reader,
                     const PHXmlStringTable *haystack,
                     gint *result,
                     GError **error)
{
    xmlChar *needle = NULL;

    if (ph_xml_extract_text(reader, &needle, error)) {
        *result = ph_xml_find_string(haystack, needle);
        xmlFree(needle);
        return TRUE;
    }
    else
        return FALSE;
}

/*
 * Directly obtain a UNIX timestamp from ph_xml_extract_text().  Assumes that
 * the time value is encoded in ISO 8601 format.  Returns FALSE on error.
 */
gboolean
ph_xml_extract_time(xmlTextReaderPtr reader,
                    glong *result,
                    GError **error)
{
    xmlChar *text = NULL;

    if (ph_xml_extract_text(reader, &text, error)) {
        GTimeVal tmp;
        (void) g_time_val_from_iso8601((gchar *) text, &tmp);
        *result = tmp.tv_sec;
        xmlFree(text);
        return TRUE;
    }
    else
        return FALSE;
}

/* Process XML attributes {{{1 */

/*
 * Check if the named XML attribute has the expected value.  Returns TRUE if
 * it does, FALSE if the attribute is either not present or not equal.
 */
gboolean
ph_xml_attrib_compare(xmlTextReaderPtr reader,
                      const xmlChar *attrib,
                      const xmlChar *value)
{
    xmlChar *text;
    gboolean matches;

    text = xmlTextReaderGetAttribute(reader, attrib);
    matches = (xmlStrcasecmp(text, value) == 0);
    xmlFree(text);
    return matches;
}

/*
 * Get an attribute of an XML element and store it in result.  The caller is
 * responsible for invoking xmlFree() afterwards.  On error, result will be
 * NULL and the return value FALSE.
 */
gboolean
ph_xml_attrib_text(xmlTextReaderPtr reader,
                   const xmlChar *attrib,
                   xmlChar **result,
                   GError **error)
{
    if (*result != NULL)
        xmlFree(*result);
    *result = xmlTextReaderGetAttribute(reader, attrib);
    if (*result == NULL) {
        if (xmlGetLastError() != NULL)
            (void) ph_xml_set_last_error(error);
        else
            g_set_error(error,
                    PH_XML_ERROR, PH_XML_ERROR_PARSE,
                    _("Missing attribute `%s' on <%s> on line %d"),
                    (gchar *) attrib, (gchar *) xmlTextReaderConstName(reader),
                    xmlTextReaderGetParserLineNumber(reader));
        return FALSE;
    }
    return TRUE;
}

/*
 * Parse an XML attribute as an integer and store it in result.  Returns FALSE
 * if the attribute is not present.
 */
gboolean
ph_xml_attrib_int(xmlTextReaderPtr reader,
                  const xmlChar *attrib,
                  gint *result,
                  GError **error)
{
    xmlChar *text = NULL;

    if (ph_xml_attrib_text(reader, attrib, &text, error)) {
        *result = atoi((gchar *) text);
        xmlFree(text);
        return TRUE;
    }
    else
        return FALSE;
}

/*
 * Parse an XML attribute as a double and store it in result.  Returns FALSE
 * if the attribute is not present.
 */
gboolean
ph_xml_attrib_double(xmlTextReaderPtr reader,
                     const xmlChar *attrib,
                     gdouble *result,
                     GError **error)
{
    xmlChar *text = NULL;

    if (ph_xml_attrib_text(reader, attrib, &text, error)) {
        *result = g_ascii_strtod((gchar *) text, NULL);
        xmlFree(text);
        return TRUE;
    }
    else
        return FALSE;
}

/* Error reporting {{{1 */

/*
 * Unique identifier for XML errors.
 */
GQuark
ph_xml_error_quark()
{
    return g_quark_from_static_string("ph-xml-error");
}

/*
 * Report the last error of the XML reader as a PH_XML_ERROR. Returns FALSE for
 * reasons of convenience.
 */
gboolean
ph_xml_set_last_error(GError **error)
{
    xmlErrorPtr err = xmlGetLastError();

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    g_return_val_if_fail(err != NULL, FALSE);

    g_set_error_literal(error,
            PH_XML_ERROR, PH_XML_ERROR_PARSE,
            err->message);
    return FALSE;
}

/* }}} */

/* vim: set sw=4 sts=4 et cino=(0,Ws tw=80 fdm=marker: */
