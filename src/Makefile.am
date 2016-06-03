bin_PROGRAMS = melo

melo_SOURCES = \
	melo_module.c \
	melo_file.c \
	melo_jsonrpc.c \
	melo_httpd.c \
	melo_httpd_file.c \
	melo.c

melo_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(LIBSOUP_CFLAGS) \
	$(LIBJSON_GLIB_CFLAGS) \
	-DMELO_DATA_DIR=\"$(pkgdatadir)\" \
	-Wall

melo_LDFLAGS = \
	$(GLIB_LIBS) \
	$(LIBSOUP_LIBS) \
	$(LIBJSON_GLIB_LIBS)

meloincludedir = $(includedir)/melo

meloinclude_HEADERS = \
	melo_module.h

noinst_HEADERS = \
	melo_file.h \
	melo_jsonrpc.h \
	melo_httpd.h \
	melo_httpd_file.h
