#!/bin/sh
set -e
sqlite3 rssrolltest.db < ../scripts/create_database.sql
sqlite3 rssrolltest.db "INSERT INTO categories (title) VALUES ('test')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/atom.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss091.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss092.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss10.xml')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'https://raw.githubusercontent.com/koue/rssroll/develop/tests/rss20.xml')"
../src/rssroll -v -d rssrolltest.db
