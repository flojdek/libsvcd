AC_INIT([libsvcd], [1.0], [kupczyk.tomasz@gmail.com])
AC_CONFIG_AUX_DIR([./config])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_LIBTOOL
AC_PROG_CXX

AX_BOOST_BASE(1.42)
AX_BOOST_PROGRAM_OPTIONS

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	tests/Makefile
])

AC_OUTPUT
