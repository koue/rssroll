AC_INIT(rssroll, 0.5,
                 [https://github.com/koue/rssroll/issues], ,
                 [https://github.com/koue/rssroll])

# local m4 libraries
m4_include([m4/ax_lib_sqlite3.m4])
m4_include([m4/ax_lib_xml2.m4])
m4_include([m4/libcurl.m4])

#AM_PROG_CC_C_O

AC_PREREQ([2.59])
AM_MAINTAINER_MODE([disable])
AM_INIT_AUTOMAKE([1.10 no-define])

AC_MSG_NOTICE([This is RssRoll.])

echo " ===> Testing for some programs"
AC_PROG_LN_S

echo " ===> Testing for a C compiler"
AC_PROG_CC

echo " ===> Testing for some headers"
AC_CHECK_HEADER(unistd.h)

echo " ===> Testing fir some functions"
AC_CHECK_FUNCS([strlcpy strlcat])

echo " ===> Testing for some libraries"
AX_LIB_SQLITE3(3.0.0)
AX_LIB_XML2(2.5.0)
LIBCURL_CHECK_CONFIG(, 7.19.0, , 
	[AC_MSG_ERROR([Missing required libcurl >= 7.19.0])])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_HEADERS([config.h])
dnl AC_CONFIG_COMMANDS(make, [ls -l], [])
AC_OUTPUT
