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
strptime2(const char *s)
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

	memset (&time_tag, 0, sizeof (struct tm));

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

	t = mktime (&time_tag);

	return (t);
}


const unsigned char *
xml_get_value(xmlNode *n, const char *name)
{
	if (n)
		if (xmlHasProp (n, (const unsigned char *) name))
			return (xmlGetProp(n, (const unsigned char *) name));
	return (NULL);
}

void
rss_st_rss_t_sanity_check(st_rss_t *rss)
{
	int i = 0;

	for (; i < rss->item_count; i++)
		printf ("pos: %d\n"
			"title: %s\n"
			"url: %s\n"
			"date: %ld\n"
			"desc: %s\n\n",
		i,
		rss->item[i].title,
		rss->item[i].url,
		rss->item[i].date,
		rss->item[i].desc);
	printf ("rss->item_count: %d\n\n", rss->item_count);
}

static void
rss_read_copy(char *d, xmlDoc *doc, xmlNode *n)
{
	dmsg(1,"rss_read_copy in");
	const char *p = (const char *) xmlNodeGetContent(n);
	if (p)
		strncpy (d, p, RSSMAXBUFSIZE)[RSSMAXBUFSIZE-1] = 0;
	else
		*d = 0;
	dmsg(1,"rss_read_copy p: %s", p);
	dmsg(1,"rss_read_copy ouy");
}

int
rss_close(st_rss_t *rss)
{
	dmsg(1,"rss_close");
	if (rss) {
		free (rss);
		rss = NULL;
	}

	return (0);
}

static void
rss_channel(st_rss_t *rss, xmlDoc *doc, xmlNode *pnode)
{
	while (pnode) {
		dmsg(1,"pnode->name: %s", (char *) pnode->name);
		if (!strcmp ((char *) pnode->name, "title"))
			rss_read_copy(rss->title, doc, pnode->xmlChildrenNode);
		else if (!strcmp ((char *) pnode->name, "description"))
			rss_read_copy (rss->desc, doc, pnode->xmlChildrenNode);
		else if (!strcmp ((char *) pnode->name, "date") ||
		    !strcmp ((char *) pnode->name, "pubDate") ||
		    !strcmp ((char *) pnode->name, "dc:date"))
			rss->date = strptime2((const char *) xmlNodeListGetString(pnode->xmlChildrenNode->doc, pnode->xmlChildrenNode, 1));

		pnode = pnode->next;
	}
}

static void
rss_entry(st_rss_item_t *item, xmlDoc *doc, xmlNode *pnode)
{
	char link[RSSMAXBUFSIZE], guid[RSSMAXBUFSIZE];
	const char *p = NULL;

	*link = *guid = 0;

	while (pnode) {
		while (pnode && xmlIsBlankNode(pnode))
			pnode = pnode->next;

		if (!pnode)
			break;

		dmsg(1,"%s\n", (char *) pnode->name);
		if (!strcmp ((char *) pnode->name, "title")) {
			rss_read_copy (item->title, doc, pnode->xmlChildrenNode);
		} else if (!strcmp ((char *) pnode->name, "link")) {
			p = (const char *) xml_get_value(pnode, "rel");	// atom
			if (p) {
				if (strcmp(p, "alternate") == 0) {
					p = (const char *) xml_get_value(pnode, "href");
					strncpy(link, p, RSSMAXBUFSIZE)[RSSMAXBUFSIZE-1] = 0;
				}
			} else {
				rss_read_copy (link, doc, pnode->xmlChildrenNode); //rss
			}
		} else if (!strcmp ((char *) pnode->name, "guid") && (!(*link))) {
			rss_read_copy (guid, doc, pnode->xmlChildrenNode);
		} else if (!strcmp ((char *) pnode->name, "description")) {
			rss_read_copy (item->desc, doc, pnode->xmlChildrenNode);
		} else if (!strcmp ((char *) pnode->name, "content")) {
			rss_read_copy (item->desc, doc, pnode->xmlChildrenNode);
		} else if (!strcasecmp ((char *) pnode->name, "date") ||
		    !strcasecmp ((char *) pnode->name, "pubDate") ||
		    !strcasecmp ((char *) pnode->name, "dc:date") ||
		    !strcmp ((char *) pnode->name, "modified") ||
		    !strcmp ((char *) pnode->name, "updated") ||
		    !strcasecmp ((char *) pnode->name, "cropDate")) {
			item->date = strptime2((const char *) xmlNodeListGetString(pnode->xmlChildrenNode->doc, pnode->xmlChildrenNode, 1));
		}

		pnode = pnode->next;
	}

	// some feeds use the guid tag for the link
	if (*link)
		strcpy (item->url, link);
	else if (*guid)
		strcpy (item->url, guid);
	else
		*(item->url) = 0;
}

static void
rss_head(st_rss_t *rss, xmlDoc *doc, xmlNode *node)
{
	while (node) {
		while (node && xmlIsBlankNode(node))
			node = node->next;

		if (!node)
			break;

		dmsg(1,"node->name: %s", (char *) node->name);
		if (!strcmp ((char *) node->name, "title"))
			rss_read_copy (rss->title, doc, node->xmlChildrenNode);
		else if (!strcmp ((char *) node->name, "description"))
			rss_read_copy (rss->desc, doc, node->xmlChildrenNode);
		else if (!strcmp ((char *) node->name, "date") ||
		    !strcmp ((char *) node->name, "pubDate") ||
		    !strcmp ((char *) node->name, "modified") ||
		    !strcmp ((char *) node->name, "updated") ||
		    !strcmp ((char *) node->name, "dc:date"))
			rss->date = strptime2((const char *) xmlNodeListGetString(node->xmlChildrenNode->doc, node->xmlChildrenNode, 1));
		else if (!strcmp ((char *) node->name, "channel") && (rss->version == RSS_V1_0)) {
			rss_channel(rss, doc, node->xmlChildrenNode);
		} else if (!strcmp ((char *) node->name, "item") ||
		    !strcmp ((char *) node->name, "entry")) {
			rss_entry(&rss->item[rss->item_count], doc, node->xmlChildrenNode);
			rss->item_count++;
			if (rss->item_count == RSSMAXITEM)
				break;
		}
	node = node->next;
	}
}

static st_rss_t *
rss_parse(st_rss_t *rss)
{
	xmlDoc *doc;
	xmlNode *node;

	dmsg(1,"rss_open_rss");
	doc = xmlParseFile (rss->url);
	if (!doc) {
		fprintf(stderr, "ERROR: cannot read %s\n", rss->url);
		return (NULL);
	}

	node = xmlDocGetRootElement(doc);
	if (!node) {
		fprintf (stderr, "ERROR: empty document %s\n", rss->url);
		return (NULL);
	}
	dmsg(1,"rss->url: %s", rss->url);

	dmsg(1,"node->name: %s", (char *) node->name);

	node = node->xmlChildrenNode;
	while (node && xmlIsBlankNode(node))
		node = node->next;

	if (!node) {
		// fprintf (stderr, "");
		return (NULL);
	}

	if (rss->version < ATOM_V0_1) {
		if (strcmp ((char *) node->name, "channel")) {
			fprintf (stderr, "ERROR: bad document: did not immediately find the RSS element\n");
			return (NULL);
		}

		if (rss->version != RSS_V1_0) // document is RSS
			node = node->xmlChildrenNode;
	}

	rss_head(rss, doc, node);
	if(debug > 1) {
		rss_st_rss_t_sanity_check (rss);
		fflush(stdout);
	}
	return (rss);
}

int
rss_demux(const char *fname)
{
	xmlDoc *doc = NULL;
	xmlNode *node = NULL;
	int version = -1;
	char *p = NULL;

	dmsg(1,"rss_demux %s", fname);

	if (!(doc = xmlParseFile(fname))) {
		fprintf(stderr, "ERROR: cannot read %s\n", fname);
		return (-1);
	}

	node = xmlDocGetRootElement(doc);
	if (!node)
		return (-1);
	if (!(char *) node->name )
		return (-1);
	if (!strcmp((char *) node->name, "html")) // not xml
		return (-1);
	if (!strcmp((char *) node->name, "feed")) {
		version = ATOM_V0_1;	//default

		if (!(p = (char *) xml_get_value(node, "version")))
			return (version);
		if (!strcmp(p, "0.3"))
			version = ATOM_V0_3;
		else if (!strcmp(p, "0.2"))
			version = ATOM_V0_2;
		return (version);
	} else if (!strcmp((char *) node->name, "rss")) {
		if (!(p = (char *) xml_get_value(node, "version")))
			return (-1);
		if (!strcmp(p, "0.91"))
			version = RSS_V0_91;
		else if (!strcmp(p, "0.92"))
			version = RSS_V0_92;
		else if (!strcmp(p, "0.93"))
			version = RSS_V0_93;
		else if (!strcmp(p, "0.94"))
			version = RSS_V0_94;
		else if (!strcmp(p, "2") || !strcmp(p, "2.0") || !strcmp(p, "2.00"))
			version = RSS_V2_0;
		return (version);
	} else if (!strcmp((char *) node->name, "rdf") || !strcmp((char *) node->name, "RDF")) {
		version = RSS_V1_0;
		return (version);
	}

	return (-1);
}

st_rss_t *
rss_open(const char *fname)
{
	st_rss_t *rss = NULL;

	dmsg(1,"rss_open %s\n", fname);
	if (!(rss = malloc(sizeof (st_rss_t))))
		return (NULL);

	memset(rss, 0, sizeof(st_rss_t));
	strncpy(rss->url, fname, RSSMAXBUFSIZE)[RSSMAXBUFSIZE - 1] = 0;
	rss->item_count = 0;

	rss->version = rss_demux(fname);

	if (rss->version == -1) {
		fprintf (stderr, "ERROR: uknown feed format %s.\n", rss->url);
		return (NULL);
	}
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
