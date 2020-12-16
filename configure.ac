#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

m4_define(my_version, [m4_esyscmd([tr -d '\n' < version])])

AC_PREREQ([2.69])
AC_INIT([smenu], [my_version], [p.gen.progs@gmail.com])
AC_CONFIG_SRCDIR([smenu.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([no-dist-gzip dist-bzip2 foreign -Wall])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_AWK
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_SEARCH_LIBS([tgetent], [tinfo curses ncursesw ncurses], [HAVE_CURSES=True])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h limits.h langinfo.h locale.h stdint.h stdlib.h \
                  string.h sys/ioctl.h sys/time.h termios.h unistd.h wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([mblen memset nl_langinfo pathconf regcomp setlocale])
AC_CHECK_FUNCS([strchr strrchr strspn strcasecmp strdup strndup])

AC_CANONICAL_HOST
# OS-specific tests
case "${host_os}" in
*freebsd*|*dragonfly*)
  CPPFLAGS="-D__BSD_VISIBLE $CPPFLAGS"
  ;;

*darwin*)
  CPPFLAGS="-D_DARWIN_C_SOURCE $CPPFLAGS"
  ;;

*netbsd*)
  CPPFLAGS="-D_NETBSD_SOURCE $CPPFLAGS"
  ;;

*openbsd*|*bitrig*)
  CPPFLAGS="-D_BSD_SOURCE $CPPFLAGS"
  ;;
esac

AC_CONFIG_FILES([Makefile])
AC_OUTPUT([smenu.spec])
