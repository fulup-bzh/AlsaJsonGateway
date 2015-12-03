VERSION=0.2.2

all-local :

AM_CPPFLAGS = $(GCC_CPPFLAGS) -I$(top_builddir)/include -DAJQ_VERSION=\"$(VERSION)\"
AM_CFLAGS = $(GCC_CFLAGS) -Wstrict-prototypes

CLEANFILES =

bin_PROGRAMS = ajg-daemon

ajg_daemon_LDFLAGS = -export-dynamic
ajg_daemon_CPPFLAGS = $(AM_CPPFLAGS) -Wno-unused-result
ajg_daemon_CFLAGS = $(AM_CFLAGS) $(ALSA_CFLAGS) $(LIBMICROHTTPD_CFLAGS) $(JSONC_CFLAGS)
ajg_daemon_LDADD = $(ALSA_LIBS) $(LIBMICROHTTPD_LIBS) $(JSONC_LIBS)

ajg_daemon_SOURCES =			\
	main-ajg.c			\
	config-ajg.c			\
	httpd-ajg.c			\
	alsa-ajg.c			\
	session-ajq.c

ajg_daemondatadir = $(localstatedir)/www/fakemod
dist_ajg_daemondata_DATA =		\
	$(top_srcdir)/samples/*
