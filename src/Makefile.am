AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"

bin_PROGRAMS = xema
     
xema_SOURCES = \
	base64.c \
	base64.h \
	utils.c \
	utils.h \
	xema.c \
	xema.h \
	xema_defs.h

AM_CFLAGS = -Wall -std=gnu99 -O3

xema_CFLAGS = \
	$(AM_CFLAGS) \
	$(libolmec_CFLAGS) \
	$(libpst_CFLAGS) \
	$(zlib_CFLAGS)

xema_LDADD = \
	-lunistring \
	-lpopt \
	-luuid \
	$(libolmec_LIBS) \
	$(libpst_LIBS) \
	$(zlib_LIBS)

#if LINUX
xema_LDADD += -lbsd
AM_CFLAGS += -D_GNU_SOURCE
#endif






