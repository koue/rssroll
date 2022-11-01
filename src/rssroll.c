/*
 * Copyright (c) 2012-2022 Nikola Kolev <koue@chaosophia.net>
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

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fetch.h>

#include <fslbase.h>
#include <fsldb.h>
#include <sqlite3.h>

#include "rss.h"

int debug = 0;

/* rss database store	*/
Global g;

/* add new item into the database */
void
add_feed(int chan_id, char *item_url, char *item_title, char *item_desc,
    time_t item_date)
{
	dmsg(0, "%s: %s", __func__, item_url);
	db_multi_exec("INSERT INTO feeds (chanid, modified, link, title, "
					"description, pubdate) "
			"VALUES (%d, 0, '%q', '%q', '%q', '%ld')",
			 chan_id, item_url, item_title, item_desc, item_date);
	printf("New feed has been added %s.\n", item_url);
}

/* checks if the feed url appears into the database */
int
check_link(int chan_id, char *item_link, time_t item_pubdate)
{
	int result = 0;
	time_t	date;

	dmsg(0, "check_link");
	result = db_int(0, "SELECT id FROM feeds WHERE pubdate = '%ld' "
				"AND chanid = '%d' AND link = '%q'",
					 item_pubdate, chan_id, item_link);
	if (result) {
		dmsg(0, "record has been found.");
		return (1); /* Don't do anything ;
			     If you want to update changed post do it here */
	}
	/* update last modified  time of the channel */
	db_multi_exec("UPDATE channels SET modified = '%ld' WHERE id = '%d'",
	    time(&date), chan_id);
	/* call add_feed to add the item into the database */
	return (0);
}

/* parse content of the rss */
void
parse_body(int chan_id, char *rssbody)
{
	struct feed *rss = NULL;
	struct item *item;

	dmsg(0,"parse_body.");

	if ((rss = rss_parse(rssbody, 0)) == NULL) {
		printf("rss id [%d] cannot be parsed.\n", chan_id);
		return;
	}
	TAILQ_FOREACH(item, &rss->items_list, entry) {
		if (check_link(chan_id, item->url, item->date) == 0) {
			add_feed(chan_id, item->url, item->title, item->desc,
			    item->date);
		}
	}
	rss_close(rss);
}

/* fetch rss file */
void
fetch_channel(int id, time_t modified, const char *link)
{
    Blob body = empty_blob;
    struct url *url;
    struct url_stat us;
    char flags[8];
    FILE *fp;

    *flags = 0;

	dmsg(0, "%s: %d, %ld, %s", __func__, id, modified, link);

    if ((url = fetchParseURL(link)) == NULL) {
        dmsg(0, "%s: invalid URL %s", __func__, link);
        return;
    }

    url->ims_time = modified;
    strcat(flags, "i");

    if ((fp = fetchXGet(url, &us, flags)) == NULL) {
        dmsg(0, "%s: cannot fetch URL %s", __func__, link);
        goto fail;
    }
    if ((blob_read_from_channel(&body, fp, -1)) < 1) {
        dmsg(0, "%s: empty body %s", __func__, link);
        goto reset;
    }
    parse_body(id, blob_str(&body));
reset:
    blob_reset(&body);
fail:
    fetchFreeURL(url);
}

static void
usage(void)
{
	extern	char *__progname;
	fprintf(stderr, "Usage: %s [-v] [-d database]\n", __progname);
	exit(1);
}

int
main(int argc, char** argv)
{

	int ch;
	const char *dbname = "/var/db/rssroll.db";
	Stmt q;

	while ((ch = getopt(argc, argv, "d:v")) != -1) {
		switch (ch) {
			case 'd':
				dbname = optarg;
				break;
			case 'v':
				debug++;
				break;
			default:
				usage();
		}
	}
	if (argc != optind) {
		usage();
	}
	if (access(dbname, R_OK)) {
		fprintf(stderr, "Cannot read database file: %s!\n", dbname);
		return (1);
	}
	if (sqlite3_open(dbname, &g.db) != SQLITE_OK) {
		fprintf(stderr, "Cannot open database file: %s\n", dbname);
		return (1);
	}
	dmsg(0, "database successfully loaded.");
	db_prepare(&q, "SELECT id, modified, link FROM channels");
	while (db_step(&q)==SQLITE_ROW) {
		fetch_channel(db_column_int(&q, 0), (time_t)db_column_int64(&q, 1),
		    db_column_text(&q, 2));
	}
	db_finalize(&q);
	sqlite3_close(g.db);
	dmsg(0, "database successfully closed.");
	return (0);
}
