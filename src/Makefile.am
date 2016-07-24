SUBDIRS = \
	lib \
	modules

# Main program
bin_PROGRAMS = melo

melo_SOURCES = \
	melo_httpd.c \
	melo_httpd_file.c \
	melo_httpd_jsonrpc.c \
	melo_config_main.c \
	melo.c

melo_CFLAGS = \
	$(MELO_LIB_CFLAGS) \
	$(LIBSOUP_CFLAGS) \
	-DMELO_DATA_DIR=\"$(pkgdatadir)\"

melo_LDADD = \
	$(MELO_LIB_LIBS) \
	$(LIBSOUP_LIBS)

# Built in modules
melo_LDADD += \
	modules/airplay/libmelo_airplay.la \
	modules/file/libmelo_file.la \
	modules/radio/libmelo_radio.la

noinst_HEADERS = \
	melo_config_main.h \
	melo_httpd.h \
	melo_httpd_file.h \
	melo_httpd_jsonrpc.h \
	melo.h
