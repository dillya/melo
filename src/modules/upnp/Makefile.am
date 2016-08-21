melolib_LTLIBRARIES = libmelo_upnp.la

# UPnP / DNLA module library
libmelo_upnp_la_DEPENDENCIES = \
	$(top_builddir)/src/lib/libmelo.la

libmelo_upnp_la_SOURCES = \
	melo_config_upnp.c \
	melo_player_upnp.c \
	melo_upnp.c

libmelo_upnp_la_CFLAGS = \
	$(RYGEL_CFLAGS) \
	$(MELO_ALL_CFLAGS)

libmelo_upnp_la_LIBADD = \
	$(RYGEL_LIBS) \
	$(MELO_ALL_LIBS)

noinst_HEADERS = \
	melo_upnp.h \
	melo_player_upnp.h \
	melo_config_upnp.h
