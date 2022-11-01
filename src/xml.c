/*
 * Copyright (c) 2022 Nikola Kolev <koue@chaosophia.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <time.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cez_core_pool.h>

static time_t
strptime2(char *s)
{
	int i = 0;
	char y[100], m[100], d[100];
	char h[100], min[100];
	struct tm time_tag;
	time_t t = time (0);
	const char *month_s[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};

	*y = *m = *d = *h = *min = 0;

	if (s[10] == 'T') {                     // YYYY-MM-DDT00:00+00:00
		sscanf (s, " %4s-%2s-%2sT%2s:%2s", y, m, d, h, min);
	} else if (s[3] == ',' && s[4] == ' ') {// Mon, 31 Jul 2006 15:05:00 GMT
		sscanf (s + 5, "%2s %s %4s %2s:%2s", d, m, y, h, min);
		for (i = 0; month_s[i]; i++)
		if (!strcasecmp (m, month_s[i])) {
			sprintf (m, "%d", i + 1);
			break;
		}
	} else if (s[4] == '-' && s[7] == '-') {	// 2006-07-19
		sscanf (s, "%4s-%2s-%2s", y, m, d);
	} else {					// YYYYMMDDTHHMMSS
		// sscanf (s, " %4s%2s%2sT", y, m, d);
	}
	free(s);

	memset(&time_tag, 0, sizeof(struct tm));

	if (*y)
		time_tag.tm_year = strtol (y, NULL, 10) - 1900;
	if (*m)
		time_tag.tm_mon = strtol (m, NULL, 10) - 1;
	if (*d)
		time_tag.tm_mday = strtol (d, NULL, 10);
	if (*h)
		time_tag.tm_hour = strtol (h, NULL, 10);
	if (*min)
		time_tag.tm_min = strtol (min, NULL, 10);

	t = mktime(&time_tag);

	return (t);
}

char *
xml_get_value(struct pool *pool, xmlNode *node, const char *name)
{
	xmlChar *current;
	char *value;

	if (node && name) {
		if (xmlHasProp(node, (const unsigned char *)name)) {
			current = xmlGetProp(node, (const unsigned char *)name);
			value = pool_strdup(pool, (char *)current);
			xmlFree(current);
			return (value);
		}
	}
	return (NULL);
}

char *
xml_get_content(struct pool *pool, xmlNode *node)
{
	char *content;
	xmlChar *current;

	if (node) {
		if ((current = xmlNodeGetContent(node->xmlChildrenNode)) == NULL) {
			return (NULL);
		}
		content = pool_strdup(pool, (char *)current);
		xmlFree(current);
		return (content);
	}

	return (NULL);
}

int
xml_isnode(xmlNode *node, const char *string, int usecase)
{
    if (usecase == 1)
	    return (!strcasecmp((char *)node->name, string));
    else
	    return (!strcmp((char *)node->name, string));
}

void
xml_isnode_date(xmlNode *node, time_t *var) {
    if (xml_isnode(node, "date", 1) || xml_isnode(node, "pubDate", 1) ||
      xml_isnode(node, "dc:date", 1) || xml_isnode(node, "modified", 0) ||
      xml_isnode(node, "updated", 0) || xml_isnode(node, "cropDate", 1) ||
      xml_isnode(node, "lastBuildDate", 0)) {
        *var = strptime2((char *)xmlNodeListGetString(node->xmlChildrenNode->doc,
             node->xmlChildrenNode, 1));
    }
}
