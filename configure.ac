dnl Autoconf settings for Melo
AC_PREREQ([2.69])

dnl Initialize Autoconf for Melo
AC_INIT([Melo], [0.0.1], [http://www.sparod.com/melo/], [melo])

dnl Initialize Automake
AM_INIT_AUTOMAKE([subdir-objects])
AM_SILENT_RULES([yes])

dnl Set up macro directory
AC_CONFIG_MACRO_DIR([m4])

dnl Set source directory
AC_CONFIG_SRCDIR([src/melo.c])

dnl Set output for config header
AC_CONFIG_HEADERS([config.h])

dnl Check for programs
AC_PROG_CC
AM_PROG_CC_C_O

dnl Check and init libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl Check for header files
AC_HEADER_STDC

dnl Check for gtk-doc
GTK_DOC_CHECK([1.14],[--flavour no-tmpl])
#m4_ifdef([GTK_DOC_CHECK], [
#  GTK_DOC_CHECK([1.14],[--flavour no-tmpl])
#],[
#  AM_CONDITIONAL([ENABLE_GTK_DOC], false)
#])

dnl Library requirements
GLIB_REQ=2.40.0
JSON_GLIB_REQ=1.0.2
GSTREAMER_REQ=1.10.4
AVAHI_GOBJECT_REQ=0.6.31

dnl Check library dependencies
PKG_CHECK_MODULES([LIBMELO_DEPS],
  glib-2.0 >= $GLIB_REQ
  gobject-2.0
  gmodule-no-export-2.0
  json-glib-1.0 >= $JSON_GLIB_REQ
  gstreamer-1.0 >= $GSTREAMER_REQ
  gstreamer-tag-1.0 >= $GSTREAMER_REQ
  gstreamer-pbutils-1.0 >= $GSTREAMER_REQ
  avahi-gobject >= $AVAHI_GOBJECT_REQ)

dnl disable modules if not needed
AC_ARG_ENABLE([melo],
  AS_HELP_STRING([--disable-melo],[Disable Melo program]),
  enable_melo=no, enable_melo=yes)
AC_ARG_ENABLE([module-file],
  AS_HELP_STRING([--disable-module-file],[Disable File module]),
  enable_module_file=no, enable_module_file=yes)
AC_ARG_ENABLE([module-radio],
  AS_HELP_STRING([--disable-module-radio],[Disable Radio module]),
  enable_module_radio=no, enable_module_radio=yes)
AC_ARG_ENABLE([module-upnp],
  AS_HELP_STRING([--disable-module-upnp],[Disable UPnP module]),
  enable_module_upnp=no, enable_module_upnp=yes)

dnl Optional libraries
AC_ARG_WITH([libnm-glib],
  AS_HELP_STRING([--with-libnm-glib],[use libnm-glib @<:@default=check@:>@]),,
  with_libnm_glib=check)

dnl Check for melo dependencies
if test "x$enable_melo" = "xyes"; then
  MELO_LIBSOUP_REQ=2.40.0
  PKG_CHECK_MODULES([MELO_DEPS],
    libsoup-2.4 >= $MELO_LIBSOUP_REQ,
    [enable_melo=yes])
fi

dnl Check for File module dependencies
if test "x$enable_module_file" = "xyes"; then
  MELO_MODULE_FILE_SQLITE3_REQ=3.6.19
  PKG_CHECK_MODULES([MELO_MODULE_FILE_DEPS],
    sqlite3 >= $MELO_MODULE_FILE_SQLITE3_REQ,
    [enable_module_file=yes])
  AC_DEFINE([HAVE_MELO_MODULE_FILE], 1, [Use File module])
fi

dnl Check for Radio module dependencies
if test "x$enable_module_radio" = "xyes"; then
  MELO_MODULE_RADIO_LIBSOUP_REQ=2.40.0
  PKG_CHECK_MODULES([MELO_MODULE_RADIO_DEPS],
    libsoup-2.4 >= $MELO_MODULE_RADIO_LIBSOUP_REQ,
    [enable_module_radio=yes])
  AC_DEFINE([HAVE_MELO_MODULE_RADIO], 1, [Use Radio module])
fi

dnl Check for UPnP module dependencies
if test "x$enable_module_upnp" = "xyes"; then
  MELO_MODULE_UPNP_GUPNP_REQ=0.20.12
  MELO_MODULE_UPNP_LIBSOUP_REQ=2.40.0
  AC_ARG_WITH([rygel_2.4],
    AS_HELP_STRING([--with-rygel-2.4],[Use Rygel 2.4 instead of 2.6]),
    [MELO_MODULE_UPNP_RYGEL_VERSION=2.4],[MELO_MODULE_UPNP_RYGEL_VERSION=2.6]
  )
  PKG_CHECK_MODULES([MELO_MODULE_UPNP_DEPS],
    gupnp-1.0 >= $MELO_MODULE_UPNP_GUPNP_REQ
    libsoup-2.4 >= $MELO_MODULE_UPNP_LIBSOUP_REQ
    rygel-renderer-gst-$MELO_MODULE_UPNP_RYGEL_VERSION,
    [enable_module_upnp=yes])
  AC_DEFINE([HAVE_MELO_MODULE_UPNP], 1, [Use UPnP module])
fi

dnl Use NetworkManager if available
if test "x$with_libnm_glib" != "xno"; then
  LIBNM_GLIB_REQ=0.9.10.0
  PKG_CHECK_MODULES([LIBNM_GLIB],
    libnm-glib >= $LIBNM_GLIB_REQ,
    [with_libnm_glib=yes
     AC_DEFINE([HAVE_LIBNM_GLIB], 1, [LibNM Glib is available])],
    [if test "x$with_libnm_glib" != "xcheck"; then
       AC_MSG_FAILURE([--with-libnm-glib was given, but package is not found])
     fi
     with_libnm_glib=no])
fi

dnl Build modules
AM_CONDITIONAL([BUILD_MELO], [test "x$enable_melo" = "xyes"])
AM_CONDITIONAL([BUILD_MODULE_FILE], [test "x$enable_module_file" = "xyes"])
AM_CONDITIONAL([BUILD_MODULE_RADIO], [test "x$enable_module_radio" = "xyes"])
AM_CONDITIONAL([BUILD_MODULE_UPNP], [test "x$enable_module_upnp" = "xyes"])
AM_CONDITIONAL([WITH_LIBNM_GLIB], [test "x$with_libnm_glib" = "xyes"])

dnl Generate CFLAGS and LIBS for Melo library
LIBMELO_CFLAGS="-I\$(top_srcdir)/src/lib \$(LIBMELO_DEPS_CFLAGS)"
LIBMELO_LIBS="\$(LIBMELO_DEPS_LIBS)"
AC_SUBST(LIBMELO_CFLAGS)
AC_SUBST(LIBMELO_LIBS)

dnl Set Melo libraries directory
melolibdir="\$(libdir)/melo"
AC_SUBST(melolibdir)

dnl Generate CFLAGS and LIBS for libmelo based programs
MELO_CFLAGS="\$(MELO_DEPS_CFLAGS)"
MELO_LIBS="\$(top_builddir)/src/lib/libmelo.la \$(MELO_DEPS_LIBS)"
AC_SUBST(MELO_CFLAGS)
AC_SUBST(MELO_LIBS)

AC_CONFIG_FILES(
  Makefile
  doc/Makefile
  src/Makefile
  src/lib/Makefile
  src/lib/libmelo.pc
  src/modules/Makefile
  src/modules/file/Makefile
  src/modules/radio/Makefile
  src/modules/upnp/Makefile
  tests/Makefile
  www/Makefile
)
AC_OUTPUT

echo "
   ${PACKAGE} - ${VERSION} configuration:

   Built-in modules:
   -----------------
     file:              ${enable_module_file}
     radio:             ${enable_module_radio}
     upnp:              ${enable_module_upnp}

   Melo program:
   -------------
     melo:              ${enable_melo}

   Optional libraries:
   -------------------
     libnm-glib:        ${with_libnm_glib}
"
