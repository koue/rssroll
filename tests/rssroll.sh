#!/bin/sh
set -eo pipefail

### Variables
VALGRINDCMD="valgrind -q --tool=memcheck --leak-check=yes --num-callers=20"
TESTCMD="../src/index.cgi --valgrind"

###
# clean database
rm -f rssrolltest.db

sqlite3 rssrolltest.db < ../scripts/create_database.sql
sqlite3 rssrolltest.db "INSERT INTO categories (title) VALUES ('test1')"
sqlite3 rssrolltest.db "INSERT INTO categories (title) VALUES ('test2')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/atom.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (2, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss091.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss092.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (2, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss10.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss20.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (2, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/notexist.xml')"

### Valgrind test
${VALGRINDCMD} ../src/rssroll -d rssrolltest.db

### DB queries test
SQLITERUN="sqlite3 rssrolltest.db"
_runquery() {
    QUERY=`echo "${1}" | cut -d ';' -f 1`
    RESULT=`echo "${1}" | cut -d ';' -f 2`

    if [ `${SQLITERUN} "${QUERY}"` != "${RESULT}" ]
    then
        echo " '${QUERY}' failed"
	exit 1
    fi
}

echo
echo "===== Run DB tests ====="
_runquery "SELECT COUNT(*) FROM categories;2"
_runquery "SELECT title FROM categories WHERE id=2;test2"
_runquery "SELECT COUNT(*) FROM channels;6"
_runquery "SELECT COUNT(*) FROM channels WHERE catid=1;3"
_runquery "SELECT COUNT(*) FROM feeds;28"
_runquery "SELECT COUNT(*) FROM feeds WHERE chanid=3;10"
_runquery "SELECT COUNT(*) FROM feeds WHERE chanid=5;9"
_runquery "SELECT modified FROM feeds WHERE id=15;0"
_runquery "SELECT modified FROM feeds WHERE id=25;0"
_runquery "SELECT pubdate FROM feeds WHERE id=1;1122812940"
_runquery "SELECT pubdate FROM feeds WHERE id=2;0"
_runquery "SELECT pubdate FROM feeds WHERE id=15;0"
_runquery "SELECT pubdate FROM feeds WHERE id=26;1033350720"
_runquery "SELECT COUNT(*) FROM feeds WHERE pubdate=0;18"
_runquery "SELECT id FROM feeds WHERE title LIKE 'Rule 1';21"
_runquery "SELECT id FROM feeds WHERE title LIKE 'Law and Order';22"
_runquery "SELECT COUNT(*) FROM feeds WHERE description LIKE '%Tool%';3"
_runquery "SELECT COUNT(*) FROM feeds WHERE title IS '(NULL)';6"
_runquery "SELECT COUNT(*) FROM feeds WHERE title IS NOT '(NULL)';22"
_runquery "SELECT COUNT(*) FROM feeds WHERE link LIKE '%backissues%';9"
_runquery "SELECT COUNT(*) FROM feeds WHERE description LIKE '%ok%';4"
echo "OK"
echo "===== Done ====="

### HTML tests
_runhtml() {
    QUERY_STRING=`echo "${1}" | cut -d ':' -f 1`
    TEMPLATE=`echo "${1}" | cut -d ':' -f 2`
    RESULT=`echo "${1}" | cut -d ':' -f 3`

    ${TESTCMD} > test.file
    diff -q "${TEMPLATE}" test.file
    ${VALGRINDCMD} ${TESTCMD} | ${RESULT}
}

echo
echo "===== Run html tests ====="
# default html
_runhtml ":html/default.template:grep DOCTYPE"
# category 1 html
_runhtml "1:html/category1.template:grep DOCTYPE"
# category 1, page2 html
_runhtml "1/10:html/category1-page2.template:grep DOCTYPE"
# category 1, page3 html
_runhtml "1/20:html/category1-page3.template:grep DOCTYPE"
# category 1, nomore html
_runhtml "1/30:html/category1-nomore.template:grep DOCTYPE"
# category 2 html
_runhtml "2:html/category2.template:grep DOCTYPE"
# no category html
_runhtml "3:html/nocategory.template:grep DOCTYPE"
# wrong query html
_runhtml "zxc/10:html/wrongquery.template:grep 400"
# wrong query html
_runhtml "./10:html/wrongquery.template:grep 400"
# wrong query html
_runhtml "&amp;10:html/wrongquery.template:grep 400"
# remove test file
rm -f test.file
echo "OK"
echo "===== Done ====="
