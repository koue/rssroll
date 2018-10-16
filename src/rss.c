/*
rss.h - RSS (and Atom) parser and generator (using libxml2)

Copyright (c) 2012-2018 Nikola Kolev <koue@chaosophia.net>
Copyright (c) 2006 NoisyB


This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <time.h>
#include <unistd.h>

#include "rss.h"

time_t
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


unsigned char *
xml_get_value(xmlNode *n, const char *name)
{
	if (n)
		if (xmlHasProp(n, (const unsigned char *)name))
			return (xmlGetProp(n, (const unsigned char *)name));
	return (NULL);
}

void
rss_st_rss_t_sanity_check(st_rss_t *rss)
{
	for (int i = 0; i < rss->item_count; i++)
		printf("pos: %d\n"
			"title: %s\n"
			"url: %s\n"
			"date: %ld\n"
			"desc: %s\n\n",
		i,
		rss->item[i].title,
		rss->item[i].url,
		(long)rss->item[i].date,
		rss->item[i].desc);
	printf("rss->item_count: %d\n\n", rss->item_count);
}

static void
rss_read_copy(char *d, xmlDoc *doc, xmlNode *n)
{
	dmsg(1, "%s: start", __func__);
	char *p = (char *)xmlNodeGetContent(n);
	if (p)
		strncpy(d, p, RSSMAXBUFSIZE)[RSSMAXBUFSIZE-1] = 0;
	else
		*d = 0;
	dmsg(1, "%s: p: %s", __func__, p);
	if (p)
		free(p);
	dmsg(1, "%s: end", __func__);
}

int
rss_close(st_rss_t *rss)
{
	dmsg(1, __func__);
	if (rss) {
		free(rss);
		rss = NULL;
	}

	return (0);
}

static void
rss_channel(st_rss_t *rss, xmlDoc *doc, xmlNode *pnode)
{
	dmsg(1, "%s: start", __func__);
	while (pnode) {
		dmsg(1, "%s: pnode->name: %s", __func__, (char *) pnode->name);
		if (strcmp((char *)pnode->name, "title") == 0)
			rss_read_copy(rss->title, doc, pnode->xmlChildrenNode);
		else if (strcmp((char *)pnode->name, "description") == 0)
			rss_read_copy (rss->desc, doc, pnode->xmlChildrenNode);
		else if (strcmp((char *)pnode->name, "date") == 0||
		    strcmp((char *)pnode->name, "pubDate") == 0 ||
		    strcmp((char *) pnode->name, "dc:date") == 0)
			rss->date = strptime2((char *)xmlNodeListGetString(pnode->xmlChildrenNode->doc, pnode->xmlChildrenNode, 1));

		pnode = pnode->next;
	}
	dmsg(1, "%s: end", __func__);
}

static void
rss_entry(st_rss_item_t *item, xmlDoc *doc, xmlNode *pnode)
{
	char link[RSSMAXBUFSIZE], guid[RSSMAXBUFSIZE];
	char *p = NULL, *href = NULL;

	*link = *guid = 0;

	dmsg(1, "%s: start", __func__);
	while (pnode) {
		while (pnode && xmlIsBlankNode(pnode))
			pnode = pnode->next;

		if (pnode == NULL)
			break;

		dmsg(1, "%s\n", (char *)pnode->name);
		if (strcmp((char *)pnode->name, "title") == 0) {
			rss_read_copy(item->title, doc, pnode->xmlChildrenNode);
		} else if (strcmp((char *)pnode->name, "link") == 0) {
			p = (char *)xml_get_value(pnode, "rel");	// atom
			if (p) {
				if (strcmp(p, "alternate") == 0) {
					href = (char *) xml_get_value(pnode, "href");
					strncpy(link, href, RSSMAXBUFSIZE)[RSSMAXBUFSIZE-1] = 0;
					free(href);
				}
				free(p);
			} else {
				rss_read_copy(link, doc, pnode->xmlChildrenNode); //rss
			}
		} else if (strcmp((char *)pnode->name, "guid") == 0 && (!(*link))) {
			rss_read_copy(guid, doc, pnode->xmlChildrenNode);
		} else if (!strcmp((char *)pnode->name, "description")) {
			rss_read_copy(item->desc, doc, pnode->xmlChildrenNode);
		} else if (!strcmp((char *)pnode->name, "content")) {
			rss_read_copy(item->desc, doc, pnode->xmlChildrenNode);
		} else if (!strcasecmp((char *)pnode->name, "date") ||
		    !strcasecmp((char *)pnode->name, "pubDate") ||
		    !strcasecmp((char *)pnode->name, "dc:date") ||
		    !strcmp((char *)pnode->name, "modified") ||
		    !strcmp((char *)pnode->name, "updated") ||
		    !strcasecmp((char *)pnode->name, "cropDate")) {
			item->date = strptime2((char *)xmlNodeListGetString(pnode->xmlChildrenNode->doc, pnode->xmlChildrenNode, 1));
		}

		pnode = pnode->next;
	}

	// some feeds use the guid tag for the link
	if (*link)
		strlcpy(item->url, link, sizeof(item->url));
	else if (*guid)
		strlcpy(item->url, guid, sizeof(item->url));
	else
		*(item->url) = 0;
	dmsg(1, "%s: end", __func__);
}

static void
rss_head(st_rss_t *rss, xmlDoc *doc, xmlNode *node)
{
	dmsg(1, "%s: start", __func__);
	while (node) {
		while (node && xmlIsBlankNode(node))
			node = node->next;

		if (node == NULL)
			break;

		dmsg(1, "%s: node->name: %s", __func__, (char *)node->name);
		if (!strcmp((char *)node->name, "title"))
			rss_read_copy(rss->title, doc, node->xmlChildrenNode);
		else if (!strcmp((char *)node->name, "description"))
			rss_read_copy(rss->desc, doc, node->xmlChildrenNode);
		else if (!strcmp((char *)node->name, "date") ||
		    !strcmp((char *)node->name, "pubDate") ||
		    !strcmp((char *)node->name, "modified") ||
		    !strcmp((char *)node->name, "updated") ||
		    !strcmp((char *)node->name, "dc:date"))
			rss->date = strptime2((char *)xmlNodeListGetString(node->xmlChildrenNode->doc, node->xmlChildrenNode, 1));
		else if (!strcmp((char *)node->name, "channel") && (rss->version == RSS_V1_0)) {
			rss_channel(rss, doc, node->xmlChildrenNode);
		} else if (!strcmp((char *)node->name, "item") ||
		    !strcmp((char *)node->name, "entry")) {
			rss_entry(&rss->item[rss->item_count], doc, node->xmlChildrenNode);
			rss->item_count++;
			if (rss->item_count == RSSMAXITEM)
				break;
		}
	node = node->next;
	}
	dmsg(1, "%s: end", __func__);
}

static st_rss_t *
rss_parse(st_rss_t *rss)
{
	xmlDoc *doc;
	xmlNode *node;

	dmsg(1, "%s: start", __func__);
	if ((doc = xmlParseFile(rss->url)) == NULL) {
		fprintf(stderr, "%s: cannot read %s\n", __func__, rss->url);
		return (NULL);
	} else if ((node = xmlDocGetRootElement(doc)) == NULL) {
		fprintf (stderr, "%s: empty document %s\n", __func__, rss->url);
		xmlFreeDoc(doc);
		return (NULL);
	}

	dmsg(1, "%s: rss->url %s", __func__, rss->url);
	dmsg(1, "%s: node->name: %s", __func__, (char *)node->name);

	node = node->xmlChildrenNode;
	while (node && xmlIsBlankNode(node))
		node = node->next;

	if (node == NULL) {
		fprintf(stderr, "%s: bad document %s\n", __func__, rss->url);
		xmlFreeDoc(doc);
		return (NULL);
	} else if (rss->version < ATOM_V0_1) {
		if (strcmp((char *)node->name, "channel")) {
			fprintf (stderr, "%s: bad document: channel missing %s\n",
							__func__, rss->url);
			return (NULL);
		} else if (rss->version != RSS_V1_0) // document is RSS
			node = node->xmlChildrenNode;
	}

	rss_head(rss, doc, node);
	if (debug > 1) {
		rss_st_rss_t_sanity_check(rss);
		fflush(stdout);
	}

	xmlFreeDoc(doc);
	dmsg(1, "%s: end", __func__);
	return (rss);
}

int
rss_demux(const char *fname)
{
	xmlDoc *doc = NULL;
	xmlNode *node = NULL;
	int version = -1;
	char *p = NULL;

	dmsg(1, "%s: start %s", __func__, fname);
	if ((doc = xmlParseFile(fname)) == NULL) {
		fprintf(stderr, "%s: cannot read %s\n", __func__, fname);
		goto done;
	}

	if ((node = xmlDocGetRootElement(doc)) == NULL)
		goto done;
	else if ((char *)node->name == NULL)
		goto done;
	else if (strcmp((char *)node->name, "html") == 0) // not xml
		goto done;
	else if (strcmp((char *)node->name, "feed") == 0) {
		version = ATOM_V0_1;	//default
		if ((p = (char *)xml_get_value(node, "version")) == NULL)
			goto done;
		else if (strcmp(p, "0.3") == 0)
			version = ATOM_V0_3;
		else if (strcmp(p, "0.2") == 0)
			version = ATOM_V0_2;
	} else if (strcmp((char *)node->name, "rss") == 0) {
		if ((p = (char *)xml_get_value(node, "version")) == NULL)
			goto done;
		else if (strcmp(p, "0.91") == 0)
			version = RSS_V0_91;
		else if (strcmp(p, "0.92") == 0)
			version = RSS_V0_92;
		else if (strcmp(p, "0.93") == 0)
			version = RSS_V0_93;
		else if (strcmp(p, "0.94") == 0)
			version = RSS_V0_94;
		else if ((strcmp(p, "2") == 0) || (strcmp(p, "2.0") == 0) ||
			    (strcmp(p, "2.00") == 0))
			version = RSS_V2_0;
	} else if ((strcmp((char *)node->name, "rdf") == 0) ||
		    (strcmp((char *)node->name, "RDF") == 0)) {
		version = RSS_V1_0;
	}
done:
	if (p != NULL)
		free(p);
	xmlFreeDoc(doc);
	dmsg(1, "%s: end", __func__);
	return (version);
}

st_rss_t *
rss_open(const char *fname)
{
	st_rss_t *rss = NULL;

	dmsg(1, "%s: start", __func__);
	dmsg(1, "%s: %s", __func__, fname);
	if ((rss = malloc(sizeof(st_rss_t))) == NULL)
		return (NULL);

	memset(rss, 0, sizeof(st_rss_t));
	strncpy(rss->url, fname, RSSMAXBUFSIZE)[RSSMAXBUFSIZE - 1] = 0;
	rss->item_count = 0;

	rss->version = rss_demux(fname);

	if (rss->version == -1) {
		fprintf(stderr, "ERROR: uknown feed format %s.\n", rss->url);
		return (NULL);
	}
	dmsg(1, "%s: end", __func__);
	return rss_parse(rss);
}

/* debug message out */
void
dmsg(int verbose, const char *fmt, ...)
{
	if (debug > verbose) {
		va_list ap;
		time_t t = time(NULL);
		struct tm *tm = gmtime(&t);
		fprintf(stdout, "%4.4d.%2.2d.%2.2d %2.2d:%2.2d:%2.2d ",
		    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
		    tm->tm_min, tm->tm_sec);
		va_start(ap, fmt);
		vfprintf(stdout, fmt, ap);
		va_end(ap);
		fprintf(stdout, "\n");
		fflush(stdout);
	}
}
