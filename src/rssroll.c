/*
 * Copyright (c) 2004 Daniel Hartmeier
 * Copyright (c) 2012, 2013 Nikola Kolev
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <unistd.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <curl/easy.h>

#include "rss.h"

#define RSSROLL_VERSION	"rssroll/0.4"
/* max length of the insert query			*/
#define	MAXQUERY	131072

int debug = 0;
static char debugmsg[512];

/* rss database store	*/
static sqlite3* db;

/* debug message out */
void 
dmsg(char *m) {

	char msg[1024];
	snprintf(msg, sizeof(msg), "[debug] %s", m);
	printf("%s\n", msg);
	fflush(stdout);
}

/* curl write function */
static size_t 
write_data(void *ptr, size_t size, size_t nmemb, void *stream) {

	int written = fwrite(ptr, size, nmemb, (FILE *)stream);
	return written;
}

/* add new item into the database */
void
add_feed(int chan_id, char *item_url, char *item_title, char *item_desc, time_t item_date) {
	char *errmsg;
	char query[MAXQUERY];	/* if the content of the item is too long MAXQUERY value should be increased */	

	if (debug)
		dmsg("add_item");

	sqlite3_snprintf(sizeof(query), query, "insert into feeds (chanid, modified, link, title, description, pubdate) values (%d, 0, '%q', '%q', '%q', '%ld')", chan_id, item_url, item_title, item_desc, item_date);	/* %q is same as %s but with escaped characters */ 

	if (debug) 
		dmsg(query);

	if (sqlite3_exec(db, query, NULL, 0, &errmsg) != SQLITE_OK) {
		printf("Error: in query %s [%s].\n", query, errmsg);
	} else {
		printf("New feed has been added %s.\n", item_url);
	}
}

/* used to counts results from the query */
int
select_channel_callback(void *p_data, int num_fields, char **p_fields, char **p_col_names) {
	
	int *p_rn = (int*)p_data;
	(*p_rn)++;
	return 0;
}

/* checks if the feed url appears into the database */
int 
check_link(int chan_id, char *item_link, time_t item_pubdate) {
	
	char *errmsg;
	char query[1024]; 
	int result = 0;
	time_t	date;

	if (debug) 
		dmsg("check_link");

	sqlite3_snprintf(sizeof(query), query, "select id from feeds where pubdate = '%ld' and chanid = '%d' and link = '%q'", item_pubdate, chan_id, item_link);

	if (debug)
		dmsg(query);

	if (sqlite3_exec(db, query, select_channel_callback, &result, &errmsg) != SQLITE_OK) {
		printf("Error: in query %s [%s]\n.", query, errmsg);
		return 1;
	} else {
		if (result)  { // record has been found
			if(debug)
				dmsg("record has been found.");
			return 1; // dont do anything ; If you want to update changed post do it here
		}
		else {
			sqlite3_snprintf(sizeof(query), query, "update channels set modified = '%ld' where id = '%d'", time(&date), chan_id); /* update last modified  
																	time of the channel */
			if (debug)
				dmsg(query);

			if (sqlite3_exec(db, query, NULL, 0, &errmsg) != SQLITE_OK) 
				sqlite3_mprintf("Error: in query %q [%s].\n", query, errmsg);
			return 0; // call add_feed to add the item into the database
		}
	}
}

/* parse content of the rss */
void
parse_body(int chan_id, char *rssfile) {

	int i;
	
	st_rss_t *rss = NULL;
	
        if(debug)
                dmsg("parse_body.");

	rss = rss_open(rssfile);
	if (!rss) {	
		printf("rss id [%d] cannot be parsed.\n", chan_id);
		return;
	}

	if (debug) {
		snprintf(debugmsg, sizeof(debugmsg), "items - %d", rss->item_count);	
		dmsg(debugmsg);
	}

/*	if (debug) {
		for (i = (rss->item_count - 1); i > -1  ; i--)
			printf("[%d] - %s\n"
				"%s\n"
				"%s\n", i, rss->item[i].title, rss->item[i].url, rss->item[i].desc);
	}
*/

	for ( i = (rss->item_count - 1); i > -1; i--)	/* check urls in reverse order, first feed has been added last to the rss */
		if(!check_link(chan_id, rss->item[i].url, rss->item[i].date))
			add_feed(chan_id, rss->item[i].url, rss->item[i].title, rss->item[i].desc, rss->item[i].date);
	rss_close (rss);
}

/* fetch rss file */
int 
fetch_channel(int chan_id, long chan_modified, char *chan_link) {
	CURL *curl_handle;
	struct curl_slist *if_chan_modified = NULL;
	FILE *bodyfile;
	char chan_last_modified_time[64], fn[]="/tmp/rssroll.tmp.XXXXXXXXXX";
	long	http_code = 0;
	int fd;

	if (debug) {
		snprintf(debugmsg, sizeof(debugmsg), "fetch_channel - %d, %ld, %s", chan_id, chan_modified, chan_link);
		dmsg(debugmsg);
	}

	strftime(chan_last_modified_time, sizeof(chan_last_modified_time), "If-Modified-Since: %a, %d %b %Y %T %Z", localtime(&chan_modified));
	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();
	if_chan_modified = curl_slist_append(if_chan_modified, chan_last_modified_time);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, RSSROLL_VERSION);
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, if_chan_modified);
	curl_easy_setopt(curl_handle, CURLOPT_URL, chan_link);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

	if ((fd = mkstemp(fn)) == -1) {
		fprintf(stderr, "Cannot create temp file!\n");
		return 0;
	}

	if (fchmod(fd, 0600)) {
		fprintf(stderr, "Cannot set permission to temp file!\n");
		return 0;
	}


	if ((bodyfile = fdopen(fd, "w")) == NULL) {
		curl_easy_cleanup(curl_handle);
		fprintf(stderr, "Cannot open file for parsing!\n"); 
		return 0;
	}

	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, bodyfile);
	curl_easy_perform(curl_handle);
	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
	fclose(bodyfile);
	curl_easy_cleanup(curl_handle);
	if (http_code != 304)
		parse_body(chan_id, fn);
	else {
		if (debug) {
			snprintf(debugmsg, sizeof(debugmsg), "channel: id - %d, link - %s has not been changed.", chan_id, chan_link);	
			dmsg(debugmsg);
		}
	}
	unlink(fn);	
	return 1;
}

/* get id, modified and url from the database for the rss url */
int 
trace_channels_callback(void *p_data, int num_fields, char **p_fields, char **p_col_names) {
/* 	p_fields[0] 	-	id	
	p_fields[1]	-	modified
	p_fields[2]	-	link
*/
	if (debug) {
		snprintf(debugmsg, sizeof(debugmsg), "%s, %s, %s", p_fields[0], p_fields[1], p_fields[2]);
		dmsg(debugmsg);
	}

	if (!fetch_channel(atoi(p_fields[0]), atol(p_fields[1]), p_fields[2])) {
		fprintf(stderr, "Error: cannot fetch channel %s.\n", p_fields[2]);
	}
	return 0;
}

void 
trace_channels(void) {
	char *errmsg;
	char query[]="select id, modified, link from channels";

	if(debug) 
		dmsg("trace_channels");

	if(sqlite3_exec(db, query, trace_channels_callback, 0, &errmsg) != SQLITE_OK) {
		printf("SQL error: %s [%s].\n", query, errmsg);
	}
}

static void 
usage(void){
	extern	char *__progname;

	fprintf(stderr, "Usage: %s [-v] [-d database]\n", __progname);
	exit(1);
}

int 
main( int argc, char** argv){

	int ch;
	const char *dbname = "/var/db/rssroll.db";

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
		return 1;
	}
	if (sqlite3_open(dbname, &db) != SQLITE_OK) {
		fprintf(stderr, "Cannot open database file: %s\n", dbname);
		return 1;
	}
	if(debug) 
		dmsg("database successfully loaded.");
	trace_channels();
	sqlite3_close(db);
	if(debug) 
		dmsg("database successfully closed.");
	return 0;
}
