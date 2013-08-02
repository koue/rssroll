/*
 *
 * Copyright (c) 2004-2006 Daniel Hartmeier. All rights reserved.
 * Copyright (c) 2012, 2013 Nikola Kolev. All rights reserved.
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
#include "configfile.h"

static sqlite3*		db;		
static char		query_string[10];
static char		query_category[10];
static char		query_limit[256];
static struct query	*q = NULL;
static gzFile		gz = NULL;
static char		*rssrollrc = "/usr/local/etc/rssrollrc";

struct index_params {
	int	feeds;
	int	defcat;
	char    url[256];
	char    name[256];
	char    dbpath[256];
	char    desc[256];
	char    owner[256];
	char	ct_html[256];
	char	htmldir[256];
};


static struct index_params	rssroll[1];

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

void load_default_config(void){
	snprintf(rssroll->url, sizeof(rssroll->url), "http://rssroll.example.com");
	snprintf(rssroll->name, sizeof(rssroll->name), "rssroll");
	snprintf(rssroll->dbpath, sizeof(rssroll->dbpath), "/usr/local/etc/rssroll.db");
	snprintf(rssroll->desc, sizeof(rssroll->desc), "rssroll description");
	snprintf(rssroll->owner, sizeof(rssroll->owner), "dont@blame.me");
	snprintf(rssroll->ct_html, sizeof(rssroll->ct_html), "Content-Type: text/html; charset=utf-8");
	snprintf(rssroll->htmldir, sizeof(rssroll->htmldir), "html");
	rssroll->feeds = 10;
	rssroll->defcat = 1;
}

void config_cb (const char *name, const char *value) {
	if (!strcmp(name, "url"))
		snprintf(rssroll->url, sizeof(rssroll->url), "%s", value);
	else if (!strcmp(name, "name"))
		snprintf(rssroll->name, sizeof(rssroll->name), "%s", value);
	else if (!strcmp(name, "dbpath"))
		snprintf(rssroll->dbpath, sizeof(rssroll->dbpath), "%s", value);
	else if (!strcmp(name, "desc"))
		snprintf(rssroll->desc, sizeof(rssroll->desc), "%s", value);
	else if (!strcmp(name, "owner"))
		snprintf(rssroll->owner, sizeof(rssroll->owner), "%s", value);
	else if (!strcmp(name, "ct_html"))
		snprintf(rssroll->ct_html, sizeof(rssroll->ct_html), "%s", value);
	else if (!strcmp(name, "htmldir"))
		snprintf(rssroll->htmldir, sizeof(rssroll->htmldir), "%s", value);
	else if (!strcmp(name, "feeds"))
		rssroll->feeds = atoi(value);
	else if (!strcmp(name, "category"))
		rssroll->defcat = atoi(value);
}

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
		printf("error d_printf: vsnprintf: r %d (%d)", r, (int)sizeof(s));
	if (gz != NULL) {
		r = gzputs(gz, s);
		if (r != strlen(s))
			printf("error d_printf: gzputs: r %d (%d)",
			    r, (int)strlen(s));
	} else
		fprintf(stdout, "%s", s);
}

int
trace_categories_callback (void *p_data, int num_fields, char **p_fields, char **p_col_names)
{
	/*
		p_fields[0]	-	id
		p_fields[1]	-	title

		the rest are not used.
	*/
	d_printf("<p><a href='%s?%s'>%s</a></p>", rssroll->url, p_fields[0], p_fields[1]);

	return 0;
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
	
	snprintf(fn, sizeof(fn), "%s/feed.html", rssroll->htmldir);

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
	printf("%s\r\n\r\n", rssroll->ct_html);
	fflush(stdout);
	d_printf("<html><head><title>Error</title></head><body>\n");
	d_printf("<h2>Error</h2><p><b>%s</b><p>\n", s);
	if (q != NULL) {
		d_printf("Request: <b>%s</b><br>\n",
		    html_esc(q->query_string, e, sizeof(e), 0));
		d_printf("Address: <b>%s</b><br>\n",
		    html_esc(q->remote_addr, e, sizeof(e), 0));
		if (q->user_agent != NULL)
			d_printf("User agent: <b>%s</b><br>\n",
			    html_esc(q->user_agent, e, sizeof(e), 0));
		if (q->referer != NULL)
			d_printf("Referer: <b>%s</b><br>\n",
			    html_esc(q->referer, e, sizeof(e), 0));
	}
	d_printf("Time: <b>%s</b><br>\n", rfc822_time(time(0)));
	d_printf("<p>If you believe this is a bug in <i>this</i> server, "
	    "please send reports with instructions about how to "
	    "reproduce to <a href=\"mailto:%s\"><b>%s</b></a><p>\n",
	    rssroll->owner, rssroll->owner);
	d_printf("</body></html>\n");
}

static int
render_html(const char *html_fn, render_cb r, const st_rss_item_t *e)
{
	FILE *f;
	char s[8192];

	if ((f = fopen(html_fn, "r")) == NULL) {
		d_printf("ERROR: fopen: %s: %s<br>\n", html_fn, strerror(errno));
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
					d_printf("%s", rssroll->url);
				else if (!strcmp(a, "RSSROLL_NAME"))
					d_printf("%s", rssroll->name);
				else if (!strcmp(a, "RSSROLL_OWNER"))
					d_printf("%s", rssroll->owner);
				else if (!strcmp(a, "RSSROLL_CTYPE"))
					d_printf("%s", rssroll->ct_html);
				else if (!strcmp(a, "RSSROLL_CATEGORIES")){
					char *errmsg;
					if(sqlite3_exec(db, "SELECT id, title FROM categories", trace_categories_callback, 0, &errmsg) != SQLITE_OK) {
						render_error("cannot load database");
					}
				} else if (!strcmp(a, "PREV")) {
					d_printf("<a href='%s?%s/%ld'> <<< </a>", rssroll->url, query_category, strtol(query_limit, NULL, 0) + rssroll->feeds);
				} else if (!strcmp(a, "NEXT")) {
					if (strtol(query_limit, NULL, 0))
						d_printf("<a href='%s?%s/%ld'> >>> </a>", rssroll->url, query_category, strtol(query_limit, NULL, 0) - rssroll->feeds);
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
	char fn[1024], *errmsg;
	/*
		catid = 1 - tech
		catid = 2 - blog
		catid = 3 - news
	*/
	char feeds_query[256];

	sqlite3_snprintf(sizeof(feeds_query), feeds_query, "SELECT id, modified, link, title, description, pubdate from feeds where chanid IN (select id from channels where catid = '%q') order by id desc limit %q, %d", query_category, query_limit, rssroll->feeds);
	//char *feeds_query = "SELECT id, modified, link, title, description, pubdate from feeds where chanid IN (select id from channels where catid = 2) order by id desc limit 10";

	if (!strcmp(m, "FEEDS")) {
		if(sqlite3_exec(db, feeds_query, trace_feeds_callback, 0, &errmsg) != SQLITE_OK) {
			render_error("cannot load database");
		}
	} else if (!strcmp(m, "HEADER")) {
		snprintf(fn, sizeof(fn), "%s/header.html", rssroll->htmldir);
		render_html(fn, NULL, NULL);
	} else if (!strcmp(m, "FOOTER")) {
		snprintf(fn, sizeof(fn), "%s/footer.html", rssroll->htmldir);
		render_html(fn, NULL, NULL);
	} else
		d_printf("render_front: unknown macro '%s'<br>\n", m);
}

static void
render_front_feed(const char *m, const st_rss_item_t *e)
{
	if (!strcmp(m, "PUBDATE")) {
		d_printf("%s", ctime(&e->date));
	} else  if (!strcmp(m, "TITLE")) {
		d_printf("%s", e->title);
	} else if (!strcmp(m, "DESCRIPTION")) {
		d_printf("%s", e->desc);
	} else if (!strcmp(m, "URL")) {
		d_printf("%s", e->url);
	} else
		d_printf("render_front_feed: unknown macro '%s'<br>\n", m);
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
	const char *s, *query_args;
	time_t if_modified_since = 0;
	int i;

	umask(007);
	load_default_config();
	if (parse_configfile(rssrollrc, config_cb) == -1) {
		render_error("error: cannot open config file: %s", rssrollrc);	
		goto done;
	}
	/*if (chdir("/tmp")) {
		printf("error main: chdir: /tmp: %s", strerror(errno));
		render_error("chdir: /tmp: %s", strerror(errno));
		goto done;
	} */

	if (sqlite3_open(rssroll->dbpath, &db) != SQLITE_OK) {
		render_error("cannot load database: %s", rssroll->dbpath);
		goto done;
	}

	// default feeds
	snprintf(query_category, sizeof(query_string), "%d", rssroll->defcat);
	snprintf(query_limit, sizeof(query_limit), "0");

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
				/* 
					sanity check of the query string, accepts only alpha
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
			snprintf(query_category, sizeof(query_category), "%s", query_args);
			query_args = strtok(NULL, "/");
			if (query_args != NULL)
				snprintf(query_limit, sizeof(query_limit), "%s", query_args);
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
	
	printf("%s\r\n\r\n", rssroll->ct_html);
	fflush(stdout);
	snprintf(fn, sizeof(fn), "%s/main.html", rssroll->htmldir);
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
