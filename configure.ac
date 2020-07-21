# Souffle - A Datalog Compiler
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

m4_define([SOUFFLE_VERSION], m4_esyscmd([
  souffle_version=$(git describe --tags --always)
  test -z "$souffle_version" && souffle_version=$(pwd | sed '/^.*souffle-[0-9\.]*$/!d' | sed 's/^.*souffle-//')
  test -z "$souffle_version" && souffle_version="unknown"
  echo $souffle_version| tr -d '\n']))

AC_INIT(souffle, SOUFFLE_VERSION, [https://github.com/souffle-lang/souffle/issues], souffle, [https://github.com/souffle-lang/souffle])
AC_CONFIG_AUX_DIR([auxfiles])
AC_PREREQ(2.68)
AC_COPYRIGHT(['2013-15 Oracle and/or its affiliates'])

ISODATE=$(date +%Y-%m-%d)
AC_SUBST([ISODATE])

#LT_PREREQ([2.4])
LT_INIT([dlopen])

AC_CANONICAL_TARGET # target_cpu, target_vendor, and target_os
AC_CANONICAL_BUILD  # build_cpu, build_vendor, and build_os
AC_CANONICAL_HOST   # host_cpu, host_vendor, and host_os

AM_INIT_AUTOMAKE([foreign subdir-objects 1.9 tar-pax])
AM_MAINTAINER_MODE
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_MACRO_DIR([m4])

dnl Do not set default CFLAGS and CXXFLAGS
ENV_CXXFLAGS="$CXXFLAGS"
CXXFLAGS=" -fwrapv "

# Disable provenance
AC_ARG_ENABLE(
  [ncurses],
  AS_HELP_STRING([--disable-ncurses], [Disable ncurses-based provenance display])
)
AS_IF([test "x$enable_ncurses" != "xno"], [
  AX_WITH_CURSES
  AS_IF([test "x$ax_cv_ncurses" != "xyes"],[AC_MSG_ERROR([ncurses library is needed for support of the ncurses provenance display. To build without ncurses support use --disable-ncurses.])])
  AS_VAR_APPEND(LIBS, ["$CURSES_LIB"])
  AS_VAR_APPEND(CXXFLAGS, [" -DUSE_NCURSES "])
])
AM_CONDITIONAL([NCURSES], [test "x$enable_ncurses" != "xno"])

AC_LANG(C++)

# Enable sanitise memory mode
AC_ARG_ENABLE(
  [sanitise-memory],
  [AS_HELP_STRING([--enable-sanitise-memory], [Enable sanitise memory mode])]
)
AS_IF([test "x$enable_sanitise_memory" = "xyes"], [CXXFLAGS="$CXXFLAGS -fsanitize=address,leak"])

# Enable sanitise threads mode
AC_ARG_ENABLE(
  [sanitise-thread],
  [AS_HELP_STRING([--enable-sanitise-thread], [Enable sanitise thread mode])]
)
AS_IF([test "x$enable_sanitise_thread" = "xyes"], [CXXFLAGS="$CXXFLAGS -fsanitize=thread"])

# Enable debug mode
AC_ARG_ENABLE(
  [debug],
  [AS_HELP_STRING([--enable-debug], [Enable debug mode])]
)

AS_IF([test "x$enable_debug" = "xyes"], [CXXFLAGS="$CXXFLAGS -O0 -g3"],
  [test "x$enable_debug" != "xyes"], [CXXFLAGS="$CXXFLAGS -O3"]
)

if test "$BUILD_TYPE" == "Debug" ; then
  AC_MSG_WARN( [BUILD_TYPE environment variable is deprecated. Use --enable-debug])
  # set up debug mode
  CXXFLAGS="$CXXFLAGS -O0 -g3"
fi

# Enable SWIG
AC_ARG_ENABLE(
  [swig],
  [AS_HELP_STRING([--enable-swig], [Enable SWIG])]
)
AS_IF([test "x$enable_swig" = "xyes"], [
  AC_CHECK_PROG(JAVAC, javac, javac)
  AS_IF([test -z "$JAVAC"], [AC_MSG_ERROR([javac not found, required for swig])])
  AC_CHECK_PROG(PYTHON3, python3, python3)
  AS_IF([test -z "$PYTHON3"], [AC_MSG_ERROR([python3 not found, required for swig])])
  AC_CHECK_PROG(SWIG, swig, swig)
  AS_IF([test -z "$SWIG"], [AC_MSG_ERROR([swig not found])])
])
AM_CONDITIONAL([SWIG], [test "x$enable_swig" = "xyes"])

AC_PROG_CXX
AX_CXX_COMPILE_STDCXX(17, noext)

AC_PROG_INSTALL
AC_PROG_MAKE_SET

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

# Check for flex
AC_CHECK_PROG(FLEX, flex, flex)
AS_IF([test -z "$FLEX"], [AC_MSG_ERROR([flex not found.])])

# Check for mcpp
AC_CHECK_PROG(MCPP, mcpp, mcpp)
AS_IF([test -z "$MCPP"], [AC_MSG_ERROR([mcpp not found.])])

# Check for bison
AC_CHECK_PROG(BISON, bison, bison)
AS_IF([test -z "$BISON"], [AC_MSG_ERROR([bison not found.])])
AX_PROG_BISON_VERSION([3.0.4], [], [AC_MSG_ERROR([bison version 3.0.4 is required.])])


# Enable packaging option
AC_ARG_ENABLE(
  [host-packaging],
  AS_HELP_STRING([--enable-host-packaging], [Enable packaging using a given distribution])
)
AS_IF([test "x$enable_host_packaging" = "xyes"], [SOUFFLE_PACKAGING=1],
  [test "x$enable_host_packaging" != "xyes"], [unset SOUFFLE_PACKAGING]
)

# Disable libz
AC_ARG_ENABLE(
  [libz],
  AS_HELP_STRING([--disable-libz], [Disable use of libz file compression])
)
AS_IF([test "x$enable_libz" != "xno"], [
       AC_CHECK_HEADER(zlib.h,,[AC_MSG_ERROR([required library zlib missing. Use --disable-libz to build without gzip file IO.])])
       AC_CHECK_LIB(z, compress,,[AC_MSG_ERROR([required library zlib missingi. Use --disable-libz to build without gzip file IO.])])
    AS_VAR_APPEND(CXXFLAGS, [" -DUSE_LIBZ "])
])
AM_CONDITIONAL([LIBZ], [test "x$enable_libz" != "xno"])

# Disable sqlite3
AC_ARG_ENABLE(
  [sqlite],
  AS_HELP_STRING([--disable-sqlite], [Disable use of sqlite IO])
)
AS_IF([test "x$enable_sqlite" != "xno"], [
    AC_CHECK_HEADER(sqlite3.h,,[AC_MSG_ERROR([required library sqlite3 missing. Use --disable-sqlite without support for SQLite IO support.])])
    AC_CHECK_LIB(sqlite3, sqlite3_open,,[AC_MSG_ERROR([required library sqlite3 missing. Use --disable-sqlite without support for SQLite IO support.])])
    AS_VAR_APPEND(CXXFLAGS, [" -DUSE_SQLITE "])
])
AM_CONDITIONAL([SQLITE], [test "x$enable_sqlite" != "xno"])

if test -n "$SOUFFLE_PACKAGING"; then
  case $host_os in
    darwin* )
      DISTRIBUTION=osx
    ;;
    linux*)
      DISTRIBUTION=[`. /etc/os-release; echo $ID`]
      AS_IF([test -z "$DISTRIBUTION"], [DISTRIBUTION=unknown])
    ;;
    *BSD* | *bsd* )
      DISTRIBUTION=bsd
    ;;
    *)
      AC_MSG_ERROR([Unsupported platform])
    ;;
  esac
  echo "Enable packaging for: $DISTRIBUTION"
fi

AC_ARG_ENABLE(
  [64bit-domain],
  [AS_HELP_STRING([--enable-64bit-domain], [Enable 64-bit number values in Datalog tuples])]
)
AS_IF([test "x$enable_64bit_domain" = "xyes"], [
  AS_VAR_APPEND(CXXFLAGS, [" -DRAM_DOMAIN_SIZE=64"])
])

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

dnl ============================libffi=========================================
m4_define_default([PKG_PROG_PKG_CONFIG],[])
m4_define_default([PKG_CHECK_MODULES],
   [
  case $2 in
    libffi )
      AC_CHECK_LIB(ffi, ffi_call,,
          [AC_MSG_ERROR([required library ffi missing])])
      AC_CHECK_HEADER(ffi.h,,[AC_MSG_ERROR([required headers ffi.h missing.])])
    ;;
    * )
      AC_MSG_ERROR([Unsupported library])
    ;;
  esac
])

# Executed if PKG_CONFIG is available
PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(FFI, [libffi])

dnl Enable pthread library
AC_CHECK_LIB(pthread, pthread_create,,
    [AC_MSG_ERROR([required library pthread missing])])

dnl Enable dynamic loadable libaries
AC_CHECK_LIB(dl, dlopen,,
    [AC_MSG_ERROR([required library dynamic load missing])])

dnl Enables OpenMP in the souffle compiler and interpreter
AC_OPENMP
AS_VAR_APPEND(CXXFLAGS, [" $OPENMP_CXXFLAGS "])
AC_MSG_CHECKING( for target cpu)

AC_CONFIG_TESTDIR([tests])
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  src/tests/Makefile
  tests/Makefile
  tests/atlocal
  tests/interface/functors/Makefile
])
AC_CONFIG_LINKS([include/souffle/BinaryConstraintOps.h:src/BinaryConstraintOps.h])
AC_CONFIG_LINKS([include/souffle/BTree.h:src/BTree.h])
AC_CONFIG_LINKS([include/souffle/CompiledOptions.h:src/CompiledOptions.h])
AC_CONFIG_LINKS([include/souffle/RecordTable.h:src/RecordTable.h])
AC_CONFIG_LINKS([include/souffle/CompiledSouffle.h:src/CompiledSouffle.h])
AC_CONFIG_LINKS([include/souffle/CompiledTuple.h:src/CompiledTuple.h])
AC_CONFIG_LINKS([include/souffle/EventProcessor.h:src/EventProcessor.h])
AC_CONFIG_LINKS([include/souffle/Explain.h:src/Explain.h])
AC_CONFIG_LINKS([include/souffle/ExplainProvenance.h:src/ExplainProvenance.h])
AC_CONFIG_LINKS([include/souffle/ExplainProvenanceImpl.h:src/ExplainProvenanceImpl.h])
AC_CONFIG_LINKS([include/souffle/ExplainTree.h:src/ExplainTree.h])
AC_CONFIG_LINKS([include/souffle/EquivalenceRelation.h:src/EquivalenceRelation.h])
AC_CONFIG_LINKS([include/souffle/IOSystem.h:src/IOSystem.h])
AC_CONFIG_LINKS([include/souffle/LambdaBTree.h:src/LambdaBTree.h])
AC_CONFIG_LINKS([include/souffle/Logger.h:src/Logger.h])
AC_CONFIG_LINKS([include/souffle/PiggyList.h:src/PiggyList.h])
AC_CONFIG_LINKS([include/souffle/ProfileDatabase.h:src/ProfileDatabase.h])
AC_CONFIG_LINKS([include/souffle/ProfileEvent.h:src/ProfileEvent.h])
AC_CONFIG_LINKS([include/souffle/RamTypes.h:src/RamTypes.h])
AC_CONFIG_LINKS([include/souffle/ReadStream.h:src/ReadStream.h])
AC_CONFIG_LINKS([include/souffle/ReadStreamCSV.h:src/ReadStreamCSV.h])
AC_CONFIG_LINKS([include/souffle/ReadStreamSQLite.h:src/ReadStreamSQLite.h])
AC_CONFIG_LINKS([include/souffle/ReadStreamJSON.h:src/ReadStreamJSON.h])
AC_CONFIG_LINKS([include/souffle/SerialisationStream.h:src/SerialisationStream.h])
AC_CONFIG_LINKS([include/souffle/SignalHandler.h:src/SignalHandler.h])
AC_CONFIG_LINKS([include/souffle/SouffleInterface.h:src/SouffleInterface.h])
AC_CONFIG_LINKS([include/souffle/SymbolTable.h:src/SymbolTable.h])
AC_CONFIG_LINKS([include/souffle/Table.h:src/Table.h])
AC_CONFIG_LINKS([include/souffle/Brie.h:src/Brie.h])
AC_CONFIG_LINKS([include/souffle/UnionFind.h:src/UnionFind.h])
AC_CONFIG_LINKS([include/souffle/utility/CacheUtil.h:src/utility/CacheUtil.h])
AC_CONFIG_LINKS([include/souffle/utility/ContainerUtil.h:src/utility/ContainerUtil.h])
AC_CONFIG_LINKS([include/souffle/utility/EvaluatorUtil.h:src/utility/EvaluatorUtil.h])
AC_CONFIG_LINKS([include/souffle/utility/FileUtil.h:src/utility/FileUtil.h])
AC_CONFIG_LINKS([include/souffle/utility/FunctionalUtil.h:src/utility/FunctionalUtil.h])
AC_CONFIG_LINKS([include/souffle/utility/MiscUtil.h:src/utility/MiscUtil.h])
AC_CONFIG_LINKS([include/souffle/utility/StreamUtil.h:src/utility/StreamUtil.h])
AC_CONFIG_LINKS([include/souffle/utility/StringUtil.h:src/utility/StringUtil.h])
AC_CONFIG_LINKS([include/souffle/utility/ParallelUtil.h:src/utility/ParallelUtil.h])
AC_CONFIG_LINKS([include/souffle/utility/tinyformat.h:src/utility/tinyformat.h])
AC_CONFIG_LINKS([include/souffle/WriteStream.h:src/WriteStream.h])
AC_CONFIG_LINKS([include/souffle/WriteStreamCSV.h:src/WriteStreamCSV.h])
AC_CONFIG_LINKS([include/souffle/WriteStreamSQLite.h:src/WriteStreamSQLite.h])
AC_CONFIG_LINKS([include/souffle/WriteStreamJSON.h:src/WriteStreamJSON.h])
AC_CONFIG_LINKS([include/souffle/gzfstream.h:src/gzfstream.h])
AC_CONFIG_LINKS([include/souffle/json11.h:src/json11.h])
AC_CONFIG_LINKS([include/souffle/profile/Cell.h:src/profile/Cell.h])
AC_CONFIG_LINKS([include/souffle/profile/CellInterface.h:src/profile/CellInterface.h])
AC_CONFIG_LINKS([include/souffle/profile/Cli.h:src/profile/Cli.h])
AC_CONFIG_LINKS([include/souffle/profile/DataComparator.h:src/profile/DataComparator.h])
AC_CONFIG_LINKS([include/souffle/profile/Iteration.h:src/profile/Iteration.h])
AC_CONFIG_LINKS([include/souffle/profile/OutputProcessor.h:src/profile/OutputProcessor.h])
AC_CONFIG_LINKS([include/souffle/profile/ProgramRun.h:src/profile/ProgramRun.h])
AC_CONFIG_LINKS([include/souffle/profile/Reader.h:src/profile/Reader.h])
AC_CONFIG_LINKS([include/souffle/profile/Relation.h:src/profile/Relation.h])
AC_CONFIG_LINKS([include/souffle/profile/Row.h:src/profile/Row.h])
AC_CONFIG_LINKS([include/souffle/profile/Rule.h:src/profile/Rule.h])
AC_CONFIG_LINKS([include/souffle/profile/StringUtils.h:src/profile/StringUtils.h])
AC_CONFIG_LINKS([include/souffle/profile/Table.h:src/profile/Table.h])
AC_CONFIG_LINKS([include/souffle/profile/Tui.h:src/profile/Tui.h])
AC_CONFIG_LINKS([include/souffle/profile/UserInputReader.h:src/profile/UserInputReader.h])
AC_CONFIG_LINKS([include/souffle/profile/HtmlGenerator.h:src/profile/HtmlGenerator.h])
AC_CONFIG_LINKS([include/souffle/profile/htmlCssChartist.h:src/profile/htmlCssChartist.h])
AC_CONFIG_LINKS([include/souffle/profile/htmlJsChartistMin.h:src/profile/htmlJsChartistMin.h])
AC_CONFIG_LINKS([include/souffle/profile/htmlJsMain.h:src/profile/htmlJsMain.h])
AC_CONFIG_LINKS([include/souffle/profile/htmlJsUtil.h:src/profile/htmlJsUtil.h])
AC_CONFIG_LINKS([include/souffle/profile/htmlCssStyle.h:src/profile/htmlCssStyle.h])
AC_CONFIG_LINKS([include/souffle/profile/htmlJsChartistPlugin.h:src/profile/htmlJsChartistPlugin.h])
AC_CONFIG_LINKS([include/souffle/profile/htmlJsTableSort.h:src/profile/htmlJsTableSort.h])
AC_CONFIG_LINKS([include/souffle/profile/htmlMain.h:src/profile/htmlMain.h])
AC_CONFIG_LINKS([include/souffle/swig/SwigInterface.h:src/swig/SwigInterface.h])
AC_CONFIG_LINKS([include/souffle/swig/SwigInterface.i:src/swig/SwigInterface.i])
AC_CONFIG_LINKS([utilities/bash-completion/completions/souffle:debian/souffle.bash-completion])

AM_MISSING_PROG([AUTOM4TE], [autom4te])

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h inttypes.h libintl.h limits.h malloc.h memory.h netdb.h stddef.h stdint.h stdlib.h string.h strings.h sys/time.h sys/timeb.h unistd.h wchar.h wctype.h])

SOUFFLE_CXXFLAGS="$CXXFLAGS"
AC_SUBST(SOUFFLE_CXXFLAGS)
CXXFLAGS="-Wall -Wextra $CXXFLAGS $ENV_CXXFLAGS"

AC_OUTPUT(
  [ src/souffle-compile src/souffle-config debian/changelog man/souffle.1 man/souffle-compile.1 man/souffle-config.1 man/souffle-profile.1],
  [ chmod +x src/souffle-compile src/souffle-config ]
)
