/*
 * Copyright (c) 2012-2019 Nikola Kolev <koue@chaosophia.net>
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

#include <cez_fossil.h>
#include <cez_misc.h>
#include <cez_queue.h>
#include <ctype.h>
#include <errno.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "rss.h"

Global g;

static char		query_string[10];
unsigned long		query_category = 1;
unsigned long		query_limit = 0;
static gzFile		gz = NULL;
static char		*rssrollrc = "/etc/rssrollrc";
static unsigned long	callback_result = 0;

static struct 		cez_queue config;
static const char *params[] = { "category", "feeds", "ct_html", "dbpath",
    "htmldir", "name", "owner", "url", "webtheme", NULL };

typedef	void (*render_cb)(const char *, const st_rss_item_t *);

static void	 render_error(const char *fmt, ...);
static int	 render_html(const char *html_fn, render_cb r,
		     const st_rss_item_t *e);
static void	 render_front(const char *m, const st_rss_item_t *e);
static void	 render_front_feed(const char *m, const st_rss_item_t *e);

static void
d_printf(const char *fmt, ...)
{
	static char s[RSSMAXBUFSIZE];
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);
	if (r < 0 || r >= sizeof(s)) {
		printf("error d_printf: vsnprintf: r %d (%d)", r,
		    (int)sizeof(s));
	}
	if (gz != NULL) {
		r = gzputs(gz, s);
		if (r != strlen(s)) {
			printf("error d_printf: gzputs: r %d (%d)",
			    r, (int)strlen(s));
		}
	} else {
		fprintf(stdout, "%s", s);
	}
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
	d_printf("<html><head><title>Error</title></head><body>\n");
	d_printf("<h2>Error</h2><p><b>%s</b><p>\n", s);
	d_printf("Time: <b>%s</b><br>\n", rfc822_time(time(0)));
	d_printf("</body></html>\n");
}

static int
render_html(const char *html_fn, render_cb r, const st_rss_item_t *e)
{
	FILE *f;
	char s[8192];
	Stmt q;

	if ((f = fopen(html_fn, "re")) == NULL) {
		d_printf("ERROR: fopen: %s: %s<br>\n", html_fn,
		    strerror(errno));
		return (1);
	}
	while (fgets(s, sizeof(s), f)) {
		char *a, *b;
		for (a = s; (b = strstr(a, "%%")) != NULL;) {
			*b = 0;
			d_printf("%s", a);
			a = b + 2;
			if ((b = strstr(a, "%%")) != NULL) {
				*b = 0;
				if (strcmp(a, "RSSROLL_BASEURL") == 0) {
					d_printf("%s", cez_queue_get(&config, "url"));
				} else if (strcmp(a, "RSSROLL_NAME") == 0) {
					d_printf("%s", cez_queue_get(&config, "name"));
				} else if (strcmp(a, "RSSROLL_OWNER") == 0) {
					d_printf("%s", cez_queue_get(&config, "owner"));
				} else if (strcmp(a, "RSSROLL_CTYPE") == 0) {
					d_printf("%s", cez_queue_get(&config, "ct_html"));
				} else if (strcmp(a, "RSSROLL_CATEGORIES") == 0) {
					db_prepare(&q, "SELECT id, title FROM categories");
					while(db_step(&q)==SQLITE_ROW) {
						d_printf("<p><a href='%s?%d'>%s</a></p>",
						    cez_queue_get(&config, "url"), db_column_int(&q, 0), db_column_text(&q, 1));
					}
					db_finalize(&q);
				} else if (strcmp(a, "PREV") == 0) {
					if (callback_result == strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10)) {
						d_printf("<a href='%s?%ld/%ld'> <<< </a>",
						    cez_queue_get(&config, "url"), query_category, query_limit + strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10));
					}
				} else if (strcmp(a, "NEXT") == 0) {
					if (query_limit) {
						d_printf("<a href='%s?%ld/%ld'> >>> </a>",
						    cez_queue_get(&config, "url"), query_category, query_limit - strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10));
					}
				}
				else if (r != NULL) {
					(*r)(a, e);
				}
				a = b + 2;
			}
		}
		d_printf("%s", a);
	}
	fclose(f);
	return (0);
}

static void
render_front(const char *m, const st_rss_item_t *e)
{
	char fn[1024];
	Stmt q;
	st_rss_item_t	rss_item;

	if (strcmp(m, "FEEDS") == 0) {
		db_prepare(&q, "SELECT id, modified, link, title, description, pubdate "
				"FROM feeds WHERE chanid IN (select id from channels where catid = '%ld') "
				"ORDER BY id DESC LIMIT '%ld', '%d'",
				query_category, query_limit, strtol(cez_queue_get(&config, "feeds"), (char **)NULL, 10));
		while (db_step(&q)==SQLITE_ROW) {
			/* PREV option */
			callback_result++;
			snprintf(fn, sizeof(fn), "%s/%s/feed.html",
			    cez_queue_get(&config, "htmldir"), cez_queue_get(&config, "webtheme"));
			snprintf(rss_item.title, sizeof(rss_item.title), "%s",
			    db_column_text(&q, 3));
			snprintf(rss_item.url, sizeof(rss_item.url), "%s",
			    db_column_text(&q, 2));
			snprintf(rss_item.desc, sizeof(rss_item.desc), "%s",
			    db_column_text(&q, 4));
			rss_item.date = db_column_int64(&q, 5);
			render_html(fn, &render_front_feed, &rss_item);
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
		d_printf("render_front: unknown macro '%s'<br>\n", m);
	}
}

static void
render_front_feed(const char *m, const st_rss_item_t *e)
{
	if (strcmp(m, "PUBDATE") == 0) {
		d_printf("%s", ctime(&e->date));
	} else if (strcmp(m, "TITLE") == 0) {
		d_printf("%s", e->title);
	} else if (strcmp(m, "DESCRIPTION") == 0) {
		d_printf("%s", e->desc);
	} else if (strcmp(m, "URL") == 0) {
		d_printf("%s", e->url);
	} else if (strcmp(m, "CHANNEL") == 0) {
		char domain[64];
		sscanf(e->url, "%*[^//]//%63[^/]", domain);
		d_printf("%s", domain);
	} else {
		d_printf("render_front_feed: unknown macro '%s'<br>\n", m);
	}
}

int
main(void)
{
	const char *s, *query_args;
	time_t if_modified_since = 0;
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

	if ((s = getenv("IF_MODIFIED_SINCE")) != NULL) {
		if_modified_since = convert_rfc822_time(s);
		if (if_modified_since <= 0) {
			if_modified_since =
			    (time_t)strtoul(s, NULL, 10);
		}
		if (if_modified_since <= 0) {
			printf("warning main: invalid IF_MODIFIED_SINCE '%s'", s);
		}
	}
	if ((s = getenv("HTTP_ACCEPT_ENCODING")) != NULL) {
		char *p = strstr(s, "gzip");

		if (p != NULL && ((strncmp(p, "gzip;q=0", 8) != 0) ||
		    strtol(p + 7, (char **)NULL, 10)  > 0.0)) {
			gz = gzdopen(fileno(stdout), "wb9");
			if (gz == NULL) {
				printf("error main: gzdopen");
			} else {
				printf("Content-Encoding: gzip\r\n");
			}
		}
	}

	char fn[1024];

	printf("%s\r\n\r\n", cez_queue_get(&config, "ct_html"));
	fflush(stdout);
	snprintf(fn, sizeof(fn), "%s/main.html", cez_queue_get(&config, "htmldir"));
	render_html(fn, &render_front, NULL);

done:
	if (gz != NULL) {
		if (gzclose(gz) != Z_OK) {
			printf("error main: gzclose");
		}
		gz = NULL;
	} else {
		fflush(stdout);
	}

	cez_queue_purge(&config);
	sqlite3_close(g.db);
	return (0);
}
