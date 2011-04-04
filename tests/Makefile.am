TESTS = configfiletest payloadsizetest initialsetuptest
bin_PROGRAMS = configfiletest payloadsizetest initialsetuptest

configfiletest_SOURCES = configfiletest.cpp
configfiletest_LDADD = $(top_srcdir)/src/libservdisc.la
configfiletest_CPPFLAGS = -I$(top_srcdir)/src

payloadsizetest_SOURCES = payloadsizetest.cpp
payloadsizetest_LDADD = $(top_srcdir)/src/libservdisc.la
payloadsizetest_CPPFLAGS = -I$(top_srcdir)/src

initialsetuptest_SOURCES = initialsetuptest.cpp
initialsetuptest_LDADD = $(top_srcdir)/src/libservdisc.la
initialsetuptest_CPPFLAGS = -I$(top_srcdir)/src

EXTRA_DIST = ./data/configfile
