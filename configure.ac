dnl Autoconf settings for Melo
AC_PREREQ([2.69])

dnl Initialize Autoconf for Melo
AC_INIT([Melo], [0.0.1], [http://www.sparod.com/aircat/], [melo])

dnl Initialize Automake
AM_INIT_AUTOMAKE()

dnl Set source directory
AC_CONFIG_SRCDIR([src/melo.c])

dnl Set output for config header
AC_CONFIG_HEADERS([config.h])

dnl Check for programs
AC_PROG_CC
AM_PROG_CC_C_O

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

AC_CONFIG_FILES(
Makefile
src/Makefile
www/Makefile
)
AC_OUTPUT
