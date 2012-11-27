/*
 *
 * Copyright (c) 2004-2006 Daniel Hartmeier. All rights reserved.
 * Copyright (c) 2012 Nikola Kolev. All rights reserved.
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
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include <sqlite3.h>

#include "cgi.h"
#include "rss.h"

static char		*baseurl = "https://blogroll.chaosophia.net/";
static char		*ct_html = "Content-Type: text/html; charset=utf-8";
static char		*htmldir = "html";
static char		*mailaddr = "koue@chaosophia.net";
static sqlite3*		db;		
static char		*dbname = "/srv/rss.db";
static struct query	*q = NULL;
static gzFile		gz = NULL;

typedef	void (*render_cb)(const char *, const st_rss_item_t *);

static void	 render_error(const char *fmt, ...);
static int	 render_html(const char *html_fn, render_cb r, const st_rss_item_t *e);
static void	 render_front(const char *m, const st_rss_item_t *e);
static void	 render_front_feed(const char *m, const st_rss_item_t *e);
static char	*html_esc(const char *s, char *d, size_t len, int allownl);
static const char *rfc822_time(time_t t);

void
chomp(char *s) {
        while (*s && *s != '\n' && *s != '\r')
                s++;
        *s = 0;
}

static void
dprintf(const char *fmt, ...)
{
	static char s[65536];
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);
	if (r < 0 || r >= sizeof(s))
		printf("error dprintf: vsnprintf: r %d (%d)", r, (int)sizeof(s));
	if (gz != NULL) {
		r = gzputs(gz, s);
		if (r != strlen(s))
			printf("error dprintf: gzputs: r %d (%d)",
			    r, (int)strlen(s));
	} else
		fprintf(stdout, "%s", s);
}

int
trace_feeds_callback (void *p_data, int num_fields, char **p_fields, char **p_col_names) 
{
	/*
		p_fields[0]	-	id
		p_fields[1]	-	modified
		p_fields[2]	-	link
		p_fields[3]	-	title
		p_fields[4]	-	description
		p_fields[5]	-	pubdate
	*/
	st_rss_item_t	rss_item;	
	char	fn[1024];
	
	snprintf(fn, sizeof(fn), "%s/feed.html", htmldir);

//	printf("%s\n", p_fields[5]);
	snprintf(rss_item.title, sizeof(rss_item.title), "%s", p_fields[3]);
	snprintf(rss_item.url, sizeof(rss_item.url), "%s", p_fields[2]);
	snprintf(rss_item.desc, sizeof(rss_item.desc), "%s", p_fields[4]);
	rss_item.date = strtol(p_fields[5], NULL, 0);
//	snprintf(rss_item.date, sizeof(rss_item.date), "%s", p_fields[5]);

	render_html(fn, &render_front_feed, &rss_item);
	//render_html(fn, NULL, NULL);
	
	return 0;
}


static void
render_error(const char *fmt, ...)
{
	va_list ap;
	char s[8192], e[8192];

	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);
	printf("%s\r\n\r\n", ct_html);
	fflush(stdout);
	dprintf("<html><head><title>Error</title></head><body>\n");
	dprintf("<h2>Error</h2><p><b>%s</b><p>\n", s);
	if (q != NULL) {
		dprintf("Request: <b>%s</b><br>\n",
		    html_esc(q->query_string, e, sizeof(e), 0));
		dprintf("Address: <b>%s</b><br>\n",
		    html_esc(q->remote_addr, e, sizeof(e), 0));
		if (q->user_agent != NULL)
			dprintf("User agent: <b>%s</b><br>\n",
			    html_esc(q->user_agent, e, sizeof(e), 0));
		if (q->referer != NULL)
			dprintf("Referer: <b>%s</b><br>\n",
			    html_esc(q->referer, e, sizeof(e), 0));
	}
	dprintf("Time: <b>%s</b><br>\n", rfc822_time(time(0)));
	dprintf("<p>If you believe this is a bug in <i>this</i> server, "
	    "please send reports with instructions about how to "
	    "reproduce to <a href=\"mailto:%s\"><b>%s</b></a><p>\n",
	    mailaddr, mailaddr);
	dprintf("</body></html>\n");
}

static int
render_html(const char *html_fn, render_cb r, const st_rss_item_t *e)
{
	FILE *f;
	char s[8192];

	if ((f = fopen(html_fn, "r")) == NULL) {
		dprintf("ERROR: fopen: %s: %s<br>\n", html_fn, strerror(errno));
		return (1);
	}
	while (fgets(s, sizeof(s), f)) {
		char *a, *b;

		for (a = s; (b = strstr(a, "%%")) != NULL;) {
			*b = 0;
			dprintf("%s", a);
			a = b + 2;
			if ((b = strstr(a, "%%")) != NULL) {
				*b = 0;
				if (!strcmp(a, "BASEURL"))
					dprintf("%s", baseurl);
				else if (!strcmp(a, "CTYPE"))
					dprintf("%s", ct_html);
				else if (r != NULL)
					(*r)(a, e);
				a = b + 2;
			}
		}
		dprintf("%s", a);
	}
	fclose(f);
	return (0);
}

static void
render_front(const char *m, const st_rss_item_t *e)
{
	char fn[1024], *errmsg;
	/*
		catid = 1 - tech
		catid = 2 - blog
		catid = 3 - news
	*/
	char *query = "SELECT id, modified, link, title, description, pubdate from feeds where chanid IN (select id from channels where catid = 2) order by id desc limit 10";

	if (!strcmp(m, "FEEDS")) {
		snprintf(fn, sizeof(fn), "%s/feed.html", htmldir);
		if(sqlite3_exec(db, query, trace_feeds_callback, 0, &errmsg) != SQLITE_OK) {
			render_error("cannot load database");
		}
	} else if (!strcmp(m, "HEADER")) {
		snprintf(fn, sizeof(fn), "%s/header.html", htmldir);
		render_html(fn, NULL, NULL);
	} else if (!strcmp(m, "FOOTER")) {
		snprintf(fn, sizeof(fn), "%s/footer.html", htmldir);
		render_html(fn, NULL, NULL);
	} else if (!strcmp(m, "CONTENT")) {
		snprintf(fn, sizeof(fn), "%s/content.html", htmldir);
		render_html(fn, NULL, NULL);
	} else
		dprintf("render_front: unknown macro '%s'<br>\n", m);
}

static void
render_front_feed(const char *m, const st_rss_item_t *e)
{
	if (!strcmp(m, "PUBDATE")) {
		dprintf("%s", ctime(&e->date));
	} else  if (!strcmp(m, "TITLE")) {
		dprintf("%s", e->title);
	} else if (!strcmp(m, "DESCRIPTION")) {
		dprintf("%s", e->desc);
	} else if (!strcmp(m, "URL")) {
		dprintf("%s", e->url);
	} else
		dprintf("render_front_feed: unknown macro '%s'<br>\n", m);
}

static char *
html_esc(const char *s, char *d, size_t len, int allownl)
{
	size_t p;

	for (p = 0; *s && p < len - 1; ++s)
		switch (*s) {
		case '&':
			if (p < len - 5) {
				strlcpy(d + p, "&amp;", 6);
				p += 5;
			}
			break;
		case '\"':
			if (p < len - 6) {
				strlcpy(d + p, "&quot;", 7);
				p += 6;
			}
			break;
		case '<':
			if (p < len - 4) {
				strlcpy(d + p, "&lt;", 5);
				p += 4;
			}
			break;
		case '>':
			if (p < len - 4) {
				strlcpy(d + p, "&gt;", 5);
				p += 4;
			}
			break;
		case '\r':
		case '\n':
			if (!allownl) {
				/* skip */
				break;
			} else if (allownl > 1 && *s == '\r') {
				if (p < len - 4) {
					strlcpy(d + p, "<br>", 5);
					p += 4;
				}
				break;
			}
			/* else fall through */
		default:
			d[p++] = *s;
		}
	d[p] = 0;
	return (d);
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

int 
main(int argc, char *argv[])
{
	const char *s;
	time_t if_modified_since = 0;
	int i;

	umask(007);
	/*if (chdir("/tmp")) {
		printf("error main: chdir: /tmp: %s", strerror(errno));
		render_error("chdir: /tmp: %s", strerror(errno));
		goto done;
	} */
	sqlite3_open(dbname, &db);

	if ((q = get_query()) == NULL) {
		render_error("get_query");
		printf("error main: get_query() NULL");
		goto done;
	}
	if ((s = getenv("QUERY_STRING")) != NULL) {
		if (strlen(s) > 64) {
			printf("Status: 400\r\n\r\n You are trying to send very long query!\n");
			fflush(stdout);
			return (0);

		} else if (strstr(s, "&amp;") != NULL) {
			printf("warning main: escaped query '%s', user agent '%s', "
			    "referer '%s'", s,
			    q->user_agent ? q->user_agent : "(null)",
			    q->referer ? q->referer : "(null)");
			printf("Status: 400\r\n\r\nHTML escaped ampersand in cgi "
			    "query string \"%s\"\n"
			    "This might be a problem in your client \"%s\",\n"
			    "or in the referring document \"%s\"\n"
			    "See http://www.htmlhelp.org/tools/validator/problems.html"
			    "#amp\n", s, q->user_agent ? q->user_agent : "",
			    q->referer ? q->referer : "");
			fflush(stdout);
			return (0);
		} else {
			for (i = 0; i < strlen(s); i++) {
				/* sanity check of the query string, accepts only alpha, '/' and '_' and '.' if its on 5 position before the end of the string
					Correct: /follow/this/path/
						 /or/this/
						 /and/this/if/its/single/article.html
				*/
                        	if ((!isalpha(s[i])) && (!isdigit(s[i])) && (s[i] != '/') && (s[i] != '_')) {
					if ((i == (strlen(s)-5)) && (s[i] == '.'))
						continue;
                                	printf("Status: 400\r\n\r\nYou are trying to send wrong query!\n");
	                                fflush(stdout);
        	                        return (0);
                	        }
                	}
		}	
	}

	if ((q->referer != NULL && strstr(q->referer, "morisit")) ||
	    (s != NULL && strstr(s, "http://"))) {
		printf("Status: 503\r\n\r\nWe are not redirecting, "
		    "nice try.\n");
		fflush(stdout);
		return (0);
	}
	if (q->user_agent != NULL && !strncmp(q->user_agent, "Googlebot", 9)) {
		printf("Status: 503\r\n\r\nGooglebot you are not.\n");
		fflush(stdout);
		return (0);
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
	
	printf("%s\r\n\r\n", ct_html);
	fflush(stdout);
	snprintf(fn, sizeof(fn), "%s/main.html", htmldir);
	render_html(fn, &render_front, NULL);

done:
	if (gz != NULL) {
		if (gzclose(gz) != Z_OK)
			printf("error main: gzclose");
		gz = NULL;
	} else
		fflush(stdout);
	if (q != NULL)
		free_query(q);

	sqlite3_close(db);
	return (0);
}
