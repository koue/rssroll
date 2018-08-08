#
project (rssroll)
cmake_minimum_required (VERSION 2.6)

#
set (PACKAGE_NAME "rssroll")
set (PACKAGE_VERSION "0.6")
set (PACKAGE_URL "https://github.com/koue/rssroll")
set (PACKAGE_BUGREPORT "https://github.com/koue/rssroll/issues")

#
enable_testing()

#
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
include (${CMAKE_ROOT}/Modules/CheckFunctionExists.cmake)

#
set (CMAKE_C_FLAGS "-Wall -Wstrict-prototypes")

# libxml2
find_package (LibXml2 REQUIRED)

# curl
find_package (CURL REQUIRED)

# sqlite
find_package (Sqlite REQUIRED)

# libcez
find_package (Libcez REQUIRED)

#
configure_file ("${PROJECT_SOURCE_DIR}/config.h.in"
                "${PROJECT_BINARY_DIR}/src/config.h" )
#
include_directories (${LIBXML2_INCLUDE_DIR})
include_directories (${SQLITE_INCLUDE_DIR})
include_directories (${CURL_INCLUDE_DIRS})
include_directories (${LIBCEZ_CONFIG_INCLUDE_DIRS})
include_directories (${LIBCEZ_FOSSIL_INCLUDE_DIRS})
include_directories (${PROJECT_BINARY_DIR}/src)

#
install (FILES rssrollrc
        DESTINATION ${CMAKE_INSTALL_PREFIX}/etc
	RENAME rssrollrc.sample)

install (DIRECTORY html css
	DESTINATION ${CMAKE_INSTALL_PREFIX}/www/rssroll)

#
add_subdirectory (src)
add_subdirectory (tests)
