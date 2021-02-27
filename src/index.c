/*
 * Copyright (c) 2012-2021 Nikola Kolev <koue@chaosophia.net>
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

#include <cez_core_pool.h>
#include <cez_misc.h>
#include <cez_queue.h>
#include <cez_render.h>
#include <ctype.h>
#include <errno.h>
#include <fslbase.h>
#include <fsldb.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "rss.h"

Global g;

/*
** query_array[3]:
**
** query_array[0]:
**   - default: [-1]: list tag id
**   - [0]: list single channel only
**
** query_array[1]:
**   - [>0]: list tag id or single channel id
**
** query_array[2]:
**   - [>=0]: limit in the feed queue
*/
static long		query_array[3] = { -1, 1, 0 };
static unsigned long	callback_result = 0;

static struct		cez_render render;
static struct 		cez_queue config;
static const char *params[] = { "category", "feeds", "ct_html", "dbpath",
    "htmldir", "name", "owner", "url", "webtheme", NULL };

static int
query_parse(char *str)
{
	int i = 0;

	while (*str) {
		char *value;
		value = str;
		while (*str && *str != '/')
			str++;
		if (*str) {
			*str = 0;
			str++;
		}
		query_array[i++] = strtol(value, NULL, 0);
	}
	if ((i > 3) || ((i == 3) && (query_array[0] != 0))) {
		return (-1); /* wrong query */
	} else if (query_array[0] != 0) {
		if (i == 2) {
			query_array[2] = query_array[1];
			query_array[1] = query_array[0];
			query_array[0] = -1;
		} else if (i == 1) {
			query_array[1] = query_array[0];
			query_array[0] = -1;
		}
	}

	return (0);
}

static int
query_string_validate(char *str)
{
	if (str == NULL)
		return (-1);
	if (strlen(str) > 43) {
		printf("Status: 400\r\n\r\n You are trying to send very long query!\n");
		fflush(stdout);
		return (-1);
	}
	while (*str) {
		if (*str != '/' && !(*str >= '0' && *str <= '9')) {
			printf("Status: 400\r\n\r\nYou are trying to send wrong query!\n");
			fflush(stdout);
			return (-1);
		}
		str++;
	}
	return (0);
}

static void
render_error(const char *fmt, ...)
{
	va_list ap;
	char s[8192];

	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);
	printf("Content-Type: text/html; charset=utf-8\r\n\r\n");
	fflush(stdout);
	printf("<html><head><title>Error</title></head><body>\n");
	printf("<h2>Error</h2><p><b>%s</b><p>\n", s);
	printf("Time: <b>%s</b><br>\n", rfc822_time(time(0)));
	printf("</body></html>\n");
}

static char *
getvalue(struct pool *pool, const char *value)
{
	char *current;
	if (value) {
		current = pool_strdup(pool, value);
		return (current);
	}
	return (NULL);
}

static void
render_items_list(const char *macro, void *arg)
{
	Stmt q;
	struct pool *pool;
	struct item *item;
	Blob sql = empty_blob;

	blob_append_sql(&sql, "SELECT "
	                      "    id, modified, link, title, description, pubdate, chanid "
		              "FROM "
		              "    feeds "
		              "WHERE ");
	if (query_array[0] == 0) // list single channel
		blob_append_sql(&sql, "chanid = '%ld' ", query_array[1]);
	else // list tag
		blob_append_sql(&sql, "chanid IN (select id from channels where catid = '%ld') ",
		    query_array[1]);
	blob_append_sql(&sql, "ORDER BY id "
			      "DESC LIMIT '%ld', '%d'",
			      query_array[2],
			      strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10));

	db_prepare_blob(&q, &sql);
	while (db_step(&q)==SQLITE_ROW) {
		/* PREV option */
		callback_result++;
		pool = pool_create(1024);
		item = item_create(pool);
		item->title = getvalue(pool, db_column_text(&q, 3));
		item->url = getvalue(pool, db_column_text(&q, 2));
		item->desc = getvalue(pool, db_column_text(&q, 4));
		item->date = db_column_int64(&q, 5);
		item->chanid = db_column_int64(&q, 6);
		cez_render_call(&render, "ITEMHTML", (void *)item);
		pool_free(pool);
	}
	db_finalize(&q);
}

static void
render_channel(const char *macro, void *arg)
{
	struct item *current = (struct item *)arg;

	if (current && current->url) {
		char domain[64];
		sscanf(current->url, "%*[^//]//%63[^/]", domain);
		printf("%s", domain);
	}
}

static void
render_follow(const char *macro, void *arg)
{
	struct item *current = (struct item *)arg;

	if (current && current->chanid) {
		printf("%ld", current->chanid);
	}
}

static void
render_url(const char *macro, void *arg)
{
	struct item *current = (struct item *)arg;

	if (current && current->url) {
		printf("%s", current->url);
	}
}

static void
render_description(const char *macro, void *arg)
{
	struct item *current = (struct item *)arg;

	if (current && current->desc) {
		printf("%s", current->desc);
	}
}

static void
render_pubdate(const char *macro, void *arg)
{
	struct item *current = (struct item *)arg;

	if (current && current->date) {
		printf("%s", ctime(&current->date));
	}
}

static void
render_title(const char *macro, void *arg)
{
	struct item *current = (struct item *)arg;

	if (current && current->title) {
		printf("%s", current->title);
	}
}

static void
render_main(const char *macro, void *arg)
{
	cez_render_call(&render, macro, arg);
}

static void
render_next(const char *macro, void *arg)
{
	if (query_array[2]) {
		long step = query_array[2] - strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10);
		if (query_array[0] == 0) {
			printf("<a href='%s?0/%ld/%ld'> >>> </a>", cez_queue_get(&config, "url"),
			    query_array[1], step);
		} else {
			printf("<a href='%s?%ld/%ld'> >>> </a>", cez_queue_get(&config, "url"),
			    query_array[1], step);
		}
	}
}

static void
render_prev(const char *macro, void *arg)
{
	long step = 0;
	if (callback_result == strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10)) {
		step = query_array[2] + strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10);
		if (query_array[0] == 0) {
			printf("<a href='%s?0/%ld/%ld'> <<< </a>", cez_queue_get(&config, "url"),
			    query_array[1], step);
		} else {
			printf("<a href='%s?%ld/%ld'> <<< </a>", cez_queue_get(&config, "url"),
			    query_array[1], step);
		}
	}
}

static void
render_categories(const char *macro, void *arg)
{
	Stmt q;

	db_prepare(&q, "SELECT id, title FROM categories");
	while(db_step(&q)==SQLITE_ROW) {
		printf("<p><a href='%s?%d'>%s</a></p>",
		    cez_queue_get(&config, "url"), db_column_int(&q, 0),
		    db_column_text(&q, 1));
	}
	db_finalize(&q);
}


static void
render_ctype(const char *macro, void *arg)
{
	printf("%s", cez_queue_get(&config, "ct_html"));
}

static void
render_owner(const char *macro, void *arg)
{
	printf("%s", cez_queue_get(&config, "owner"));
}

static void
render_name(const char *macro, void *arg)
{
	printf("%s", cez_queue_get(&config, "name"));
}

static void
render_baseurl(const char *macro, void *arg)
{
	printf("%s", cez_queue_get(&config, "url"));
}

static void
render_init(void)
{
	char fn[256];

	cez_render_init(&render);
	snprintf(fn, sizeof(fn), "%s/main.html", cez_queue_get(&config, "htmldir"));
	cez_render_add(&render, "MAIN", fn, (struct item *)render_main);
	snprintf(fn, sizeof(fn), "%s/%s/header.html", cez_queue_get(&config, "htmldir"),
	    cez_queue_get(&config, "webtheme"));
	cez_render_add(&render, "HEADER", fn, (struct item *)render_main);
	snprintf(fn, sizeof(fn), "%s/%s/footer.html", cez_queue_get(&config, "htmldir"),
	    cez_queue_get(&config, "webtheme"));
	cez_render_add(&render, "FOOTER", fn, (struct item *)render_main);
	cez_render_add(&render, "FEEDS", NULL, (struct entry *)render_items_list);
	snprintf(fn, sizeof(fn), "%s/%s/feed.html", cez_queue_get(&config, "htmldir"),
	    cez_queue_get(&config, "webtheme"));
	cez_render_add(&render, "ITEMHTML", fn, (struct entry *)render_main);
	cez_render_add(&render, "RSSROLL_BASEURL", NULL, (struct item *)render_baseurl);
	cez_render_add(&render, "RSSROLL_NAME", NULL, (struct item *)render_name);
	cez_render_add(&render, "RSSROLL_OWNER", NULL, (struct item *)render_owner);
	cez_render_add(&render, "RSSROLL_CTYPE", NULL, (struct item *)render_ctype);
	cez_render_add(&render, "RSSROLL_CATEGORIES", NULL, (struct item *)render_categories);
	cez_render_add(&render, "PUBDATE", NULL, (struct item *)render_pubdate);
	cez_render_add(&render, "TITLE", NULL, (struct item *)render_title);
	cez_render_add(&render, "DESCRIPTION", NULL, (struct item *)render_description);
	cez_render_add(&render, "URL", NULL, (struct item *)render_url);
	cez_render_add(&render, "CHANNEL", NULL, (struct item *)render_channel);
	cez_render_add(&render, "FOLLOW", NULL, (struct item *)render_follow);
	cez_render_add(&render, "PREV", NULL, (struct item*)render_prev);
	cez_render_add(&render, "NEXT", NULL, (struct item*)render_next);
}

int
main(int argc, char *argv[])
{
	char *conffile, *query_string;
	const char *confcheck;
	int i, valgrind = 0;

	umask(007);

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--valgrind") == 0) {
			valgrind = 1;
		}
	}

	cez_queue_init(&config);
	if (valgrind) {
		conffile = "../etc/rssrollrc";
	} else {
		conffile = CONFFILE;
	}
	if (configfile_parse(conffile, &config) == -1) {
		render_error("error: cannot open config file: %s", conffile);
		goto done;
	}
	if ((confcheck = cez_queue_check(&config, params)) != NULL) {
		render_error("error: missing config: %s", confcheck);
		goto done;
	}

	if (valgrind) {
		if (cqu(&config, "dbpath", "rssrolltest.db") == -1) {
			printf("Cannot adjust dbpath. Exit\n");
			exit (1);
		}
		if (cqu(&config, "htmldir", "../html") == -1) {
			printf("Cannot adjust htmldir. Exit\n");
			exit (1);
		}
	}

	if ((valgrind == 0) && chdir("/tmp")) {
		printf("error main: chdir: /tmp: %s", strerror(errno));
		render_error("chdir: /tmp: %s", strerror(errno));
		goto done;
	}

	if (sqlite3_open(cez_queue_get(&config, "dbpath"), &g.db) != SQLITE_OK) {
		render_error("cannot load database: %s", cez_queue_get(&config, "dbpath"));
		goto done;
	}

	if (((query_string = getenv("QUERY_STRING")) != NULL) && strlen(query_string)) {
		if (query_string_validate(query_string) == -1) {
			cez_queue_purge(&config);
			return (0);
		}
	}
	if (query_parse(query_string) == -1) {
               	printf("Status: 400\r\n\r\nYou are trying to send wrong query!\n");
	       	fflush(stdout);
		return (-1);
	}

	printf("%s\r\n\r\n", cez_queue_get(&config, "ct_html"));
	render_init();
	cez_render_call(&render, "MAIN", NULL);

done:
	fflush(stdout);

	cez_render_purge(&render);
	cez_queue_purge(&config);
	sqlite3_close(g.db);
	return (0);
}
