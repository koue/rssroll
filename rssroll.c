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
#include <curl/curl.h>
#include <curl/easy.h>
#include <sys/param.h>

#include "rss.h"

#define DEBUG 		0
/* file to save header information about the rss file 	*/
#define HEADERFILE	"/tmp/.head.txt"
/* file to save content for parsing from rss file 	*/
#define BODYFILE	"/tmp/.body.txt"
/* max length of the insert query			*/
#define	MAXQUERY	131072

/* rss database store	*/
static sqlite3* db;

/* debug message out */
void 
dmsg(char *m) {

	char msg[1024];
	snprintf(msg, sizeof(msg), "[DEBUG] %s", m);
	printf("%s\n", msg);
	fflush(stdout);
}

/* remove \r and \n from the end of string */
void
chomp(char *s) {
	while (*s && *s != '\n' && *s != '\r') 
		s++;
	*s = 0;
}

/* curl write function */
static size_t 
write_data(void *ptr, size_t size, size_t nmemb, void *stream) {

	int written = fwrite(ptr, size, nmemb, (FILE *)stream);
	return written;
}

/* add new feed into the database */
void
add_feed(int id, char *url, char *title, char *desc, time_t date) {
	char *errmsg;
	char query[MAXQUERY];	/* if the content of the feed is too long MAXQUERY value should be increased */	
	int ret;

	if (DEBUG)
		dmsg("add_feed");

	sqlite3_snprintf(sizeof(query), query, "insert into feeds (chanid, modified, link, title, description, pubdate) values (%d, 0, '%q', '%q', '%q', '%ld')", id, url, title, desc, date);	/* %q is like %s but with escapesd characters */ 

	if (DEBUG) 
		dmsg(query);

	ret = sqlite3_exec(db, query, NULL, 0, &errmsg);
	if ( ret != SQLITE_OK) {
		printf("Error: in query %s [%s].\n", query, errmsg);
	} else {
		printf("New feed has been added %s.\n", url);
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
check_link(int id, char *link, time_t pubdate) {
	
	char *errmsg;
	char query[1024], currtime[32];
	int result = 0, ret;
	time_t	date;

	if (DEBUG) 
		dmsg("check_link");

	sqlite3_snprintf(sizeof(query), query, "select id from feeds where pubdate = '%ld' and chanid = %d and link = '%q'", pubdate, id, link);

	if (DEBUG)
		dmsg(query);

	ret = sqlite3_exec(db, query, select_channel_callback, &result, &errmsg);

	if (DEBUG) 
		printf("results: %d\n", result);

	if (ret != SQLITE_OK) {
		printf("Error: in query %s [%s]\n.", query, errmsg);
		return 1;
	} else {
		if (result)  { // record has been found
			return 1; // dont do anything
		}
		else {
			time(&date);
			strcpy(currtime, ctime(&date));
			chomp(currtime);
			sqlite3_snprintf(sizeof(query), query, "update channels set modified = '%q' where id = %d", currtime, id); /* update last modified  
																	time of the channel */
			if (DEBUG)
				dmsg(query);

			ret = sqlite3_exec(db, query, NULL, 0, &errmsg);
			if (ret != SQLITE_OK)
				sqlite3_mprintf("Error: in query %q [%s].\n", query, errmsg);
			return 0; // call add_feed to add the item into the database
		}
	}
}

/* parse content of the rss */
void
parse_body(int id, char *modified) {

	int i;
	
	st_rss_t *rss = NULL;
	
        if(DEBUG)
                dmsg("parse_body.");

	rss = rss_open(BODYFILE);
	if (!rss) {	
		printf("rss id [%d] cannot be parsed.\n", id);
		return;
	}

	if (DEBUG) {
		printf("items - %d\n", rss->item_count);
	}

/*	if (DEBUG) {
		for (i = (rss->item_count - 1); i > -1  ; i--)
			printf("[%d] - %s\n"
				"%s\n"
				"%s\n", i, rss->item[i].title, rss->item[i].url, rss->item[i].desc);
	}
*/

	for ( i = (rss->item_count - 1); i > -1; i--)	/* check urls in reverse order, first feed has been added last to the rss */
		if(!check_link(id, rss->item[i].url, rss->item[i].date))
			add_feed(id, rss->item[i].url, rss->item[i].title, rss->item[i].desc, rss->item[i].date);
	rss_close (rss);
}

/* parse header of the rss, if last-modified date is same as in the database then don't do anything */
int 
parse_header(int id, char *modified) {
        char s[8192], *t, k[128];
        int i;
        FILE *fp;

        if(DEBUG)
                dmsg("parse_header");

        if ((fp = fopen(HEADERFILE, "r")) == NULL) {
                printf("Count not parse %s with ID: %d\n", HEADERFILE, id);
                return 0;
        }

        while ((t = fgets(s, sizeof(s), fp))) {
                while (*t == ' ' || *t == '\t')
                        t++;
                for (i = 0; i < sizeof(k) -1 && *t && *t != ' ' && *t != '\t'; ++i)
                        k[i] = *t++;
                k[i] = 0;
                if (!strcmp(k, "Last-Modified:")) {
                        t++;
                        k[0] = 0;
                        for (i = 0; i < MAXPATHLEN - 1 && *t && *t != '\n'; ++i)
                                k[i] = *t++;
                        break;
                }
        }
        fclose(fp);
        if (strcmp(modified, k))
                return 1;
        else
                return 0;
}

/* start rss tracing */
void 
parse_channel(int id, char *modified) {

	if (DEBUG) {
		dmsg("parse_channel");
		printf("id - %d\n", id);
	}

	if (parse_header(id, modified)) {
		parse_body(id, modified);
	}
	return;
}

/* fetch rss file */
int 
fetch_channel(char *link) {
	CURL *curl_handle;
	FILE *headerfile;
	FILE *bodyfile;

	if (DEBUG) {
		dmsg("fetch_channel");
		dmsg(link);
	}

	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "rssroll/1.0");
	curl_easy_setopt(curl_handle, CURLOPT_URL, link);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

	if ((headerfile = fopen(HEADERFILE, "w")) == NULL) {
		curl_easy_cleanup(curl_handle);
		return -1;
	}

	if ((bodyfile = fopen(BODYFILE, "w")) == NULL) {
		curl_easy_cleanup(curl_handle);
		return -1;
	}

	curl_easy_setopt(curl_handle, CURLOPT_WRITEHEADER, headerfile);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, bodyfile);
	curl_easy_perform(curl_handle);
	fclose(headerfile);
	fclose(bodyfile);
	curl_easy_cleanup(curl_handle);

	return 0;
}

/* get id, modified and url from the database for the rss url */
int 
trace_channels_callback(void *p_data, int num_fields, char **p_fields, char **p_col_names) {
/* 	p_fields[0] 	-	id	
	p_fields[1]	-	modified
	p_fields[2]	-	link
*/
	if (DEBUG) {
		dmsg("trace_chennels_callback");
		dmsg(p_fields[0]);
		dmsg(p_fields[1]);
		dmsg(p_fields[2]);
	}

//	printf("channel - %s.\n", p_fields[2]);
	if (!fetch_channel(p_fields[2])) {
		parse_channel (atoi(p_fields[0]), p_fields[1]);
	} else {
		printf("Error: cannot fetch channel %s.\n", p_fields[2]);
	}
	return 0;
}

void 
trace_channels(void) {
	char *errmsg;
	char query[]="select id, modified, link from channels";

	if(DEBUG) 
		dmsg("trace_channels");

	if(sqlite3_exec(db, query, trace_channels_callback, 0, &errmsg) != SQLITE_OK) {
		printf("SQL error: %s [%s].\n", query, errmsg);
	}
}

void 
usage(char *f){
	fprintf(stderr, "Usage:\n\t %s -d database.db\n", f);
}

int 
main( int argc, char** argv){

	char DBNAME[MAXPATHLEN];
	
	if(argc != 3) {
		usage(argv[0]);
		return 0;
	}

	if (strcmp (argv[1], "-d")) {
		usage(argv[0]);
		return 0;	
	}
	
	snprintf(DBNAME, sizeof(DBNAME), "%s", argv[2]);

	if(access(DBNAME, R_OK)){
		printf("Cannot read database file!\n");
		return 1;
	}
	
	sqlite3_open(DBNAME, &db);
	if (!db) {
		printf("Cannot open database %s\n",DBNAME);
		return 1;
	}
	if(DEBUG) {
		printf("[DEBUG] %s successfully loaded.\n",DBNAME);
	}
	trace_channels();
	remove(HEADERFILE);
	remove(BODYFILE);
	
	sqlite3_close(db);
	if(DEBUG) {
		printf("[DEBUG] %s successfully closed.\n",DBNAME);
	}
	return 0;
}
