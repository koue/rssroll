#
PROGS=		rssroll index.cgi

SRCS.rssroll=	rssroll.c rss.c item.c xml.c
SRCS.index.cgi=	index.c item.c

CFLAGS+=	-Werror \
		-I./ \
		-I/usr/local/include \
		-I/usr/local/include/libxml2
LDFLAGS+=	-L/usr/local/lib
LDADD.rssroll=	-lz -lfsldb -lfslbase -lsqlite3 -lxml2 -lcezcorepool -lfetch
LDADD.index.cgi=-lcezqueue -lfsldb -lfslbase -lcezmisc -lsqlite3 -lcezcorepool \
		-lrender

MAN=

MK_DEBUG_FILES=	no

MAKEOBJDIR=	.

install:

test:

.include <bsd.progs.mk>
