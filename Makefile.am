#
# Makefile for rssroll
#

bin_PROGRAMS = rssroll index.cgi

# rssroll
rssroll_SOURCES =       rssroll.c \
                        rss.c rss.h

rssroll_CFLAGS = $(XML2_CFLAGS) $(SQLITE3_CFLAGS) $(CURL_CFLAGS) -Wall -Wstrict-prototypes
rssroll_LDADD = $(XML2_LDFLAGS) $(CURL_LDFLAGS) $(SQLITE3_LDFLAGS)

# index
index_cgi_SOURCES =	index.c \
                        cgi.c cgi.h \
                        configfile.c configfile.h

index_cgi_CFLAGS = $(SQLITE3_CFLAGS) -Wall -Wstrict-prototypes
index_cgi_LDADD = $(SQLITE3_LDFLAGS) -lz

dist_noinst_SCRIPTS = autogen.sh
EXTRA_DIST = autogen.sh rssrollrc html css
