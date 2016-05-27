bin_PROGRAMS = melo

melo_SOURCES = \
	melo_module.c \
	melo_file.c \
	melo_httpd.c \
	melo.c

melo_CFLAGS = $(GLIB_CFLAGS) $(LIBSOUP_CFLAGS)

melo_LDFLAGS = $(GLIB_LIBS) $(LIBSOUP_LIBS)

meloincludedir = $(includedir)/melo

meloinclude_HEADERS = \
	melo_module.h

noinst_HEADERS = \
	melo_file.h \
	melo_httpd.h
