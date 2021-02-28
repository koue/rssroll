#!/bin/sh
set -eo pipefail

### Variables
VALGRINDCMD="valgrind -q --tool=memcheck --leak-check=yes --num-callers=20"
TESTCMD="../src/index.cgi --valgrind"
SQLITERUN="sqlite3 rssrolltest.db"

### Functions
_print_header() {
    echo
    echo "===== Run ${1} test ====="
}

_print_footer() {
    echo "OK"
    echo "===== Done ====="
}

# clean database
_clean() {
    rm -f rssrolltest.db
}

_db_create() {
    sqlite3 rssrolltest.db < ../scripts/database_create.sql
}

_db_load() {
    sqlite3 rssrolltest.db "INSERT INTO tags (title) VALUES ('test1')"
    sqlite3 rssrolltest.db "INSERT INTO tags (title) VALUES ('test2')"
    sqlite3 rssrolltest.db "INSERT INTO channels (tagid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/atom.xml')"
    sqlite3 rssrolltest.db "INSERT INTO channels (tagid, link) VALUES (2, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss091.xml')"
    sqlite3 rssrolltest.db "INSERT INTO channels (tagid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss092.xml')"
    sqlite3 rssrolltest.db "INSERT INTO channels (tagid, link) VALUES (2, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss10.xml')"
    sqlite3 rssrolltest.db "INSERT INTO channels (tagid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss20.xml')"
    sqlite3 rssrolltest.db "INSERT INTO channels (tagid, link) VALUES (2, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/notexist.xml')"
}

### Valgrind test
_test_valgrind() {
    _print_header valgrind
    ${VALGRINDCMD} ../src/rssroll -d rssrolltest.db
    _print_footer
}

### DB queries test
_runquery() {
    QUERY=`echo "${1}" | cut -d ';' -f 1`
    RESULT=`echo "${1}" | cut -d ';' -f 2`

    if [ `${SQLITERUN} "${QUERY}"` != "${RESULT}" ]
    then
        echo " '${QUERY}' failed"
	exit 1
    fi
}

_test_db() {
    _print_header db
    _runquery "SELECT COUNT(*) FROM tags;2"
    _runquery "SELECT title FROM tags WHERE id=2;test2"
    _runquery "SELECT COUNT(*) FROM channels;6"
    _runquery "SELECT COUNT(*) FROM channels WHERE tagid=1;3"
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
    _print_footer
}

### HTML tests
_runhtml() {
    echo "$1"
    QUERY_STRING=`echo "${1}" | cut -d ':' -f 1`
    TEMPLATE=`echo "${1}" | cut -d ':' -f 2`
    RESULT=`echo "${1}" | cut -d ':' -f 3`

    QUERY_STRING=${QUERY_STRING} ${TESTCMD} > test.file
    diff -q "${TEMPLATE}" test.file
    QUERY_STRING=${QUERY_STRING} ${VALGRINDCMD} ${TESTCMD} | ${RESULT}
}

_test_html() {
    _print_header html
    # channel 1 html
    _runhtml "0/1:html/channel1.template:grep DOCTYPE"
    # channel 4 html
    _runhtml "0/4:html/channel4.template:grep DOCTYPE"
    # default html
    _runhtml ":html/default.template:grep DOCTYPE"
    # tag 1 html
    _runhtml "1:html/tag1.template:grep DOCTYPE"
    # tag 1, page2 html
    _runhtml "1/10:html/tag1-page2.template:grep DOCTYPE"
    # tag 1, page3 html
    _runhtml "1/20:html/tag1-page3.template:grep DOCTYPE"
    # tag 1, nomore html
    _runhtml "1/30:html/tag1-nomore.template:grep DOCTYPE"
    # tag 2 html
    _runhtml "2:html/tag2.template:grep DOCTYPE"
    # no tag html
    _runhtml "3:html/notag.template:grep DOCTYPE"
    # wrong query html
    _runhtml "zxc/10:html/wrongquery.template:grep 400"
    # wrong query html
    _runhtml "./10:html/wrongquery.template:grep 400"
    # wrong query html
    _runhtml "&amp;10:html/wrongquery.template:grep 400"
    # remove test file
    rm -f test.file
    _print_footer
}

# Start
_clean
_db_create
_db_load
_test_valgrind
_test_db
_test_html
