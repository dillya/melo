dnl Autoconf settings for Melo
AC_PREREQ([2.69])

dnl Initialize Autoconf for Melo
AC_INIT([Melo], [0.0.1], [http://www.sparod.com/melo/], [melo])

dnl Initialize Automake
AM_INIT_AUTOMAKE([subdir-objects])

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

dnl Check for GLib
GLIB_REQ=2.40.0
PKG_CHECK_MODULES([GLIB],
  glib-2.0 >= $GLIB_REQ gobject-2.0 gmodule-no-export-2.0, [],
  [
    AC_MSG_ERROR(GLib $GLIB_REQ or later is required)
  ])

dnl Check for LibSoup
SOUP_REQ=2.40.0
PKG_CHECK_MODULES([LIBSOUP],
  libsoup-2.4 >= $SOUP_REQ, [],
  [
    AC_MSG_ERROR(LibSoup $SOUP_REQ or later is required)
  ])

dnl Check for Json-glib
JSON_GLIB_REQ=1.0.2
PKG_CHECK_MODULES([LIBJSON_GLIB],
  json-glib-1.0 >= $JSON_GLIB_REQ, [],
  [
    AC_MSG_ERROR(Libjson-glib $JSON_GLIB_REQ or later is required)
  ])

dnl Check for Gstreamer
GST_REQ=1.4.4
PKG_CHECK_MODULES([GST],
  gstreamer-1.0 >= $GST_REQ, [],
  [
    AC_MSG_ERROR(Gstreamer $GST_REQ or later is required)
  ])

dnl Check for Gstreamer tag
PKG_CHECK_MODULES([GST_TAG],
  gstreamer-tag-1.0 >= $GST_REQ, [],
  [
    AC_MSG_ERROR(Gstreamer-tag $GST_REQ or later is required)
  ])

dnl Check for Gstreamer sdp
PKG_CHECK_MODULES([GST_SDP],
  gstreamer-sdp-1.0 >= $GST_REQ, [],
  [
    AC_MSG_ERROR(Gstreamer-sdp $GST_REQ or later is required)
  ])

dnl Check for Gstreamer rtp
PKG_CHECK_MODULES([GST_RTP],
  gstreamer-rtp-1.0 >= $GST_REQ, [],
  [
    AC_MSG_ERROR(Gstreamer-rtp $GST_REQ or later is required)
  ])

dnl Check for Avahi gobject
AVAHI_GOBJECT_REQ=0.6.31
PKG_CHECK_MODULES([AVAHI_GOBJECT],
  avahi-gobject >= $AVAHI_GOBJECT_REQ, [],
  [
    AC_MSG_ERROR(Avahi-gobject $AVAHI_OBJECT_REQ or later is required)
  ])

dnl Check for Libssl
LIBSSL_REQ=1.O.1
PKG_CHECK_MODULES([LIBSSL],
  libssl >= $LIBSSL_REQ, [],
  [
    AC_MSG_ERROR(OpenSSL $LIBSSL_REQ or later is required)
  ])

dnl Check for Rygel renderer
PKG_CHECK_MODULES([RYGEL],
  rygel-renderer-gst-2.4, [],
  [
    AC_MSG_ERROR(Rygel renderer is required)
  ])

dnl Generate CFLAGS and LIBS for libraries
MELO_ALL_CFLAGS="-I\$(top_srcdir)/src/lib \$(GLIB_CFLAGS) \$(GST_CFLAGS) \$(GST_TAG_CFLAGS) \$(AVAHI_GOBJECT_CFLAGS) \$(LIBJSON_GLIB_CFLAGS)"
MELO_ALL_LIBS="\$(GLIB_LIBS) \$(GST_LIBS) \$(GST_TAG_LIBS) \$(AVAHI_GOBJECT_LIBS) \$(LIBJSON_GLIB_LIBS)"
AC_SUBST(MELO_ALL_CFLAGS)
AC_SUBST(MELO_ALL_LIBS)

dnl Set Melo libraries directory
melolibdir="\$(libdir)/melo"
AC_SUBST(melolibdir)

dnl Generate CFLAGS and LIBS for libmelo based programs
MELO_LIB_CFLAGS="\$(MELO_ALL_CFLAGS)"
MELO_LIB_LIBS="\$(top_builddir)/src/lib/libmelo.la \$(MELO_ALL_LIBS)"
AC_SUBST(MELO_LIB_CFLAGS)
AC_SUBST(MELO_LIB_LIBS)

AC_CONFIG_FILES(
Makefile
src/Makefile
src/lib/Makefile
src/lib/libmelo.pc
src/modules/Makefile
src/modules/airplay/Makefile
src/modules/file/Makefile
src/modules/radio/Makefile
src/modules/upnp/Makefile
tests/Makefile
www/Makefile
)
AC_OUTPUT
