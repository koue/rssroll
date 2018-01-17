/*
 * Copyright (c) 2012-2018 Nikola Kolev <koue@chaosophia.net>
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

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <cez.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include <sqlite3.h>

#include "rss.h"

#include "config.h"

#ifndef HAVE_STRLCAT
/*
 * '_cups_strlcat()' - Safely concatenate two strings.
 */

size_t				/* O - Length of string */
strlcat(char *dst,		/* O - Destination string */
	const char *src,	/* I - Source string */
	size_t size)		/* I - Size of destination string buffer */
{
	size_t srclen;		/* Length of source string */
	size_t dstlen;		/* Length of destination string */


 /*
  * Figure out how much room is left...
  */

	dstlen = strlen(dst);
	size -= dstlen + 1;

	if (!size)
	return (dstlen);	/* No room, return immediately... */

 /*
  * Figure out how much room is needed...
  */

	srclen = strlen(src);

 /*
  * Copy the appropriate amount...
  */

	if (srclen > size)
	srclen = size;

	memcpy(dst + dstlen, src, srclen);
	dst[dstlen + srclen] = '\0';

	return (dstlen + srclen);
}
#endif /* !HAVE_STRLCAT */

#ifndef HAVE_STRLCPY
/*
 * '_cups_strlcpy()' - Safely copy two strings.
 */

size_t				/* O - Length of string */
strlcpy(char *dst,		/* O - Destination string */
	const char *src,	/* I - Source string */
	size_t size)		/* I - Size of destination string buffer */
{
	size_t srclen;		/* Length of source string */


 /*
  * Figure out how much room is needed...
  */

	size --;

	srclen = strlen(src);

 /*
  * Copy the appropriate amount...
  */

	if (srclen > size)
	srclen = size;

	memcpy(dst, src, srclen);
	dst[srclen] = '\0';

	return (srclen);
}
#endif /* !HAVE_STRLCPY */

Global g;

static char		query_string[10];
//static char		query_category[10];
unsigned long		query_category = 1;
//static char		query_limit[256];
unsigned long		query_limit = 0;
static gzFile		gz = NULL;
static char		*rssrollrc = "/usr/local/etc/rssrollrc";
static unsigned long	callback_result = 0;

struct index_params {
	int	feeds;
	int	defcat;
	char 	*url;
	char	*dbpath;
	char	*name;
	char	*desc;
	char	*owner;
	char	*ct_html;
	char	*htmldir;
	char	*webtheme;
};

static struct index_params	rssroll;

typedef	void (*render_cb)(const char *, const st_rss_item_t *);

static void	 render_error(const char *fmt, ...);
static int	 render_html(const char *html_fn, render_cb r,
						const st_rss_item_t *e);
static void	 render_front(const char *m, const st_rss_item_t *e);
static void	 render_front_feed(const char *m, const st_rss_item_t *e);
static const char *rfc822_time(time_t t);

static void
d_printf(const char *fmt, ...)
{
	static char s[RSSMAXBUFSIZE];
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);
	if (r < 0 || r >= sizeof(s))
		printf("error d_printf: vsnprintf: r %d (%d)", r,
							(int)sizeof(s));
	if (gz != NULL) {
		r = gzputs(gz, s);
		if (r != strlen(s))
			printf("error d_printf: gzputs: r %d (%d)",
			    r, (int)strlen(s));
	} else
		fprintf(stdout, "%s", s);
}

static void
render_error(const char *fmt, ...)
{
	va_list ap;
	char s[8192];

	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);
	printf("%s\r\n\r\n", rssroll.ct_html);
	fflush(stdout);
	d_printf("<html><head><title>Error</title></head><body>\n");
	d_printf("<h2>Error</h2><p><b>%s</b><p>\n", s);
	d_printf("Time: <b>%s</b><br>\n", rfc822_time(time(0)));
	d_printf("<p>If you believe this is a bug in <i>this</i> server, "
	    "please send reports with instructions about how to "
	    "reproduce to <a href=\"mailto:%s\"><b>%s</b></a><p>\n",
	    rssroll.owner, rssroll.owner);
	d_printf("</body></html>\n");
}

static int
render_html(const char *html_fn, render_cb r, const st_rss_item_t *e)
{
	FILE *f;
	char s[8192];
	Stmt q;

	if ((f = fopen(html_fn, "r")) == NULL) {
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
				if (!strcmp(a, "RSSROLL_BASEURL"))
					d_printf("%s", rssroll.url);
				else if (!strcmp(a, "RSSROLL_NAME"))
					d_printf("%s", rssroll.name);
				else if (!strcmp(a, "RSSROLL_OWNER"))
					d_printf("%s", rssroll.owner);
				else if (!strcmp(a, "RSSROLL_CTYPE"))
					d_printf("%s", rssroll.ct_html);
				else if (!strcmp(a, "RSSROLL_CATEGORIES")){
					db_prepare(&q, "SELECT id, title FROM categories");
					while(db_step(&q)==SQLITE_ROW) {
						d_printf("<p><a href='%s?%d'>%s</a></p>",
							rssroll.url, db_column_int(&q, 0), db_column_text(&q, 1));
					}
					db_finalize(&q);
				} else if (!strcmp(a, "PREV")) {
					if (callback_result == rssroll.feeds)	{
						d_printf("<a href='%s?%ld/%ld'> <<< </a>",
							rssroll.url, query_category, query_limit + rssroll.feeds);
					}
				} else if (!strcmp(a, "NEXT")) {
					if (query_limit) {
						d_printf("<a href='%s?%ld/%ld'> >>> </a>",
							rssroll.url, query_category, query_limit - rssroll.feeds);
					}
				}
				else if (r != NULL)
					(*r)(a, e);
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
	/*
		catid = 1 - tech
		catid = 2 - blog
		catid = 3 - news
	*/
	Stmt q;
	st_rss_item_t	rss_item;	

	if (!strcmp(m, "FEEDS")) {
		db_prepare(&q, "SELECT id, modified, link, title, description, pubdate "
				"FROM feeds WHERE chanid IN (select id from channels where catid = '%ld') "
				"ORDER BY id DESC LIMIT '%ld', '%d'",
				query_category, query_limit, rssroll.feeds);
		while(db_step(&q)==SQLITE_ROW) {
			/* PREV option */
			callback_result++;
			snprintf(fn, sizeof(fn), "%s/%s/feed.html",
					rssroll.htmldir, rssroll.webtheme);
			snprintf(rss_item.title, sizeof(rss_item.title), "%s",
							db_column_text(&q, 3));
			snprintf(rss_item.url, sizeof(rss_item.url), "%s",
							db_column_text(&q, 2));
			snprintf(rss_item.desc, sizeof(rss_item.desc), "%s",
							db_column_text(&q, 4));
			rss_item.date = strtol(db_column_text(&q, 5), NULL, 0);
			render_html(fn, &render_front_feed, &rss_item);
		}
		db_finalize(&q);
	} else if (!strcmp(m, "HEADER")) {
		snprintf(fn, sizeof(fn), "%s/%s/header.html", rssroll.htmldir,
							rssroll.webtheme);
		render_html(fn, NULL, NULL);
	} else if (!strcmp(m, "FOOTER")) {
		snprintf(fn, sizeof(fn), "%s/%s/footer.html", rssroll.htmldir,
							rssroll.webtheme);
		render_html(fn, NULL, NULL);
	} else
		d_printf("render_front: unknown macro '%s'<br>\n", m);
}

static void
render_front_feed(const char *m, const st_rss_item_t *e)
{
	char *a, *b;

	if (!strcmp(m, "PUBDATE")) {
		d_printf("%s", ctime(&e->date));
	} else  if (!strcmp(m, "TITLE")) {
		d_printf("%s", e->title);
	} else if (!strcmp(m, "DESCRIPTION")) {
		d_printf("%s", e->desc);
	} else if (!strcmp(m, "URL")) {
		d_printf("%s", e->url);
	} else if (!strcmp(m, "CHANNEL")) {
		for ( a = e->url; (b = strstr(a, "//")) != NULL;) {
			*b = 0;
			a = b + 2;
			if (( b = strstr(a, "/")) != NULL ) {
				*b = 0;
				d_printf("%s", a);
				a = b + 1;
			}
		}
	} else
		d_printf("render_front_feed: unknown macro '%s'<br>\n", m);
}

static const char *
rfc822_time(time_t t)
{
	static char s[30], *p;

	p = ctime(&t);
	if (p == NULL || strlen(p) != 25) {
		strlcpy(s, "<invalid-time>", sizeof(s));
		return (s);
	}
	/* Thu Nov 24 18:22:48 1986\n */
	/* Wed, 02 Oct 2002 13:00:00 GMT */
	strlcpy(s, p, 4);
	strlcat(s, ", ", 6);
	strlcat(s, p + 8, 9);
	strlcat(s, p + 4, 13);
	strlcat(s, p + 20, 17);
	strlcat(s, " ", 18);
	strlcat(s, p + 11, 26);
	strlcat(s, " GMT", 30);
	return (s);
}

static time_t
convert_rfc822_time(const char *date)
{
	const char *mns[13] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
	    "Aug", "Sep", "Oct", "Nov", "Dec", NULL };
	char wd[4], mn[4], zone[16];
	int d, h, m, s, y, i;
	struct tm tm;
	time_t t;

	if (sscanf(date, "%3s, %d %3s %d %d:%d:%d %15s",
	    wd, &d, mn, &y, &h, &m, &s, zone) != 8)
		return (0);
	for (i = 0; mns[i] != NULL; ++i)
		if (!strcmp(mns[i], mn))
			break;
	if (mns[i] == NULL)
		return (0);
	memset(&tm, 0, sizeof(tm));
	tm.tm_year = y - 1900;
	tm.tm_mon = i;
	tm.tm_mday = d;
	tm.tm_hour = h;
	tm.tm_min = m;
	tm.tm_sec = s;
	tm.tm_zone = zone;
	t = mktime(&tm);
	return (t);
}

int load_config(void) {
	char *value;
	
	if ((rssroll.url = config_queue_value_get("url")) == NULL) {
		render_error("url is missing");
		return (-1);
	} 
	if ((rssroll.dbpath = config_queue_value_get("dbpath")) == NULL) {
		render_error("dbpath is missing");
		return (-1);
	}
	if ((rssroll.name = config_queue_value_get("name")) == NULL) {
		render_error("name is missing");
		return (-1);
	}
	if ((rssroll.desc = config_queue_value_get("desc")) == NULL) {
		render_error("desc is missing");
		return (-1);
	}
	if ((rssroll.owner = config_queue_value_get("owner")) == NULL) {
		render_error("owner is missing");
		return (-1);
	}
	if ((rssroll.ct_html = config_queue_value_get("ct_html")) == NULL) {
		render_error("ct_html is missing");
		return (-1);
	}
	if ((rssroll.htmldir = config_queue_value_get("htmldir")) == NULL) {
		render_error("htmldir is missing");
		return (-1);
	}
	if ((rssroll.webtheme = config_queue_value_get("webtheme")) == NULL) {
		render_error("webtheme is missing");
		return (-1);
	}
	if ((value = config_queue_value_get("feeds")) == NULL) {
		render_error("feeds is missing");
		return (-1);
	} else {
		rssroll.feeds = atoi(value);
	}
	if (( value = config_queue_value_get("defcat")) == NULL) {
		render_error("defcat is missing");
		return (-1);
	} else {
		rssroll.defcat = atoi(value);
	}
	return (0);
}

int 
main(int argc, char *argv[])
{
	const char *s, *query_args;
	time_t if_modified_since = 0;
	int i;

	umask(007);
	if (configfile_parse(rssrollrc, config_queue_cb) == -1) {
		render_error("error: cannot open config file: %s", rssrollrc);	
		goto done;
	}
	if (load_config() == -1)
		goto done;
	/*if (chdir("/tmp")) {
		printf("error main: chdir: /tmp: %s", strerror(errno));
		render_error("chdir: /tmp: %s", strerror(errno));
		goto done;
	} */

	if (sqlite3_open(rssroll.dbpath, &g.db) != SQLITE_OK) {
		render_error("cannot load database: %s", rssroll.dbpath);
		goto done;
	}

	if ((s = getenv("QUERY_STRING")) != NULL) {
		if (strlen(s) > 64) {
			printf("Status: 400\r\n\r\n You are trying to send very long query!\n");
			fflush(stdout);
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
		if (!if_modified_since)
			if_modified_since =
			    (time_t)strtoul(s, NULL, 10);
		if (!if_modified_since)
			printf("warning main: invalid IF_MODIFIED_SINCE '%s'", s);
	}
	if ((s = getenv("HTTP_ACCEPT_ENCODING")) != NULL) {
		char *p = strstr(s, "gzip");

		if (p != NULL && (strncmp(p, "gzip;q=0", 8) ||
		    atoi(p + 7) > 0.0)) {
			gz = gzdopen(fileno(stdout), "wb9");
			if (gz == NULL)
				printf("error main: gzdopen");
			else
				printf("Content-Encoding: gzip\r\n");
		}
	}

	char fn[1024];
	
	printf("%s\r\n\r\n", rssroll.ct_html);
	fflush(stdout);
	snprintf(fn, sizeof(fn), "%s/main.html", rssroll.htmldir);
	render_html(fn, &render_front, NULL);

done:
	if (gz != NULL) {
		if (gzclose(gz) != Z_OK)
			printf("error main: gzclose");
		gz = NULL;
	} else
		fflush(stdout);

	config_queue_purge();
	sqlite3_close(g.db);
	return (0);
}
