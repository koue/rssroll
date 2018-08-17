/*
 * Copyright (c) 2012-2018 Nikola Kolev <koue@chaosophia.net>
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

#include <cez_fossil.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <unistd.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <curl/easy.h>

#include "rss.h"

#define RSSROLL_VERSION	"rssroll/0.6.1"

int debug = 0;

/* rss database store	*/
Global g;

/* curl write function */
static size_t
write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
	int written = fwrite(ptr, size, nmemb, (FILE *)stream);
	return written;
}

/* add new item into the database */
void
add_feed(int chan_id, char *item_url, char *item_title, char *item_desc,
							time_t item_date) {
	dmsg(0, "%s: %s", __func__, item_url);
	db_multi_exec("INSERT INTO feeds (chanid, modified, link, title, "
					"description, pubdate) "
			"VALUES (%d, 0, '%q', '%q', '%q', '%ld')",
			 chan_id, item_url, item_title, item_desc, item_date);
	printf("New feed has been added %s.\n", item_url);
}

/* checks if the feed url appears into the database */
int
check_link(int chan_id, char *item_link, time_t item_pubdate) {
	int result = 0;
	time_t	date;

	dmsg(0,"check_link");
	result = db_int(0, "SELECT id FROM feeds WHERE pubdate = '%ld' "
				"AND chanid = '%d' AND link = '%q'",
					 item_pubdate, chan_id, item_link);
	if (result) {
		dmsg(0,"record has been found.");
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
parse_body(int chan_id, char *rssfile) {
	int i;
	st_rss_t *rss = NULL;

	dmsg(0,"parse_body.");

	rss = rss_open(rssfile);
	if (!rss) {
		printf("rss id [%d] cannot be parsed.\n", chan_id);
		return;
	}
	dmsg(0,"items - %d", rss->item_count);

	/* check in reverse order, first feed has been added last to the rss */
	for ( i = (rss->item_count - 1); i > -1; i--) {
		if(!check_link(chan_id, rss->item[i].url, rss->item[i].date))
			add_feed(chan_id, rss->item[i].url, rss->item[i].title,
					 rss->item[i].desc, rss->item[i].date);
	}
	rss_close (rss);
}

/* fetch rss file */
void
fetch_channel(int chan_id, long chan_modified, const char *chan_link) {
	CURL *curl_handle;
	struct curl_slist *if_chan_modified = NULL;
	FILE *bodyfile;
	char chan_last_modified_time[64], fn[]="/tmp/rssroll.tmp.XXXXXXXXXX";
	long	http_code = 0;
	int fd;

	dmsg(0,"%s: %d, %ld, %s", __func__, chan_id, chan_modified, chan_link);

	strftime(chan_last_modified_time, sizeof(chan_last_modified_time),
		 "If-Modified-Since: %a, %d %b %Y %T %Z",
						localtime(&chan_modified));
	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();
	if_chan_modified = curl_slist_append(if_chan_modified,
						chan_last_modified_time);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, RSSROLL_VERSION);
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, if_chan_modified);
	curl_easy_setopt(curl_handle, CURLOPT_URL, chan_link);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

	if ((fd = mkstemp(fn)) == -1) {
		fprintf(stderr, "%s: Cannot create temp file: %s\n", __func__,
								chan_link);
		goto cleanup;
	}
	if (fchmod(fd, 0600)) {
		fprintf(stderr, "%s: Cannot set permission to temp file: %s\n",
							__func__,  chan_link);
		goto done;
	}

	if ((bodyfile = fdopen(fd, "w")) == NULL) {
		fprintf(stderr, "%s: Cannot open file for parsing: %s\n",
							__func__, chan_link);
		goto done;
	}
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, bodyfile);
	curl_easy_perform(curl_handle);
	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
	fclose(bodyfile);
	if (http_code != 304)
		parse_body(chan_id, fn);
	else {
		dmsg(0,"%s: id - %d, link - %s has not been changed.",
						__func__, chan_id, chan_link);
	}
done:
	unlink(fn);
cleanup:
	dmsg(0, "%s: cleanup", __func__);
	unlink(fn);
	curl_slist_free_all(if_chan_modified);
	curl_easy_cleanup(curl_handle);
	curl_global_cleanup();
}

static void
usage(void){
	extern	char *__progname;
	fprintf(stderr, "Usage: %s [-v] [-d database]\n", __progname);
	exit(1);
}

int
main(int argc, char** argv){

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
	if (argc != optind)
		usage();
	if(access(dbname, R_OK)){
		fprintf(stderr, "Cannot read database file: %s!\n", dbname);
		return (1);
	}
	if (sqlite3_open(dbname, &g.db) != SQLITE_OK) {
		fprintf(stderr, "Cannot open database file: %s\n", dbname);
		return (1);
	}
	dmsg(0,"database successfully loaded.");
	db_prepare(&q, "SELECT id, modified, link FROM channels");
	while(db_step(&q)==SQLITE_ROW) {
		fetch_channel(db_column_int(&q, 0), (long)db_column_int64(&q, 1),
							db_column_text(&q, 2));
	}
	db_finalize(&q);
	sqlite3_close(g.db);
	dmsg(0,"database successfully closed.");
	return (0);
}
