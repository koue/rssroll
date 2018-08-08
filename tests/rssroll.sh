#!/bin/sh
set -e
sqlite3 rssrolltest.db < ../../scripts/create_database.sql
sqlite3 rssrolltest.db "INSERT INTO categories (title) VALUES ('test')"
sqlite3 rssrolltest.db "INSERT INTO channels (catid, link) VALUES (1, 'http://koue.chaosophia.net/index.cgi?/rss')"
../src/rssroll -v -d rssrolltest.db
