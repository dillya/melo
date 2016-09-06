melolib_LTLIBRARIES = libmelo_airplay.la

# Airplay module library
libmelo_airplay_la_DEPENDENCIES = \
	$(top_builddir)/src/lib/libmelo.la

libmelo_airplay_la_SOURCES = \
	melo_config_airplay.c \
	melo_player_airplay.c \
	gstrtpraopdepay.c \
	gstrtpraop.c \
	gsttcpraop.c \
	melo_airplay.c

libmelo_airplay_la_SOURCES += \
	ext/gstrtpjitterbuffer.c \
	ext/rtpjitterbuffer.c \
	ext/rtpstats.c

libmelo_airplay_la_CFLAGS = \
	$(LIBSSL_CFLAGS) \
	$(GST_SDP_CFLAGS) \
	$(GST_RTP_CFLAGS) \
	$(MELO_ALL_CFLAGS)

libmelo_airplay_la_LIBADD = \
	$(LIBSSL_LIBS) \
	$(GST_SDP_LIBS) \
	$(GST_RTP_LIBS) \
	$(MELO_ALL_LIBS)

noinst_HEADERS = \
	melo_airplay.h \
	gsttcpraop.h \
	gstrtpraop.h \
	gstrtpraopdepay.h \
	melo_player_airplay.h \
	melo_config_airplay.h

noinst_HEADERS += \
	ext/gstrtpjitterbuffer.h \
	ext/gstrtpdefs.h \
	ext/gst-compat.h \
	ext/rtpjitterbuffer.h \
	ext/rtpstats.h
