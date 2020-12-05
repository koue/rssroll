#!/bin/sh
set -e
rm -f rssrolltest.db
sqlite3 rssrolltest.db < ../scripts/create_database.sql
sqlite3 rssrolltest.db "INSERT INTO categories (title) VALUES ('test')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/atom.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss091.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss092.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss10.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss20.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/notexist.xml')"
valgrind -q --tool=memcheck --leak-check=yes --num-callers=20 ../src/rssroll -v -d rssrolltest.db
