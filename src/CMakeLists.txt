add_executable (rssroll 
	rss.c
	rssroll.c
)

target_link_libraries (rssroll 
	${LIBXML2_LIBRARIES}
	${SQLITE_LIBRARIES}
	${CURL_LIBRARIES}
)

add_executable (index.cgi
	cgi.c
	configfile.c
	index.c
)

target_link_libraries (index.cgi 
	${SQLITE_LIBRARIES}
	-lz
)
