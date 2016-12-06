if BUILD_MODULE_UPNP
melolib_LTLIBRARIES = libmelo_upnp.la
endif

# UPnP / DNLA module library
libmelo_upnp_la_DEPENDENCIES = \
	$(top_builddir)/src/lib/libmelo.la

libmelo_upnp_la_SOURCES = \
	melo_config_upnp.c \
	melo_player_upnp.c \
	melo_upnp.c

libmelo_upnp_la_CFLAGS = \
	$(MELO_MODULE_UPNP_DEPS_CFLAGS) \
	$(LIBMELO_CFLAGS)

libmelo_upnp_la_LIBADD = \
	$(MELO_MODULE_UPNP_DEPS_LIBS) \
	$(LIBMELO_LIBS)

noinst_HEADERS = \
	melo_upnp.h \
	melo_player_upnp.h \
	melo_config_upnp.h
