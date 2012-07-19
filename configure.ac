AC_INIT([pcma], [0.2.0])
AC_CONFIG_HEADERS([config.h])

AC_PROG_SED
AC_HEADER_STDC
AC_CHECK_PROGS(A2X, a2x, "no")
AC_CHECK_PROGS(XMLLINT, xmllint, "no")
AC_CHECK_PROGS(XSLTPROC, xsltproc, "no")

if test "x$A2X"     = xno; then AC_MSG_ERROR([a2x is required]); fi
if test "x$XMLLINT" = xno; then AC_MSG_ERROR([xmllint is required]); fi
if test "x$XSLPROC" = xno; then AC_MSG_ERROR([xsltproc is required]); fi

AC_CHECK_PROGS(ASCIIDOC, asciidoc)
AC_CHECK_PROGS(XMLLINT, xmllint)
AC_CHECK_PROGS(XSLTPROC, xsltproc)

AC_CHECK_LIB([msgpack],[msgpack_version],[])

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.16],,
	AC_MSG_ERROR([glib2 2.16 or newer not found.]))
PKG_CHECK_MODULES([ZMQ], [libzmq >= 2.1],,
	AC_MSG_ERROR([libzmq 2.1 or newer not found.]))

AM_PROG_CC_C_O

AC_ARG_WITH([systemdsystemunitdir],
	AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
	[], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])

if test "x$with_systemdsystemunitdir" != xno; then
	AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi

AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])

AM_INIT_AUTOMAKE([1.10 -Wall no-define])
AC_OUTPUT(Makefile src/Makefile man/Makefile init/Makefile)
