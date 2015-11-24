#
# Makefile for rssroll
#

bin_PROGRAMS = rssroll index.cgi

# rssroll
rssroll_SOURCES =       rssroll.c \
                        rss.c rss.h

rssroll_CFLAGS = $(XML2_CFLAGS) $(SQLITE3_CFLAGS) $(LIBCURL_CPPFLAGS) -Wall -Wstrict-prototypes
rssroll_LDADD = $(XML2_LDFLAGS) $(LIBCURL) $(SQLITE3_LDFLAGS)

# index
index_cgi_SOURCES =	index.c \
                        cgi.c cgi.h \
                        configfile.c configfile.h

index_cgi_CFLAGS = $(SQLITE3_CFLAGS) -Wall -Wstrict-prototypes
index_cgi_LDADD = $(SQLITE3_LDFLAGS) -lz

EXTRA_DIST = rssrollrc html css
