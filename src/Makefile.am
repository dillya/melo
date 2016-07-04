AM_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GST_CFLAGS) \
	$(LIBJSON_GLIB_CFLAGS) \
	-Wall

AM_LDFLAGS = \
	$(GLIB_LIBS) \
	$(GST_LIBS) \
	$(LIBJSON_GLIB_LIBS)

# Libraries
melolibdir = $(libdir)/melo
melolib_LTLIBRARIES = \
	libmelo_file.la \
	libmelo.la

# Main library
libmelo_la_SOURCES = \
	melo_module.c \
	melo_browser.c \
	melo_player.c \
	melo_tags.c \
	melo_module_jsonrpc.c \
	melo_browser_jsonrpc.c \
	melo_player_jsonrpc.c \
	melo_jsonrpc.c

meloincludedir = $(includedir)/melo
meloinclude_HEADERS = \
	melo_module.h \
	melo_browser.h \
	melo_player.h \
	melo_tags.h \
	melo_module_jsonrpc.h \
	melo_browser_jsonrpc.h \
	melo_player_jsonrpc.h \
	melo_jsonrpc.h

# File module library
libmelo_file_la_DEPENDENCIES = \
	libmelo.la

libmelo_file_la_SOURCES = \
	melo_browser_file.c \
	melo_player_file.c \
	melo_file.c

# Main program
bin_PROGRAMS = melo

melo_SOURCES = \
	melo_httpd.c \
	melo_httpd_file.c \
	melo_httpd_jsonrpc.c \
	melo.c

melo_CFLAGS = \
	$(AM_CFLAGS) \
	$(LIBSOUP_CFLAGS) \
	-DMELO_DATA_DIR=\"$(pkgdatadir)\"

melo_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LIBSOUP_LIBS)

melo_LDADD = \
	libmelo_file.la \
	libmelo.la

noinst_HEADERS = \
	melo_file.h \
	melo_browser_file.h \
	melo_player_file.h \
	melo_httpd.h \
	melo_httpd_file.h \
	melo_httpd_jsonrpc.h
