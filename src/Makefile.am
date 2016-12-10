SUBDIRS = \
	lib \
	modules

# Main program
if BUILD_MELO
bin_PROGRAMS = melo
endif

melo_SOURCES = \
	melo_httpd.c \
	melo_httpd_file.c \
	melo_httpd_cover.c \
	melo_httpd_jsonrpc.c \
	melo_config_main.c \
	melo.c

melo_CFLAGS = \
	$(MELO_CFLAGS) \
	$(LIBMELO_CFLAGS) \
	-DMELO_DATA_DIR=\"$(pkgdatadir)\"

melo_LDADD = \
	$(MELO_LIBS) \
	$(LIBMELO_LIBS)

# Network support
if WITH_LIBNM_GLIB
melo_SOURCES += \
	melo_network.c \
	melo_network_jsonrpc.c
melo_CFLAGS += $(LIBNM_GLIB_CFLAGS)
melo_LDADD += $(LIBNM_GLIB_LIBS)
endif

# Built in modules
if BUILD_MODULE_FILE
melo_LDADD += modules/file/libmelo_file.la
endif
if BUILD_MODULE_RADIO
melo_LDADD += modules/radio/libmelo_radio.la
endif
if BUILD_MODULE_UPNP
melo_LDADD += modules/upnp/libmelo_upnp.la
endif

noinst_HEADERS = \
	melo_config_main.h \
	melo_network.h \
	melo_network_jsonrpc.h \
	melo_httpd.h \
	melo_httpd_file.h \
	melo_httpd_cover.h \
	melo_httpd_jsonrpc.h \
	melo.h
