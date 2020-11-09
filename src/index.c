/*
 * Copyright (c) 2012-2020 Nikola Kolev <koue@chaosophia.net>
 * Copyright (c) 2004-2006 Daniel Hartmeier. All rights reserved.
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

#include <cez_misc.h>
#include <cez_queue.h>
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

static char		query_string[10];
unsigned long		query_category = 1;
unsigned long		query_limit = 0;
static char		*rssrollrc = "/etc/rssrollrc";
static unsigned long	callback_result = 0;

static struct 		cez_queue config;
static const char *params[] = { "category", "feeds", "ct_html", "dbpath",
    "htmldir", "name", "owner", "url", "webtheme", NULL };

typedef	void (*render_cb)(const char *, const struct item *);

static void	 render_error(const char *fmt, ...);
static int	 render_html(const char *html_fn, render_cb r,
		     const struct item *e);
static void	 render_front(const char *m, const struct item *e);
static void	 render_front_feed(const char *m, const struct item *e);

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

static int
render_html(const char *html_fn, render_cb r, const struct item *e)
{
	FILE *f;
	char s[8192];
	Stmt q;

	if ((f = fopen(html_fn, "re")) == NULL) {
		printf("ERROR: fopen: %s: %s<br>\n", html_fn,
		    strerror(errno));
		return (1);
	}
	while (fgets(s, sizeof(s), f)) {
		char *a, *b;
		for (a = s; (b = strstr(a, "%%")) != NULL;) {
			*b = 0;
			printf("%s", a);
			a = b + 2;
			if ((b = strstr(a, "%%")) != NULL) {
				*b = 0;
				if (strcmp(a, "RSSROLL_BASEURL") == 0) {
					printf("%s", cez_queue_get(&config, "url"));
				} else if (strcmp(a, "RSSROLL_NAME") == 0) {
					printf("%s", cez_queue_get(&config, "name"));
				} else if (strcmp(a, "RSSROLL_OWNER") == 0) {
					printf("%s", cez_queue_get(&config, "owner"));
				} else if (strcmp(a, "RSSROLL_CTYPE") == 0) {
					printf("%s", cez_queue_get(&config, "ct_html"));
				} else if (strcmp(a, "RSSROLL_CATEGORIES") == 0) {
					db_prepare(&q, "SELECT id, title FROM categories");
					while(db_step(&q)==SQLITE_ROW) {
						printf("<p><a href='%s?%d'>%s</a></p>",
						    cez_queue_get(&config, "url"), db_column_int(&q, 0), db_column_text(&q, 1));
					}
					db_finalize(&q);
				} else if (strcmp(a, "PREV") == 0) {
					if (callback_result == strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10)) {
						printf("<a href='%s?%ld/%ld'> <<< </a>",
						    cez_queue_get(&config, "url"), query_category, query_limit + strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10));
					}
				} else if (strcmp(a, "NEXT") == 0) {
					if (query_limit) {
						printf("<a href='%s?%ld/%ld'> >>> </a>",
						    cez_queue_get(&config, "url"), query_category, query_limit - strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10));
					}
				}
				else if (r != NULL) {
					(*r)(a, e);
				}
				a = b + 2;
			}
		}
		printf("%s", a);
	}
	fclose(f);
	return (0);
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
render_front(const char *m, const struct item *e)
{
	struct pool *pool = pool_create(1024);
	struct item *item;
	char fn[1024];
	Stmt q;

	if (strcmp(m, "FEEDS") == 0) {
		snprintf(fn, sizeof(fn), "%s/%s/feed.html", cez_queue_get(&config, "htmldir"),
		    cez_queue_get(&config, "webtheme"));
		db_prepare(&q, "SELECT "
			       "    id, modified, link, title, description, pubdate "
			       "FROM "
			       "    feeds "
			       "WHERE "
			       "    chanid IN (select id from channels where catid = '%ld') "
			       "ORDER BY id "
			       "DESC LIMIT '%ld', '%d'",
				query_category, query_limit,
				strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10));
		snprintf(fn, sizeof(fn), "%s/%s/feed.html", cez_queue_get(&config, "htmldir"),
		    cez_queue_get(&config, "webtheme"));
		while (db_step(&q)==SQLITE_ROW) {
			/* PREV option */
			callback_result++;
			item = item_create(pool);
			item->title = getvalue(pool, db_column_text(&q, 3));
			item->url = getvalue(pool, db_column_text(&q, 2));
			item->desc = getvalue(pool, db_column_text(&q, 4));
			item->date = db_column_int64(&q, 5);
			render_html(fn, &render_front_feed, item);
			pool_free(pool);
		}
		db_finalize(&q);
	} else if (strcmp(m, "HEADER") == 0) {
		snprintf(fn, sizeof(fn), "%s/%s/header.html", cez_queue_get(&config, "htmldir"),
		    cez_queue_get(&config, "webtheme"));
		render_html(fn, NULL, NULL);
	} else if (strcmp(m, "FOOTER") == 0) {
		snprintf(fn, sizeof(fn), "%s/%s/footer.html", cez_queue_get(&config, "htmldir"),
		    cez_queue_get(&config, "webtheme"));
		render_html(fn, NULL, NULL);
	} else {
		printf("render_front: unknown macro '%s'<br>\n", m);
	}
}

static void
render_front_feed(const char *m, const struct item *e)
{
	if (strcmp(m, "PUBDATE") == 0) {
		printf("%s", ctime(&e->date));
	} else if (strcmp(m, "TITLE") == 0) {
		printf("%s", e->title);
	} else if (strcmp(m, "DESCRIPTION") == 0) {
		printf("%s", e->desc);
	} else if (strcmp(m, "URL") == 0) {
		printf("%s", e->url);
	} else if (strcmp(m, "CHANNEL") == 0) {
		char domain[64];
		sscanf(e->url, "%*[^//]//%63[^/]", domain);
		printf("%s", domain);
	} else {
		printf("render_front_feed: unknown macro '%s'<br>\n", m);
	}
}

int
main(void)
{
	const char *s, *query_args;
	int i;

	umask(007);
	cez_queue_init(&config);
	if (configfile_parse(rssrollrc, &config) == -1) {
		render_error("error: cannot open config file: %s", rssrollrc);
		goto done;
	}
	if ((s = cez_queue_check(&config, params)) != NULL) {
		render_error("error: missing config: %s", s);
		goto done;
	}
	if (chdir("/tmp")) {
		printf("error main: chdir: /tmp: %s", strerror(errno));
		render_error("chdir: /tmp: %s", strerror(errno));
		goto done;
	}

	if (sqlite3_open(cez_queue_get(&config, "dbpath"), &g.db) != SQLITE_OK) {
		render_error("cannot load database: %s", cez_queue_get(&config, "dbpath"));
		goto done;
	}

	if (((s = getenv("QUERY_STRING")) != NULL) && strlen(s)) {
		if (strlen(s) > 64) {
			printf("Status: 400\r\n\r\n You are trying to send very long query!\n");
			fflush(stdout);
			cez_queue_purge(&config);
			return (0);
		} else {
			for (i = 0; i < strlen(s); i++) {
				/*
					sanity check of the query string, accepts only digit
				*/
                        	if (!isdigit(s[i])) {
					if(s[i] != '/') {
                                		printf("Status: 400\r\n\r\nYou are trying to send wrong query!\n");
	                                	fflush(stdout);
						cez_queue_purge(&config);
        	                        	return (0);
					}
                	        }
                	}
			snprintf(query_string, sizeof(query_string), "%s", s);
			query_args = strtok(query_string, "/");
			query_category = strtol(query_args, NULL, 0);
			query_args = strtok(NULL, "/");
			if (query_args != NULL) {
				query_limit = strtol(query_args, NULL, 0);
			}
		}
	}

	char fn[1024];

	printf("%s\r\n\r\n", cez_queue_get(&config, "ct_html"));
	fflush(stdout);
	snprintf(fn, sizeof(fn), "%s/main.html", cez_queue_get(&config, "htmldir"));
	render_html(fn, &render_front, NULL);

done:
	fflush(stdout);

	cez_queue_purge(&config);
	sqlite3_close(g.db);
	return (0);
}
