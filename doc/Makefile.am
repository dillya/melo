DOC_MODULE = melo
DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.xml
DOC_SOURCE_DIR = $(top_srcdir)/src/lib

SCANGOBJ_OPTIONS=
SCAN_OPTIONS=
MKDB_OPTIONS=--xml-mode --output-format=xml

MKHTML_OPTIONS=
FIXXREF_OPTIONS=

HFILE_GLOB=$(top_srcdir)/src/lib/*.h
CFILE_GLOB=$(top_srcdir)/src/lib/*.c

EXTRA_HFILES=

IGNORE_HFILES=

HTML_IMAGES=

content_files=

expand_content_files=

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. GTKDOC_CFLAGS=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)
GTKDOC_CFLAGS=
GTKDOC_LIBS=$(top_builddir)/src/lib/libmelo.la

include $(top_srcdir)/gtk-doc.make

if ENABLE_GTK_DOC
TESTS_ENVIRONMENT = \
	DOC_MODULE=$(DOC_MODULE) DOC_MAIN_SGML_FILE=$(DOC_MAIN_SGML_FILE) \
	SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir)
TESTS = $(GTKDOC_CHECK)
endif

-include $(top_srcdir)/git.mk
