/*
 * Copyright (c) 2018-2019 Nikola Kolev <koue@chaosophia.net>
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

#include <errno.h>
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
rss_sanity_check(struct feed *rss)
{
	struct item *item;
	int count = 0;
	TAILQ_FOREACH(item, &rss->items_list, entry) {
		printf("title: %s\nurl: %s\ndate: %ld\ndesc: %s\n\n",
		    item->title, item->url, (long)item->date, item->desc);
		count++;
	}
	printf("%d entries\n", count);
}

int
rss_close(struct feed *rss)
{
	struct item *current;

	dmsg(1, "%s: start", __func__);
	while (!TAILQ_EMPTY(&rss->items_list)) {
		current = TAILQ_FIRST(&rss->items_list);
		free(current->desc);
		free(current->title);
		free(current->url);
		TAILQ_REMOVE(&rss->items_list, current, entry);
		free(current);
	}
	if (rss) {
		free(rss->desc);
		free(rss->title);
		free(rss->url);
		free(rss->filename);
		free(rss);
	}
	dmsg(1, "%s: end", __func__);

	return (0);

}

static void
rss_channel(struct feed *rss, xmlDoc *doc, xmlNode *pnode)
{
	dmsg(1, "%s: start", __func__);
	while (pnode) {
		dmsg(1, "%s: pnode->name: %s", __func__, (char *) pnode->name);
		if (strcmp((char *)pnode->name, "title") == 0) {
			rss->title = (char *)xmlNodeGetContent(pnode->xmlChildrenNode);
		} else if (strcmp((char *)pnode->name, "description") == 0) {
			rss->desc = (char *)xmlNodeGetContent(pnode->xmlChildrenNode);
		} else if (strcmp((char *)pnode->name, "date") == 0||
		    strcmp((char *)pnode->name, "pubDate") == 0 ||
		    strcmp((char *) pnode->name, "dc:date") == 0)
			rss->date = strptime2((char *)xmlNodeListGetString(pnode->xmlChildrenNode->doc, pnode->xmlChildrenNode, 1));

		pnode = pnode->next;
	}
	dmsg(1, "%s: end", __func__);
}

static int
rss_entry(struct feed *rss, xmlDoc *doc, xmlNode *pnode)
{
	struct item *current;

	if ((current = calloc(1, sizeof(*current))) == NULL) {
		fprintf(stderr, "%s: %s\n", __func__, strerror(errno));
		exit(1);
	}
	char *p = NULL, *link = NULL, *guid = NULL;

	dmsg(1, "%s: start", __func__);
	while (pnode) {
		while (pnode && xmlIsBlankNode(pnode))
			pnode = pnode->next;

		if (pnode == NULL)
			break;

		dmsg(1, "%s: pnode->name: %s", __func__, (char *)pnode->name);
		if (strcmp((char *)pnode->name, "title") == 0) {
			current->title = (char *)xmlNodeGetContent(pnode->xmlChildrenNode);
		} else if (strcmp((char *)pnode->name, "link") == 0) {
			p = (char *)xml_get_value(pnode, "rel");	// atom
			if (p) {
				if (strcmp(p, "alternate") == 0) {
					link = (char *)xml_get_value(pnode, "href");
				}
				free(p);
			} else {
				link = (char *)xmlNodeGetContent(pnode->xmlChildrenNode); //rss
			}
		} else if (strcmp((char *)pnode->name, "guid") == 0) {
			guid = (char *)xmlNodeGetContent(pnode->xmlChildrenNode);
		} else if (!strcmp((char *)pnode->name, "description")) {
			current->desc = (char *)xmlNodeGetContent(pnode->xmlChildrenNode);
		} else if (!strcmp((char *)pnode->name, "content")) {
			current->desc = (char *)xmlNodeGetContent(pnode->xmlChildrenNode);
		} else if (!strcasecmp((char *)pnode->name, "date") ||
		    !strcasecmp((char *)pnode->name, "pubDate") ||
		    !strcasecmp((char *)pnode->name, "dc:date") ||
		    !strcmp((char *)pnode->name, "modified") ||
		    !strcmp((char *)pnode->name, "updated") ||
		    !strcasecmp((char *)pnode->name, "cropDate")) {
			current->date = strptime2((char *)xmlNodeListGetString(pnode->xmlChildrenNode->doc, pnode->xmlChildrenNode, 1));
		}

		pnode = pnode->next;
	}
	// some feeds use the guid tag for the link
	if (link) {
		if ((current->url = strdup(link)) == NULL) {
			goto fail;
		}
	} else {
		if ((current->url = strdup(guid)) == NULL) {
			goto fail;
		}
	}
	free(link);
	free(guid);
	/* add items in reverse order, the first is the newest one */
	TAILQ_INSERT_HEAD(&rss->items_list, current, entry);
	dmsg(1, "%s: end", __func__);
	return (0);
fail:
	free(current->title);
	free(current->desc);
	free(current);
	free(link);
	free(guid);
	return (-1);
}

static void
rss_head(struct feed *rss, xmlDoc *doc, xmlNode *node)
{
	dmsg(1, "%s: start", __func__);
	TAILQ_INIT(&rss->items_list);
	while (node) {
		while (node && xmlIsBlankNode(node))
			node = node->next;

		if (node == NULL)
			break;

		dmsg(1, "%s: node->name: %s", __func__, (char *)node->name);
		if (!strcmp((char *)node->name, "title")) {
			rss->title = (char *)xmlNodeGetContent(node->xmlChildrenNode);
		} else if (!strcmp((char *)node->name, "description")) {
			rss->desc = (char *)xmlNodeGetContent(node->xmlChildrenNode);
		} else if (!strcmp((char *)node->name, "date") ||
		    !strcmp((char *)node->name, "pubDate") ||
		    !strcmp((char *)node->name, "modified") ||
		    !strcmp((char *)node->name, "updated") ||
		    !strcmp((char *)node->name, "dc:date")) {
			rss->date = strptime2((char *)xmlNodeListGetString(node->xmlChildrenNode->doc, node->xmlChildrenNode, 1));
		 } else if (!strcmp((char *)node->name, "channel") && (rss->version == RSS_V1_0)) {
			rss_channel(rss, doc, node->xmlChildrenNode);
		} else if (!strcmp((char *)node->name, "item") ||
		    !strcmp((char *)node->name, "entry")) {
			if (rss_entry(rss, doc, node->xmlChildrenNode) == -1) {
				xmlFreeDoc(doc);
				rss_close(rss);
				fprintf(stderr, "%s: %s\n", __func__, strerror(errno));
				exit(1);
			}
		}
	node = node->next;
	}
	dmsg(1, "%s: end", __func__);
}

static struct feed *
rss_parse(struct feed *rss)
{
	xmlDoc *doc;
	xmlNode *node;

	dmsg(1, "%s: start", __func__);
	if ((doc = xmlParseFile(rss->filename)) == NULL) {
		fprintf(stderr, "%s: cannot read %s\n", __func__, rss->filename);
		return (NULL);
	} else if ((node = xmlDocGetRootElement(doc)) == NULL) {
		fprintf (stderr, "%s: empty document %s\n", __func__, rss->filename);
		xmlFreeDoc(doc);
		return (NULL);
	}

	dmsg(1, "%s: rss->filename %s", __func__, rss->filename);
	dmsg(1, "%s: node->name: %s", __func__, (char *)node->name);

	node = node->xmlChildrenNode;
	while (node && xmlIsBlankNode(node))
		node = node->next;

	if (node == NULL) {
		fprintf(stderr, "%s: bad document %s\n", __func__, rss->filename);
		xmlFreeDoc(doc);
		return (NULL);
	} else if (rss->version < ATOM_V0_1) {
		if (strcmp((char *)node->name, "channel")) {
			fprintf (stderr, "%s: bad document: channel missing %s\n",
			    __func__, rss->filename);
			return (NULL);
		} else if (rss->version != RSS_V1_0) /* document is RSS */
			node = node->xmlChildrenNode;
	}

	rss_head(rss, doc, node);
	if (debug > 1) {
		rss_sanity_check(rss);
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

struct feed *
rss_open(const char *fname)
{
	struct feed *rss = NULL;

	dmsg(1, "%s: start", __func__);
	dmsg(1, "%s: %s", __func__, fname);
	if ((rss = malloc(sizeof(*rss))) == NULL)
		return (NULL);

	memset(rss, 0, sizeof(*rss));
	rss->filename = strdup(fname);

	if ((rss->version = rss_demux(fname)) == -1) {
		fprintf(stderr, "ERROR: uknown feed format %s.\n", rss->filename);
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
