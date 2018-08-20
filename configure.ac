AC_PREREQ([2.67])
AC_INIT([diskstress], [0.1], [mike@virtualinterconnect.com])
AM_INIT_AUTOMAKE([1.11 silent-rules foreign subdir-objects])
AC_CONFIG_SRCDIR([src/diskstress.cc])
AC_CONFIG_HEADERS([config.h])

AX_CXX_COMPILE_STDCXX_11(noext, mandatory)
AC_PROG_CXX

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

AC_MSG_NOTICE([
"-------------------------------------------------

 ${PACKAGE_NAME} Version ${PACKAGE_VERSION}

 Prefix: '${prefix}'.
 Compiler: '${CC} ${CFLAGS} ${CPPFLAGS}'

 Package features:
   Async Execution: ${async_exec}

 Now type 'make @<:@<target>@:>@'
   where the optional <target> is:
     all                - build all binaries
     install            - install everything

     check              - run unit tests
     dist               - build distribution tarball

     diskstress         - build diskstress

--------------------------------------------------"
])
