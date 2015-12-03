m4_define([alsajsongateway_version], 0.2.2)

AC_PREREQ([2.64])
AC_INIT([alsajsongateway],
        [alsajsongateway_version],
        [https://github.com/fulup-bzh/AlsaJsonGateway/],
        [alsajsongateway],
        [https://github.com/fulup-bzh/AlsaJsonGateway/])

AC_SUBST([ALSAJSONGATEWAY_VERSION], [alsajsongateway_version])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

AM_INIT_AUTOMAKE([1.11 parallel-tests foreign no-dist-gzip dist-xz color-tests subdir-objects])

AM_SILENT_RULES([yes])

# Check for gcc
AC_PROG_CC

# Check for (optional) valgrind
AX_VALGRIND_CHECK

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

# Check functions, libs, packages
PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(ALSA, [alsa])
PKG_CHECK_MODULES(LIBMICROHTTPD, [libmicrohttpd])
PKG_CHECK_MODULES(JSONC, [json-c])

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT

AC_MSG_RESULT([
	Type 'make' to compile AlsaJsonGateway.
])
