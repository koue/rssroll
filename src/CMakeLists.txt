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
	index.c
)

target_link_libraries (index.cgi 
	${SQLITE_LIBRARIES}
	-lz
	/usr/local/lib/libcez.a
)

#add_executable (getdata.cgi
#        configfile.c
#        getdata.c
#)

#target_link_libraries (getdata.cgi
#        ${SQLITE_LIBRARIES}
#        -lz
#)

install (TARGETS rssroll
	DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
