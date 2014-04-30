AC_INIT(xema, 0.1)

AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.14])
AC_PROG_CC_C99
AC_HEADER_STDC
AC_PROG_CC

CFLAGS=
AC_SUBST(CFLAGS)

dnl ***************************************************************************
dnl Internationalization
dnl ***************************************************************************
IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=xema
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])

AM_GLIB_GNU_GETTEXT

PKG_CHECK_MODULES(libolmec, [libolmec >= 0.1])
PKG_CHECK_MODULES(libpst, [libpst >= 0.6.59])
PKG_CHECK_MODULES(zlib, [zlib >= 1.2.8])
AC_SUBST(libolmec_CFLAGS)
AC_SUBST(libpst_CFLAGS)
AC_SUBST(zlib_CFLAGS)
AC_SUBST(libolmec_LIBS)
AC_SUBST(libpst_LIBS)
AC_SUBST(zlib_LIBS)
AC_CHECK_LIB([unistring], [u8_to_u32], [], [AC_MSG_ERROR([libunistring does not appear to be (correctly) installed)])])
AC_CHECK_LIB([popt], [poptGetOptArg], [], [AC_MSG_ERROR([libpopt does not appear to be (correctly) installed)])])
AC_CHECK_LIB([uuid], [uuid_generate], [], [AC_MSG_ERROR([libuuid does not appear to be (correctly) installed)])])
AC_CHECK_LIB([bsd], [strlcat], [], [AC_MSG_ERROR([libbsd does not appear to be (correctly) installed)])])

AC_OUTPUT([
Makefile
src/Makefile
po/Makefile.in
man/Makefile
])
