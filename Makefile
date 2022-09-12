#
SUBDIR= src tests

LOCALBASE=	/var/www
DATADIR=	/opt/rssroll
WEBDIR=		/htdocs/rssroll.chaosophia.net
BINDIR=		/bin
LIBEXECDIR=	/libexec

.if exists(src/rssroll)
.if exists(src/index.cgi)
BINLDD=		ldd src/rssroll | grep "=>" | cut -d ' ' -f 3
CGILDD=		ldd src/index.cgi | grep "=>" | cut -d ' ' -f 3

BINlibs=	${BINLDD:sh}
CGIlibs=	${CGILDD:sh}
.endif
.endif

SUBDIR_TARGETS+=	test

chroot:
	mkdir -p $(LOCALBASE)$(LIBDIR)
	mkdir -p $(LOCALBASE)$(DATADIR)
	mkdir -p $(LOCALBASE)$(WEBDIR)
	mkdir -p $(LOCALBASE)$(CONFDIR)
	mkdir -p $(LOCALBASE)$(BINDIR)
	mkdir -p $(LOCALBASE)$(LIBEXECDIR)
	cp /libexec/ld-elf.so.1 $(LOCALBASE)$(LIBEXECDIR)/
.	for l in ${BINlibs}
	cp -f ${l} $(LOCALBASE)$(LIBDIR)/
.	endfor
.	for l in ${CGIlibs}
	cp -f ${l} $(LOCALBASE)$(LIBDIR)/
.	endfor

install:
	rm -rf $(LOCALBASE)$(DATADIR)/html
	rm -rf $(LOCALBASE)$(WEBDIR)/css
	cp src/index.cgi $(LOCALBASE)$(WEBDIR)/
	cp src/rssroll $(LOCALBASE)$(BINDIR)/
	cp etc/rssrollrc $(LOCALBASE)$(CONFDIR)/rssrollrc.sample
	cp -r html $(LOCALBASE)$(DATADIR)/
	cp -r css $(LOCALBASE)$(WEBDIR)/

testlive:
	chroot -u www -g www $(LOCALBASE) $(BINDIR)/rssroll
	chroot -u www -g www $(LOCALBASE) $(WEBDIR)/index.cgi

.include <bsd.subdir.mk>
