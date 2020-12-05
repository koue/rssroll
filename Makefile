#
SUBDIR= src tests

LOCALBASE=	/var/www
DATADIR=	/opt/rssroll
WEBDIR=		/htdocs/rssroll.chaosophia.net
BINDIR=		/bin
LIBEXECDIR=	/libexec
# TESTS
VALGRINDCMD=    valgrind -q --tool=memcheck --leak-check=yes --num-callers=20
TESTCMD=	./src/index.cgi --valgrind

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
	cp rssrollrc $(LOCALBASE)$(CONFDIR)/rssrollrc.sample
	cp -r html $(LOCALBASE)$(DATADIR)/
	cp -r css $(LOCALBASE)$(WEBDIR)/

test:
	echo
	echo "===== Run html tests ====="
	# default html
	QUERY_STRING='' $(TESTCMD) > tests/test.file
	diff -q tests/html/default.html tests/test.file
	QUERY_STRING='' $(VALGRINDCMD) $(TESTCMD) | grep DOCTYPE
	# category 1 html
	QUERY_STRING='1' $(TESTCMD) > tests/test.file
	diff -q tests/html/category1.html tests/test.file
	QUERY_STRING='1' $(VALGRINDCMD) $(TESTCMD) | grep DOCTYPE
	# category 1, page2 html
	QUERY_STRING='1/10' $(TESTCMD) > tests/test.file
	diff -q tests/html/category1-page2.html tests/test.file
	QUERY_STRING='1/10' $(VALGRINDCMD) $(TESTCMD) | grep DOCTYPE
	# category 1, page3 html
	QUERY_STRING='1/20' $(TESTCMD) > tests/test.file
	diff -q tests/html/category1-page3.html tests/test.file
	QUERY_STRING='1/20' $(VALGRINDCMD) $(TESTCMD) | grep DOCTYPE
	# category 1, nomore html
	QUERY_STRING='1/30' $(TESTCMD) > tests/test.file
	diff -q tests/html/category1-nomore.html tests/test.file
	QUERY_STRING='1/30' $(VALGRINDCMD) $(TESTCMD) | grep DOCTYPE
	# category 2 html
	QUERY_STRING='2' $(TESTCMD) > tests/test.file
	diff -q tests/html/category2.html tests/test.file
	QUERY_STRING='2' $(VALGRINDCMD) $(TESTCMD) | grep DOCTYPE
	# no category html
	QUERY_STRING='3' $(TESTCMD) > tests/test.file
	diff -q tests/html/nocategory.html tests/test.file
	QUERY_STRING='3' $(VALGRINDCMD) $(TESTCMD) | grep DOCTYPE
	# wrong query html
	QUERY_STRING='zxc/10' $(TESTCMD) > tests/test.file
	diff -q tests/html/wrongquery.html tests/test.file
	QUERY_STRING='zxc/10' $(VALGRINDCMD) $(TESTCMD) | grep 400
	# wrong query html
	QUERY_STRING='./10' $(TESTCMD) > tests/test.file
	diff -q tests/html/wrongquery.html tests/test.file
	QUERY_STRING='./10' $(VALGRINDCMD) $(TESTCMD) | grep 400
	# wrong query html
	QUERY_STRING='&amp;10' $(TESTCMD) > tests/test.file
	diff -q tests/html/wrongquery.html tests/test.file
	QUERY_STRING='&amp;10' $(VALGRINDCMD) $(TESTCMD) | grep 400
	# remove test file
	rm -f tests/test.file
	echo "OK"
	echo "===== Done ====="

testlive:
	chroot -u www -g www $(LOCALBASE) $(BINDIR)/rssroll
	chroot -u www -g www $(LOCALBASE) $(WEBDIR)/index.cgi

.include <bsd.subdir.mk>
