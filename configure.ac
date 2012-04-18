AC_INIT([pcma], [0.1.0])
AC_CONFIG_HEADERS([config.h])

AC_PROG_SED
AC_HEADER_STDC
AC_CHECK_PROGS(A2X, a2x, a2x, false)
AC_CHECK_PROGS(ASCIIDOC, asciidoc)
AC_CHECK_PROGS(XMLLINT, xmllint)
AC_CHECK_PROGS(XSLTPROC, xsltproc)
AC_CHECK_LIB(msgpack,msgpack_version)
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([ZMQ], [libzmq >= 2.1])

if test "z$A2X" = 'zfalse'; then
	AC_MSG_ERROR([Build requires a2x])
fi

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
