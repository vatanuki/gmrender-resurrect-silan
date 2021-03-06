AC_PREREQ(2.59)
AC_INIT(gmediarender, 0.0.7-silan, https://github.com/hzeller/gmrender-resurrect)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE([1.8 dist-bzip2 no-dist-gzip check-news])

AC_SYS_LARGEFILE

# Checks for programs
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

EXTRA_GCC_DEBUG_CFLAGS=""
EXTRA_GCC_DEBUG_CXXFLAGS=""

if test -n "$GCC"; then
  EXTRA_GCC_DEBUG_CFLAGS="$CFLAGS"
  EXTRA_GCC_DEBUG_CXXFLAGS="$CXXFLAGS"
  CFLAGS+=" -Wall -Wpointer-arith -Wmissing-prototypes -Wmissing-declarations -Wwrite-strings"
  CXXFLAGS+=" -Wall -Wpointer-arith"
fi

AC_CHECK_FUNCS([asprintf])
AC_CHECK_LIB([m],[exp])

# Debugging
AC_ARG_ENABLE(debug, [  --enable-debug          enable debugging],, enable_debug=no)
if test "x$enable_debug" = "xyes"; then
  CFLAGS="$CFLAGS -g -O0 -Wall -Werror"
fi

PKG_PROG_PKG_CONFIG

LIBUPNP_REQUIRED=1.6.0
AC_ARG_WITH( libupnp, AC_HELP_STRING([--without-libupnp],[compile without libupnp support]), try_libupnp=$withval, try_libupnp=yes )
HAVE_LIBUPNP=no
if test x$try_libupnp = xyes; then
  dnl check for libupnp
  PKG_CHECK_MODULES(LIBUPNP, libupnp >= $LIBUPNP_REQUIRED,
    [
      HAVE_LIBUPNP=yes
      AC_SUBST(LIBUPNP_CFLAGS)
      AC_SUBST(LIBUPNP_LIBS)

    ],
    [
      HAVE_LIBUPNP=no
    ])
fi
if test x$HAVE_LIBUPNP = xyes; then
  AC_DEFINE(HAVE_LIBUPNP, , [Use libupnp])
fi
AC_SUBST(HAVE_LIBUPNP)

# Checks for header files.
AC_HEADER_STDC

dnl Give error and exit if we don't have any UPnP SDK
if test "x$HAVE_LIBUPNP" = "xno"; then
  if test "x$HAVE_LIBGUPNP" = "xno"; then
    AC_MSG_ERROR(you need either libupnp or libgupnp)
  fi
fi

AC_CONFIG_FILES([Makefile src/Makefile data/Makefile])
AC_OUTPUT
