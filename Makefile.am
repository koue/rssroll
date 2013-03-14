#
# Makefile for rssroll
#

bin_PROGRAMS = rssroll index

# rssroll
rssroll_SOURCES = rss.c rss.h \
                        rssroll.c rssroll.h
rssroll_CFLAGS = $(XML2_CFLAGS) $(SQLITE3_CFLAGS) -I/usr/include/curl
rssroll_LDADD = $(XML2_LDFLAGS) -lcurl $(SQLITE3_LDFLAGS)

# index
index_SOURCES = cgi.c cgi.h \
                configfile.c configfile.h \
                index.c index.h

index_CFLAGS = $(SQLITE3_CFLAGS)
index_LDADD = $(SQLITE3_LDFLAGS) -lz

dist_noinst_SCRIPTS = autogen.sh

#CFLAGS=-Wall -Wstrict-prototypes -g -O0 -I/usr/local/include -I/usr/local/include/libxml2
#LDFLAGS=-L/usr/local/lib
#CC=gcc

# all: rssroll index.cgi

# rssroll: rssroll.c rss.o
#       $(CC) $(CFLAGS) $(LDFLAGS) rssroll.c -o rssroll -lxml2 -lsqlite3 -lcurl rss.o

# rss.o: rss.c rss.h
#       $(CC) $(CFLAGS) -static -c rss.c

# index.cgi: index.c cgi.o configfile.o
#       $(CC) $(CFLAGS) $(LDFLAGS) -lsqlite3 -lz cgi.o configfile.o index.c -o index.cgi

# cgi.o: cgi.c cgi.h
#       $(CC) $(CFLAGS) -static -c cgi.c

# configfile.o: configfile.c configfile.h
#       $(CC) $(CFLAGS) -static -c configfile.c

# install:
#       cp rssroll /usr/local/bin
#       cp index.cgi /usr/local/www/cgi-bin/rssroll.cgi
#       cp rssrollrc /usr/local/etc
#       mkdir /usr/local/www/data/rssroll
#       cp -r html css /usr/local/www/data/rssroll

# clean:
#       rm -f rssroll *.o *.core index.cgi

# remove:
#       rm -rf /usr/local/bin/rssroll /usr/local/www/cgi-bin/rssroll.cgi /usr/local/etc/rssrollrc /usr/local/www/data/rssroll
