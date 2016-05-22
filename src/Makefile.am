bin_PROGRAMS = melo

melo_SOURCES = melo.c \
	       melo_httpd.c

melo_CFLAGS = $(GLIB_CFLAGS) $(LIBSOUP_CFLAGS)

melo_LDFLAGS = $(GLIB_LIBS) $(LIBSOUP_LIBS)

EXTRA_DIST = melo_httpd.h
