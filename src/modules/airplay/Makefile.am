melolib_LTLIBRARIES = libmelo_airplay.la

# Airplay module library
libmelo_airplay_la_DEPENDENCIES = \
	$(top_builddir)/src/lib/libmelo.la

libmelo_airplay_la_SOURCES = \
	melo_config_airplay.c \
	melo_airplay.c

libmelo_airplay_la_CFLAGS = $(MELO_ALL_CFLAGS)
libmelo_airplay_la_LIBADD = $(MELO_ALL_LIBS)

noinst_HEADERS = \
	melo_airplay.h \
	melo_config_airplay.h
