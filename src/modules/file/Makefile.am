melolib_LTLIBRARIES = libmelo_file.la

# File module library
libmelo_file_la_DEPENDENCIES = \
	$(top_builddir)/src/lib/libmelo.la

libmelo_file_la_SOURCES = \
	melo_browser_file.c \
	melo_player_file.c \
	melo_playlist_file.c \
	melo_config_file.c \
	melo_file.c

libmelo_file_la_CFLAGS = $(MELO_ALL_CFLAGS)
libmelo_file_la_LIBADD = $(MELO_ALL_LIBS)

noinst_HEADERS = \
	melo_file.h \
	melo_browser_file.h \
	melo_player_file.h \
	melo_playlist_file.h \
	melo_config_file.h
