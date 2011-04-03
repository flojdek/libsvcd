AC_INIT([libservdisc], [1.0], [kupczyk.tomasz@gmail.com])
AC_CONFIG_AUX_DIR([./config])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

LT_PREREQ([2.2])
LT_INIT([dlopen])

AC_PROG_CXX
AC_PROG_RANLIB
AC_PROG_LIBTOOL

AX_BOOST_BASE(1.42)
AX_BOOST_PROGRAM_OPTIONS

AC_CONFIG_FILES([
	Makefile
	src/Makefile	 
	tests/Makefile
])

AC_OUTPUT
