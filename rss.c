/*
rss.h - RSS (and Atom) parser and generator (using libxml2)

Copyright (c) 2006 NoisyB
Copyright (c) 2012 Nikola Kolev


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

#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <time.h>
#include <unistd.h>
#include "rss.h"

#define DEBUG	0

time_t
strptime2 (const char *s)
{
  int i = 0;
  char y[100], m[100], d[100];
  char h[100], min[100];
//  char sec[100];
  struct tm time_tag;
  time_t t = time (0);
  const char *month_s[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};

  *y = *m = *d = *h = *min = 0;

  if (s[10] == 'T')                     // YYYY-MM-DDT00:00+00:00
    {
      sscanf (s, " %4s-%2s-%2sT%2s:%2s", y, m, d, h, min);
    }
  else if (s[3] == ',' && s[4] == ' ')  // Mon, 31 Jul 2006 15:05:00 GMT
    {
      sscanf (s + 5, "%2s %s %4s %2s:%2s", d, m, y, h, min);

      for (i = 0; month_s[i]; i++)
        if (!strcasecmp (m, month_s[i]))
          {
            sprintf (m, "%d", i + 1);
            break;
          }
    }
  else if (s[4] == '-' && s[7] == '-')  // 2006-07-19
    {
      sscanf (s, "%4s-%2s-%2s", y, m, d);
    }
  else                                  // YYYYMMDDTHHMMSS
    {
//      sscanf (s, " %4s%2s%2sT", y, m, d);
    }

  memset (&time_tag, 0, sizeof (struct tm));

  if (*y)
    time_tag.tm_year = strtol (y, NULL, 10) - 1900;
  if (*m)
    time_tag.tm_mon = strtol (m, NULL, 10) - 1;
  if (*d)
    time_tag.tm_mday = strtol (d, NULL, 10);
  if (*h)
    time_tag.tm_hour = strtol (h, NULL, 10);
  if (*min)
    time_tag.tm_min = strtol (min, NULL, 10);

  t = mktime (&time_tag);

  return t;
}


const unsigned char *
xml_get_value (xmlNode *n, const char *name) {

        if (n)
                if (xmlHasProp (n, (const unsigned char *) name))
                        return xmlGetProp (n, (const unsigned char *) name);
        return NULL;
}

void
rss_st_rss_t_sanity_check (st_rss_t *rss) 
{
	int i = 0;

	for (; i < rss->item_count; i++)
		printf ("pos: %d\n"
			"title: %s\n"
			"url: %s\n"
			"date: %ld\n"
			"desc: %s\n\n",
		i,
		rss->item[i].title,
		rss->item[i].url,
		rss->item[i].date,
		rss->item[i].desc);
	printf ("rss->item_count: %d\n\n", rss->item_count);
}

static void
rss_read_copy (char *d, xmlDoc *doc, xmlNode *n) {

	if(DEBUG) {
		printf("rss_read_copy in\n");
		fflush(stdout);
	}
// koue koue
//	printf("content - [%s]\n", (char *) xmlNodeGetContent(n));
//	printf("type - [%d]\n", n->type);
//	printf("xmlStrlen - [%d]\n", xmlStrlen(xmlNodeListGetString (n->doc, n, 1)));	
//	const char *p = (const char *) xmlNodeListGetString (n->doc, n, 1);
	const char *p = (const char *) xmlNodeGetContent(n);
	if (p)
			strncpy (d, p, RSSMAXBUFSIZE)[RSSMAXBUFSIZE-1] = 0;
	else
		*d = 0;
 	if(DEBUG) {
		printf("rss_read_copy p: %s\n", p);
		fflush(stdout);
	}
	if(DEBUG) {
		printf("rss_read_copy out\n");
		fflush(stdout);
	}
}

int
rss_close (st_rss_t *rss)
{
	if(DEBUG) {
		printf("rss_close\n");
		fflush(stdout);
	}

  if (rss)
    {
      free (rss);
      rss = NULL;
    }

  return 0;
} 

static st_rss_t *
rss_open_rss (st_rss_t *rss)
{
  xmlDoc *doc;
  xmlNode *node;
  int rdf = 0;

	if(DEBUG) {
		printf("rss_open_rss\n");
		fflush(stdout);
	}

  doc = xmlParseFile (rss->url);
  if (!doc)
    {
      fprintf (stderr, "ERROR: cannot read %s\n", rss->url);
      return NULL;
    }

  node = xmlDocGetRootElement (doc);
  if (!node)
    {
      fprintf (stderr, "ERROR: empty document %s\n", rss->url);
      return NULL;
    }
 if(DEBUG) {
	printf("rss->url: %s\n", rss->url);
	fflush(stdout);
	}

  // rdf?
  // TODO: move this to rss_demux()
  if (strcmp ((char *) node->name, "rss") != 0 &&
      (!strcmp ((char *) node->name, "rdf") ||
       !strcmp ((char *) node->name, "RDF")))
    rdf = 1;

	if(DEBUG) {
		printf("node->name: %s\n", (char *) node->name);
		fflush(stdout);
	}

  node = node->xmlChildrenNode;
  while (node && xmlIsBlankNode (node))
    node = node->next;

  if (!node)
    {
//      fprintf (stderr, "");
      return NULL;
    }

  if (strcmp ((char *) node->name, "channel"))
    {
      fprintf (stderr, "ERROR: bad document: did not immediately find the RSS element\n");
      return NULL;
    }

  if (!rdf) // document is RSS
    node = node->xmlChildrenNode;

  while (node)
    {
      while (node && xmlIsBlankNode (node))
        node = node->next;

      if (!node)
        break;

	if(DEBUG) {
		printf("node->name: %s\n", (char *) node->name);
		fflush(stdout);
	}
      if (!strcmp ((char *) node->name, "title"))
        rss_read_copy (rss->title, doc, node->xmlChildrenNode);
      else if (!strcmp ((char *) node->name, "description"))
	//usleep(1);
/*	Nikola Kolev

        rss_read_copy (rss->desc, doc, node->xmlChildrenNode);
	
	xmlNodeListGetString (n->doc, n, 1) from the rss_read_copy function return Segmentation fault with some descriptions

*/
/*
	fixed - xmlNodeListGetString replaced with xmlNodeGetContent
*/
        rss_read_copy (rss->desc, doc, node->xmlChildrenNode);
      else if (!strcmp ((char *) node->name, "date") ||
               !strcmp ((char *) node->name, "pubDate") ||
               !strcmp ((char *) node->name, "dc:date"))
        rss->date = strptime2 ((const char *) xmlNodeListGetString (node->xmlChildrenNode->doc, node->xmlChildrenNode, 1));
      else if (!strcmp ((char *) node->name, "channel") && rdf)
        {
          xmlNode *pnode = node->xmlChildrenNode;

          while (pnode)
            {
	if(DEBUG) {
		printf("pnode->name: %s\n", (char *) pnode->name);
		fflush(stdout);
	}
              if (!strcmp ((char *) pnode->name, "title"))
                rss_read_copy (rss->title, doc, pnode->xmlChildrenNode);
              else if (!strcmp ((char *) pnode->name, "description"))
                rss_read_copy (rss->desc, doc, pnode->xmlChildrenNode);
              else if (!strcmp ((char *) pnode->name, "date") ||
                       !strcmp ((char *) pnode->name, "pubDate") ||
                       !strcmp ((char *) pnode->name, "dc:date"))
                rss->date = strptime2 ((const char *) xmlNodeListGetString (pnode->xmlChildrenNode->doc, pnode->xmlChildrenNode, 1));

              pnode = pnode->next;
            }

        }
      else if (!strcmp ((char *) node->name, "item") || !strcmp ((char *) node->name, "entry"))
        {
          xmlNode *pnode = node->xmlChildrenNode;
          st_rss_item_t *item = &rss->item[rss->item_count];
          int found = 0;
//          const char *p = NULL;
          char link[RSSMAXBUFSIZE], guid[RSSMAXBUFSIZE];

          *link = *guid = 0;

          while (pnode)
            {
              while (pnode && xmlIsBlankNode (pnode))
                pnode = pnode->next;

              if (!pnode)
                break;

  if(DEBUG) {
              printf ("%s\n", (char *) pnode->name);
              fflush (stdout);
	}

              if (!strcmp ((char *) pnode->name, "title"))
                {
                  rss_read_copy (item->title, doc, pnode->xmlChildrenNode);
                  found = 1;
                }
              else if (!strcmp ((char *) pnode->name, "link"))
                {
                  rss_read_copy (link, doc, pnode->xmlChildrenNode);
                  found = 1;
                }
              else if (!strcmp ((char *) pnode->name, "guid") && (!(*link)))
                {
                  rss_read_copy (guid, doc, pnode->xmlChildrenNode);
                  found = 1;
                }
              else if (!strcmp ((char *) pnode->name, "description"))
                {
                  rss_read_copy (item->desc, doc, pnode->xmlChildrenNode);
                  found = 1;
                }
              else if (!strcasecmp ((char *) pnode->name, "date") ||
                       !strcasecmp ((char *) pnode->name, "pubDate") ||
                       !strcasecmp ((char *) pnode->name, "dc:date") ||
                       !strcasecmp ((char *) pnode->name, "cropDate"))
                { 
                  item->date = strptime2 ((const char *) xmlNodeListGetString (pnode->xmlChildrenNode->doc, pnode->xmlChildrenNode, 1));
                  found = 1;
                }

              pnode = pnode->next;
            }

          // some feeds use the guid tag for the link
          if (*link)
            strcpy (item->url, link);
          else if (*guid)
            strcpy (item->url, guid);
          else
            *(item->url) = 0;

          rss->item_count++;

          if (rss->item_count == RSSMAXITEM)
            break;
        }

//      rss->item_count++;

      node = node->next;
    }

	if(DEBUG) {
		rss_st_rss_t_sanity_check (rss);
		fflush(stdout);
	}

  return rss;
}

static st_rss_t *
rss_open_atom (st_rss_t *rss)
{
  xmlDoc *doc;
  xmlNode *node;
  const char *p = NULL;

	if(DEBUG) {
		printf("rss_open_atom\n");
		fflush(stdout);
	}

  doc = xmlParseFile (rss->url);
  if (!doc)
    {
      fprintf (stderr, "ERROR: cannot read %s\n", rss->url);
      return NULL;
    }

  node = xmlDocGetRootElement (doc);
  if (!node)
    {
      fprintf (stderr, "ERROR: empty document %s\n", rss->url);
      return NULL;
    }

  node = node->xmlChildrenNode;
  while (node && xmlIsBlankNode (node))
    node = node->next;
  if (!node)
    {
      return NULL;
    }

  while (node)
    {
      while (node && xmlIsBlankNode (node))
        node = node->next;

      if (!node)
        break;

      if (!strcmp ((char *) node->name, "title"))
        rss_read_copy (rss->title, doc, node->xmlChildrenNode);
      else if (!strcmp ((char *) node->name, "description"))
        rss_read_copy (rss->desc, doc, node->xmlChildrenNode);
      else if (!strcmp ((char *) node->name, "date") ||
               !strcmp ((char *) node->name, "pubDate") ||
               !strcmp ((char *) node->name, "dc:date") ||
               !strcmp ((char *) node->name, "modified") ||
               !strcmp ((char *) node->name, "updated"))
        rss->date = strptime2 ((const char *) xmlNodeListGetString (node->xmlChildrenNode->doc, node->xmlChildrenNode, 1));
      else if ((!strcmp ((char *) node->name, "entry")))
        {
          xmlNode *pnode = node->xmlChildrenNode;
          st_rss_item_t *item = &rss->item[rss->item_count];
          int found = 0;
          char link[RSSMAXBUFSIZE];

          *link = 0;

          while (pnode)
            {
              while (pnode && xmlIsBlankNode (pnode))
                pnode = pnode->next;

              if (!pnode)
                break;

              if (!strcmp ((char *) pnode->name, "title"))
                {
                  rss_read_copy (item->title, doc, pnode->xmlChildrenNode);
                  found = 1;
                }
              else if (!strcmp ((char *) pnode->name, "link") && (!(*link)))
                {
                  p = (const char *) xml_get_value (pnode, "href");
                  if (p)
                    {
                      strncpy (link, p, RSSMAXBUFSIZE)[RSSMAXBUFSIZE-1] = 0;
                      found = 1;
                    }
                }
              else if (!strcmp ((char *) pnode->name, "content"))
                {
                  rss_read_copy (item->desc, doc, pnode->xmlChildrenNode);
                  found = 1;
                }
              else if (!strcmp ((char *) pnode->name, "modified") ||
                       !strcmp ((char *) pnode->name, "updated"))
                { 
                  item->date = strptime2 ((const char *) xmlNodeListGetString (pnode->xmlChildrenNode->doc, pnode->xmlChildrenNode, 1));
                  found = 1;
                }

              pnode = pnode->next;
            }

          if (*link)
            strcpy (item->url, link);

          rss->item_count++;

          if (rss->item_count == RSSMAXITEM)
            break;
        }

      node = node->next;
    }

  return rss;
}

int
rss_demux (const char *fname){
	
	xmlDoc *doc = NULL;
	xmlNode *node = NULL;
	int version = -1;
	char *p = NULL;

	if(DEBUG) {
		printf("rss_demux %s\n", fname);
		fflush(stdout);
	}

	if (!(doc = xmlParseFile (fname))) {
		fprintf(stderr, "ERROR: cannot read %s\n", fname);
		return -1;
	}

	node = xmlDocGetRootElement (doc);
	
	if (!node)
		return -1;
	if (!(char *) node->name )
		return -1;
	
	if (!strcmp ((char *) node->name, "html")) // not xml
		return -1;
	if (!strcmp ((char *) node->name, "feed")) {
		version = ATOM_V0_1;	//default

		if (!(p = (char *) xml_get_value (node, "version")))
			return version;
		
		if (!strcmp (p, "0.3"))
			version = ATOM_V0_3;
		else if (!strcmp (p, "0.2"))
			version = ATOM_V0_2;
		return version;
	} else if (!strcmp ((char *) node->name, "rss")) {
		if (!(p = (char *) xml_get_value (node, "version")))
			return -1;
		if (!strcmp (p, "0.91"))
			version = RSS_V0_91;
		else if (!strcmp (p, "0.92"))
			version = RSS_V0_92;
		else if (!strcmp (p, "0.93"))
			version = RSS_V0_93;
		else if (!strcmp  (p, "0.94"))
			version = RSS_V0_94;
		else if (!strcmp (p, "2") || !strcmp (p, "2.0") || !strcmp (p, "2.00"))
			version = RSS_V2_0;
		return version;
	} else if (!strcmp ((char *) node->name, "rdf") || !strcmp ((char *) node->name, "RDF")) {
		version = RSS_V1_0;
		return version;
	}

	return -1;

}

st_rss_t *
rss_open (const char *fname) {

	st_rss_t *rss = NULL;

	if(DEBUG) {
		printf("rss_open %s\n", fname);
		fflush(stdout);
	}
	
	if (!(rss = malloc (sizeof (st_rss_t))))
		return NULL;

	memset (rss, 0, sizeof (st_rss_t));
	strncpy (rss->url, fname, RSSMAXBUFSIZE)[RSSMAXBUFSIZE - 1] = 0;
	rss->item_count = 0;

	rss->version = rss_demux (fname);

	if (rss->version == -1) {
		fprintf (stderr, "ERROR: uknown feed format %s.\n", rss->url);
		return NULL;
	}
	switch (rss->version) {
		case ATOM_V0_1:
		case ATOM_V0_2:
		case ATOM_V0_3:
			return rss_open_atom (rss);
		default:
			return rss_open_rss (rss);
	}
	free (rss);
	rss = NULL;

	return NULL;
}
