CC=gcc -Wall -Wstrict-prototypes -g -O0

all: rssroll index.cgi

rssroll: rssroll.c rss.o
	$(CC) -I /usr/local/include/libxml2 -I /usr/local/include -L /usr/local/lib rssroll.c -o rssroll -lxml2 -lsqlite3 -lcurl rss.o 

rss.o: rss.c rss.h
	$(CC) -static -I /usr/local/include/libxml2 -I /usr/local/include -c rss.c

index.cgi: index.c cgi.o
	$(CC) -I /usr/local/include -L /usr/local/lib -lsqlite3 -lz cgi.o index.c -o index.cgi

cgi.o: cgi.c cgi.h
	$(CC) -static -c cgi.c
clean:
	rm -f rssroll *.o *.core index.cgi
