if BUILD_MODULE_FILE
melolib_LTLIBRARIES = libmelo_file.la
endif

# File module library
libmelo_file_la_DEPENDENCIES = \
	$(top_builddir)/src/lib/libmelo.la

libmelo_file_la_SOURCES = \
	melo_browser_file.c \
	melo_library_file.c \
	melo_player_file.c \
	melo_config_file.c \
	melo_file_db.c \
	melo_file.c

libmelo_file_la_CFLAGS = \
	$(MELO_MODULE_FILE_DEPS_CFLAGS) \
	$(LIBMELO_CFLAGS)
libmelo_file_la_LIBADD = \
	$(MELO_MODULE_FILE_DEPS_LIBS) \
	$(LIBMELO_LIBS)

noinst_HEADERS = \
	melo_file.h \
	melo_file_db.h \
	melo_browser_file.h \
	melo_library_file.h \
	melo_player_file.h \
	melo_config_file.h
