/*
 * Copyright (c) 2018-2023 Nikola Kolev <koue@chaosophia.net>
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
#include <unistd.h>

#include "rss.h"
#include "xml.h"

static struct feed *
feed_create(void)
{
	struct pool *pool = pool_create(1024);
	struct feed *feed = pool_alloc(pool, sizeof(struct feed));

	/* Make sure cleared out */
	memset(feed, 0, sizeof(struct feed));

	/* Common */
	feed->pool = pool;
	feed->version = 0;
	feed->title = NULL;
	feed->url = NULL;
	feed->desc = NULL;
	feed->doc = NULL;
	time_t date = 0;

	return (feed);
}

static void
feed_free(struct feed *feed)
{
	pool_free(feed->pool);
}

static void
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
	dmsg(1, "%s: start", __func__);
	feed_free(rss);
	dmsg(1, "%s: end", __func__);

	return (0);
}

static void
rss_channel(struct feed *rss, xmlNode *pnode)
{
	struct pool *pool = rss->pool;
	xmlDoc *doc = rss->doc;

	dmsg(1, "%s: start", __func__);
	while (pnode) {
		dmsg(1, "%s: pnode->name: %s", __func__, (char *) pnode->name);
		if (xml_isnode(pnode, "title", 0)) {
			rss->title = xml_get_content(pool, pnode);
		} else if (xml_isnode(pnode, "description", 0)) {
			rss->desc = xml_get_content(pool, pnode);
		} else {
                        xml_isnode_date(pnode, &rss->date);
                }

		pnode = pnode->next;
	}
	dmsg(1, "%s: end", __func__);
}

static int
rss_entry(struct feed *rss, xmlNode *pnode)
{
	struct item *current;
	struct pool *pool = rss->pool;
	xmlDoc *doc = rss->doc;

	char *p = NULL, *link = NULL, *guid = NULL;

	dmsg(1, "%s: start", __func__);
	if ((current = item_create(pool)) == NULL) {
		goto fail;
	}

	while (pnode) {
		while (pnode && xmlIsBlankNode(pnode))
			pnode = pnode->next;

		if (pnode == NULL)
			break;

		dmsg(1, "%s: pnode->name: %s", __func__, (char *)pnode->name);
		if (xml_isnode(pnode, "title", 0)) {
			current->title = xml_get_content(pool, pnode);
		} else if (xml_isnode(pnode, "link", 0)) {
			// atom
			if ((p = xml_get_value(pool, pnode, "rel")) != NULL) {
				if (strcmp(p, "alternate") == 0) {
					link = xml_get_value(pool, pnode, "href");
				}
			// rss
			} else {
				link = xml_get_content(pool, pnode);
			}
		} else if (xml_isnode(pnode, "guid", 0)) {
			guid = xml_get_content(pool, pnode);
		} else if (xml_isnode(pnode, "description", 0)) {
			current->desc = xml_get_content(pool, pnode);
		} else if (xml_isnode(pnode, "content", 0)) {
			current->desc = xml_get_content(pool, pnode);
		} else {
                        xml_isnode_date(pnode, &current->date);
		}

		pnode = pnode->next;
	}
	// some feeds use the guid tag for the link
	if (link) {
		if ((current->url = pool_strdup(pool, link)) == NULL) {
			goto fail;
		}
	} else {
		if ((current->url = pool_strdup(pool, guid)) == NULL) {
			goto fail;
		}
	}
	/* add items in reverse order, the first is the newest one */
	TAILQ_INSERT_HEAD(&rss->items_list, current, entry);
	dmsg(1, "%s: end", __func__);
	return (0);
fail:
	feed_free(rss);
	exit(1);
}

static void
rss_head(struct feed *rss, xmlNode *node)
{
	struct pool *pool = rss->pool;
	xmlDoc *doc = rss->doc;

	dmsg(1, "%s: start", __func__);
	TAILQ_INIT(&rss->items_list);
	while (node) {
		while (node && xmlIsBlankNode(node))
			node = node->next;

		if (node == NULL)
			break;

		dmsg(1, "%s: node->name: %s", __func__, (char *)node->name);
		if (xml_isnode(node, "title", 0)) {
			rss->title = xml_get_content(pool, node);
		} else if (xml_isnode(node, "description", 0)) {
			rss->desc = xml_get_content(pool, node);
		} else if (xml_isnode(node, "channel", 0) && (rss->version == RSS_V1_0)) {
			rss_channel(rss, node->xmlChildrenNode);
		} else if (xml_isnode(node, "item", 0) || xml_isnode(node, "entry", 0)) {
			if (rss_entry(rss, node->xmlChildrenNode) == -1) {
				xmlFreeDoc(doc);
				rss_close(rss);
				fprintf(stderr, "%s: %s\n", __func__, strerror(errno));
				exit(1);
			}
		} else {
                        xml_isnode_date(node, &rss->date);
                }

                node = node->next;
	}
	dmsg(1, "%s: end", __func__);
}

static int
rss_version_atom(struct pool *pool, xmlNode *node)
{
	int version = ATOM_V0_1;	//default
	char *p = NULL;

	if ((p = xml_get_value(pool, node, "version")) == NULL)
		goto done;
	else if (strcmp(p, "0.3") == 0)
		version = ATOM_V0_3;
	else if (strcmp(p, "0.2") == 0)
		version = ATOM_V0_2;
done:
	return (version);
}

static int
rss_version_rss(struct pool *pool, xmlNode *node)
{
	int version = -1;
	char *p = NULL;

	if ((p = xml_get_value(pool, node, "version")) == NULL)
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
done:
	return (version);
}

static int
rss_demux(struct feed *rss, xmlNode *node)
{
	struct pool *pool = rss->pool;
	int version = -1;

	dmsg(1, "%s: start", __func__);

	if ((char *)node->name == NULL)
		goto done;
	else if (xml_isnode(node, "html", 0)) // not xml
		goto done;
	else if (xml_isnode(node, "feed", 0)) {
		version = rss_version_atom(pool, node);
	} else if (xml_isnode(node, "rss", 0)) {
		version = rss_version_rss(pool, node);
	} else if (xml_isnode(node, "rdf", 0) || xml_isnode(node, "RDF", 0)) {
		version = RSS_V1_0;
	}
done:
	dmsg(1, "%s: end", __func__);
	return (version);
}

struct feed *
rss_parse(const char *xmlstream, int isfile)
{
	struct feed *rss;
	xmlNode *node;
	xmlDoc *doc;

	dmsg(1, "%s: start", __func__);

	if ((rss = feed_create()) == NULL)
		return (NULL);

	doc = rss->doc;
	if (isfile)
		doc = xmlParseFile(xmlstream);
	else
		doc = xmlParseDoc((const xmlChar *)xmlstream);

	if (doc == NULL) {
		fprintf(stderr, "%s: cannot read stream\n", __func__);
		goto fail;
	}

	if ((node = xmlDocGetRootElement(doc)) == NULL) {
		fprintf (stderr, "%s: empty document\n", __func__);
		goto faildoc;
	}

	if ((rss->version = rss_demux(rss, node)) == -1) {
		fprintf (stderr, "%s: unknown document\n", __func__);
		goto faildoc;
	}

	node = node->xmlChildrenNode;
	while (node && xmlIsBlankNode(node))
		node = node->next;

	if (node == NULL) {
		fprintf(stderr, "%s: bad document\n", __func__);
		goto faildoc;
	} else if (rss->version < ATOM_V0_1) {
		if (xml_isnode(node, "channel", 0) == 0) {
			fprintf (stderr, "%s: bad document: channel missing\n", __func__);
			goto faildoc;
		} else if (rss->version != RSS_V1_0) // document is RSS
			node = node->xmlChildrenNode;
	}

	rss_head(rss, node);
	if (debug > 1) {
		rss_sanity_check(rss);
		fflush(stdout);
	}

	dmsg(1, "%s: end", __func__);
	xmlFreeDoc(doc);
	return (rss);

faildoc:
	xmlFreeDoc(doc);

fail:
	feed_free(rss);
	return (NULL);
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
