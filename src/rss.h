/*
 * Copyright (c) 2018-2022 Nikola Kolev <koue@chaosophia.net>
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

#ifndef _RSS_H_
#define _RSS_H_

#include <sys/queue.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <time.h>

#include <cez_core_pool.h>

#define	VERSION		"rssroll/0.11.0"
#define	CONFFILE	"/etc/rssrollrc"

enum {
	RSS_V0_90,
	RSS_V0_91,
	RSS_V0_92,
	RSS_V0_93,
	RSS_V0_94,
	RSS_V1_0,
	RSS_V2_0,
	ATOM_V0_1,
	ATOM_V0_2,
	ATOM_V0_3,
};

struct item {
	char *title;
	char *url;
	char *desc;
	time_t date;
	long chanid;
	TAILQ_ENTRY(item) entry;
};

struct feed {
	struct pool *pool;
	int version;
	char *title;
	char *url;
	char *desc;
	time_t date;
	xmlDoc *doc;
	TAILQ_HEAD(items_list, item) items_list;
};

int rss_demux(struct feed *rss, xmlNode *node);

struct feed *rss_parse(const char *xmlstream, int isfile);
int rss_close(struct feed *rss);

extern int debug;
void dmsg(int, const char *fmt, ...);

struct item *item_create(struct pool *pool);

#endif /* _RSS_H_ */
