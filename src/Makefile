#
PROGS=		rssroll index.cgi

SRCS.rssroll=	rssroll.c rss.c
SRCS.index.cgi=	index.c

CFLAGS+=	-Werror \
		-I./ \
		-I/usr/local/include \
		-I/usr/local/include/libxml2
LDFLAGS+=	-L/usr/local/lib
LDADD.rssroll=	-lz -lcezfossil -lsqlite3 -lxml2 -lcurl
LDADD.index.cgi=-lz -lcezqueue -lcezfossil -lcezmisc -lsqlite3

MAN=

MK_DEBUG_FILES=	no

MAKEOBJDIR=	.

install:

test:

.include <bsd.progs.mk>
