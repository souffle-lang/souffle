# Souffle - A Datalog Compiler
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt


AC_INIT(souffle, m4_esyscmd([git describe --tags --abbrev=0 | tr -d '\n']), [souffle-talk@googlegroups.com])
AC_PREREQ(2.68)
AC_COPYRIGHT(['2013-15 Oracle and/or its affiliates'])

AC_CANONICAL_TARGET # target_cpu, target_vendor, and target_os
AC_CANONICAL_BUILD  # build_cpu, build_vendor, and build_os
AC_CANONICAL_HOST   # host_cpu, host_vendor, and host_os

AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE
AC_CONFIG_HEADERS(config.h)

dnl Do not set default CFLAGS and CXXFLAGS
CXXFLAGS=" -Wall -std=c++11  $CXXFLAGS"
if test "$BUILD_TYPE" == "Debug" ; then
    # set up debug mode
    CXXFLAGS="$CXXFLAGS -O0 -g3"
else
    # default is release mode
    CXXFLAGS="$CXXFLAGS -O3"
fi

AC_ISC_POSIX
AC_PROG_CPP
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_CC_STDC
AC_PROG_RANLIB
AX_PROG_JAVAC
AX_PROG_JAVA
AX_PROG_JAR

AM_PROG_LEX
AC_PROG_YACC

dnl Build doxygen documentation
DX_DOXYGEN_FEATURE(ON)
DX_DOT_FEATURE(ON)
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN([$PACKAGE_NAME],[doxygen.cfg],[doc])

dnl add boost configuration for c-preprocessor
AX_BOOST_BASE([1.54],, [AC_MSG_ERROR([Souffle needs Boost, but it was not found.])])
AX_BOOST_SYSTEM
AX_BOOST_FILESYSTEM
AX_BOOST_THREAD
AX_BOOST_DATE_TIME

# Check for flex
AC_CHECK_PROG(FLEX, flex, flex)
AS_IF([test -z "$FLEX"], [AC_MSG_ERROR([flex not found.])])

# Check for bison
AC_CHECK_PROG(BISON, bison, bison)
AS_IF([test -z "$BISON"], [AC_MSG_ERROR([bison not found.])])

# Enable packaging option
AC_ARG_ENABLE(
  [host-packaging],
  [enable packaging using a given distribution],
  [SOUFFLE_PACKAGING=1],
  [unset SOUFFLE_PACKAGING]
)

# Get the name of the distribution
if test -n "$SOUFFLE_PACKAGING"; then
  case $host_os in
    darwin* )
      DISTRIBUTION=osx
    ;;
    linux*)
      # Check for bison
      AC_CHECK_PROG(LSB_RELEASE, lsb_release, lsb_release)
      AS_IF([test -z "$LSB_RELEASE"], [AC_MSG_ERROR([lsb_release required to detect LINUX distribution not found.])])
      DISTRIBUTION=m4_esyscmd([lsb_release -i | sed 's/^.*:[ \t]\+//' | tr [A-Z] [a-z]])
    ;;
    *BSD*)
      DISTRIBUTION=bsd
    ;;
    *)
      AC_MSG_ERROR([Unsupported platform])
    ;;
  esac
  echo "Enable packaging for: $DISTRIBUTION"
fi

dnl Check for the program(s), define a variable and perform substitution in the
dnl Makefiles if found.  Bail with an error message otherwise
dnl   $1 -- Variable
dnl   $2 -- Program names separated by an empty space
dnl   $3 -- search path (unless specified $PATH is used)
dnl   $4 -- absolute/relative path (unless specified the path is relative and absolute otherwise)
m4_define([DEFNSUBST], [
  unset prog
  for prog in $2; do
    if test -z "`eval echo \$$1`"; then
      if test -z "$4"; then
        AC_CHECK_TOOL($1, $prog, [], $3)
      else
        AC_PATH_PROG($1, $prog, [], $3)
      fi
    fi
  done
  test -z "`eval echo \$$1`" && AC_MSG_FAILURE([Neither of: '$2' found])
  AC_SUBST([$1])
])

# Check the distribution name (if --enable-host-packaging is specified),
# enable conditional AUTOMAKE macro amnd check and define the tool responsible
# for building a package
m4_define(DIST_PACKAGING,[
  unset package
  for distro in $1; do
    if test -n "$SOUFFLE_PACKAGING" && test $DISTRIBUTION = $distro; then
      package=1;
      break;
    fi
  done

  if test -n "$package"; then
    DEFNSUBST($3, $4)
    AM_CONDITIONAL($2, true)
  else
    AM_CONDITIONAL($2, false)
  fi
])

DIST_PACKAGING([debian ubuntu],[DEBIAN_PACKAGE],[DEBUILD],[debuild])
DIST_PACKAGING([osx],[OSX_PACKAGE],[PKGBUILD],[pkgbuild])
DIST_PACKAGING([bsd],[BSD_PACKAGE],[PKGBUILD],[pkgbuild])

dnl Enable pthread library
AC_CHECK_LIB(pthread, pthread_create,,
          [AC_MSG_ERROR([required library pthread missing])])
AC_CHECK_HEADER(sqlite3.h,,[AC_MSG_ERROR([required library sqlite3 missing])])
AC_CHECK_LIB(sqlite3, sqlite3_open,,
          [AC_MSG_ERROR([required library sqlite3 missing])])

dnl Enables OpenMP in the souffle compiler and interpreter
AC_OPENMP
AS_VAR_APPEND(CXXFLAGS, [" $OPENMP_CFLAGS "])

AC_CONFIG_TESTDIR([tests])
AC_CONFIG_FILES([
    Makefile
    src/Makefile
    src/wavelib/Makefile
    prof/Makefile
    tests/Makefile
    tests/atlocal
])
AC_CONFIG_LINKS([include/souffle/CompiledRamOptions.h:src/CompiledRamOptions.h])
AC_CONFIG_LINKS([include/souffle/CompiledRamRelation.h:src/CompiledRamRelation.h])
AC_CONFIG_LINKS([include/souffle/CompiledRamTuple.h:src/CompiledRamTuple.h])
AC_CONFIG_LINKS([include/souffle/CompiledRamRecord.h:src/CompiledRamRecord.h])
AC_CONFIG_LINKS([include/souffle/CompiledSouffle.h:src/CompiledSouffle.h])
AC_CONFIG_LINKS([include/souffle/SouffleInterface.h:src/SouffleInterface.h])
AC_CONFIG_LINKS([include/souffle/ParallelUtils.h:src/ParallelUtils.h])
AC_CONFIG_LINKS([include/souffle/RamTypes.h:src/RamTypes.h])
AC_CONFIG_LINKS([include/souffle/BTree.h:src/BTree.h])
AC_CONFIG_LINKS([include/souffle/Trie.h:src/Trie.h])
AC_CONFIG_LINKS([include/souffle/Table.h:src/Table.h])
AC_CONFIG_LINKS([include/souffle/IterUtils.h:src/IterUtils.h])
AC_CONFIG_LINKS([include/souffle/Util.h:src/Util.h])
AC_CONFIG_LINKS([include/souffle/RamLogger.h:src/RamLogger.h])
AC_CONFIG_LINKS([include/souffle/SymbolTable.h:src/SymbolTable.h])
AC_CONFIG_LINKS([include/souffle/SqliteRelationWriter.h:src/SqliteRelationWriter.h])
AC_CONFIG_LINKS([prof/souffle-profile:prof/souffle-profile])

AM_MISSING_PROG([AUTOM4TE], [autom4te])

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h inttypes.h libintl.h limits.h malloc.h memory.h netdb.h stddef.h stdint.h stdlib.h string.h strings.h sys/time.h sys/timeb.h unistd.h wchar.h wctype.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([dup2 fchdir getcwd getpagesize gettimeofday isascii memset mkdir munmap pow regcomp rmdir setenv socket strcasecmp strchr strdup strerror strrchr strstr strtol strtoull])

AC_OUTPUT(
    [ src/souffle-compile src/souffle-config debian/changelog ],
    [ chmod +x src/souffle-compile src/souffle-config ]
)
