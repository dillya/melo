melolib_LTLIBRARIES = libmelo_radio.la

# Radio module library
libmelo_radio_la_DEPENDENCIES = \
	$(top_builddir)/src/lib/libmelo.la

libmelo_radio_la_SOURCES = \
	melo_browser_radio.c \
	melo_player_radio.c \
	melo_playlist_radio.c \
	melo_radio.c

libmelo_radio_la_CFLAGS = $(MELO_ALL_CFLAGS)
libmelo_radio_la_LIBADD = $(MELO_ALL_LIBS)

noinst_HEADERS = \
	melo_radio.h \
	melo_browser_radio.h \
	melo_player_radio.h \
	melo_playlist_radio.h
