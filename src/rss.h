/*
rss.h - RSS (and Atom) parser and generator (using libxml2)

Copyright (c) 2018 Nikola Kolev <koue@chaosophia.net>
Copyright (c) 2006 NoisyB


This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#ifndef RSS_H
#define RSS_H

/* increase this number if rss files have more then 32 items */
#define RSSMAXITEM 32
/* examine the rss items lenghts and change this value if its necessary. */
#define RSSMAXBUFSIZE 131072

// version id's
enum {
	RSS_V0_90 = 1,
	RSS_V0_91,
	RSS_V0_92,
	RSS_V0_93,
	RSS_V0_94,
	RSS_V1_0,
	RSS_V2_0,
	ATOM_V0_1,
	ATOM_V0_2,
	ATOM_V0_3,
};

typedef struct {
	char title[256];
	char url[256];
	char desc[RSSMAXBUFSIZE];
	time_t date;
} st_rss_item_t;


typedef struct {
	int version;		// version of the feed
	// feed information
	char title[256];
	char url[256];
	char desc[RSSMAXBUFSIZE];
	time_t date;
	st_rss_item_t item[RSSMAXITEM];
	int item_count;
} st_rss_t;

int rss_demux(const char *fname);

st_rss_t *rss_open(const char *fname);
int rss_close(st_rss_t *rss);

extern int debug;
void dmsg(int, const char *fmt, ...);

#endif //  RSS_H
