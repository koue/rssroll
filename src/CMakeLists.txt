add_executable (rssroll
	rss.c
	rssroll.c
)

target_include_directories (rssroll PUBLIC
	${LIBCEZ_CONFIG_INCLUDE_DIR}
	${LIBCEZ_FOSSIL_INCLUDE_DIR}
	${LIBCEZ_MISC_INCLUDE_DIR}
)

target_link_libraries (rssroll
	${LIBXML2_LIBRARIES}
	${SQLITE_LIBRARIES}
	${CURL_LIBRARIES}
	${LIBCEZ_CONFIG_LIBRARIES}
	${LIBCEZ_FOSSIL_LIBRARIES}
)

add_executable (index.cgi
	index.c
)

target_include_directories (index.cgi PUBLIC
	${LIBCEZ_CONFIG_INCLUDE_DIR}
	${LIBCEZ_FOSSIL_INCLUDE_DIR}
	${LIBCEZ_MISC_INCLUDE_DIR}
)

target_link_libraries (index.cgi
	${SQLITE_LIBRARIES}
	${LIBCEZ_CONFIG_LIBRARIES}
	${LIBCEZ_FOSSIL_LIBRARIES}
	-lz
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
