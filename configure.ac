# Souffle - A Datalog Compiler
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

AC_INIT(souffle, m4_esyscmd([git describe --tags --abbrev=0 --always | tr -d '\n']), [souffle-talk@googlegroups.com])
AC_PREREQ(2.68)
AC_COPYRIGHT(['2013-15 Oracle and/or its affiliates'])

#LT_PREREQ([2.4])
LT_INIT([dlopen])

AC_CANONICAL_TARGET # target_cpu, target_vendor, and target_os
AC_CANONICAL_BUILD  # build_cpu, build_vendor, and build_os
AC_CANONICAL_HOST   # host_cpu, host_vendor, and host_os

AM_INIT_AUTOMAKE([foreign subdir-objects 1.9 tar-pax])
AM_MAINTAINER_MODE
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_MACRO_DIR([m4])

# Enable jni
AC_ARG_ENABLE(
  [jni],
  [AS_HELP_STRING([--enable-jni], [Enable java interface])]
)
AS_IF([test "x$enable_jni" = "xyes"], [
  AS_VAR_APPEND(CXXFLAGS, [" -DUSE_JAVAI "])
  # Java
  AX_PROG_JAVAC
  AX_PROG_JAVAH
  AX_PROG_JAVA
  AX_PROG_JAR
  AX_JNI_INCLUDE_DIR
  for dir in $JNI_INCLUDE_DIRS; do
    JNIFLAGS="$JNIFLAGS -I$dir"
  done
  AC_SUBST([JNIFLAGS])
])
AM_CONDITIONAL([JAVAI], [test "x$enable_jni" = "xyes"])

# Enable curses library
AX_WITH_CURSES
AS_VAR_APPEND(LIBS, ["$CURSES_LIB"])

dnl Do not set default CFLAGS and CXXFLAGS
CXXFLAGS=" -Wall -std=c++11  $CXXFLAGS"

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

AC_ISC_POSIX
AC_PROG_CPP
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_CC_STDC

# Check for flex and bison
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

# Check for flex
AC_CHECK_PROG(FLEX, flex, flex)
AS_IF([test -z "$FLEX"], [AC_MSG_ERROR([flex not found.])])

# Check for bison
AC_CHECK_PROG(BISON, bison, bison)
AS_IF([test -z "$BISON"], [AC_MSG_ERROR([bison not found.])])

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
    AC_CHECK_HEADER(zlib.h,,[AC_MSG_ERROR([required library zlib missing])])
    AC_CHECK_LIB(z, compress,,[AC_MSG_ERROR([required library zlib missing])])
    AS_VAR_APPEND(CXXFLAGS, [" -DUSE_LIBZ "])
])
AM_CONDITIONAL([LIBZ], [test "x$enable_libz" != "xno"])

# Disable sqlite3
AC_ARG_ENABLE(
  [sqlite],
  AS_HELP_STRING([--disable-sqlite], [Disable use of sqlite IO])
)
AS_IF([test "x$enable_sqlite" != "xno"], [
    AC_CHECK_HEADER(sqlite3.h,,[AC_MSG_ERROR([required library sqlite3 missing])])
    AC_CHECK_LIB(sqlite3, sqlite3_open,,[AC_MSG_ERROR([required library sqlite3 missing])])
    AS_VAR_APPEND(CXXFLAGS, [" -DUSE_SQLITE "])
])
AM_CONDITIONAL([SQLITE], [test "x$enable_sqlite" != "xno"])

##
## START PART 1 MCPP DEFS
##


dnl Define host system and target system

case $host_os in
linux*)
    AC_DEFINE( [HOST_SYSTEM], [SYS_LINUX], [Define the host system.])
    ac_c_define_HOST_SYSTEM=[SYS_LINUX]
    ;;
freebsd*)
    AC_DEFINE( [HOST_SYSTEM], [SYS_FREEBSD])
    ac_c_define_HOST_SYSTEM=[SYS_FREEBSD]
    ;;
darwin*)
    AC_DEFINE( [HOST_SYSTEM], [SYS_MAC])
    ac_c_define_HOST_SYSTEM=[SYS_MAC]
    ;;
cygwin*)
    AC_DEFINE( [HOST_SYSTEM], [SYS_CYGWIN])
    ac_c_define_HOST_SYSTEM=[SYS_CYGWIN]
    ;;
mingw*)
    AC_DEFINE( [HOST_SYSTEM], [SYS_MINGW])
    ac_c_define_HOST_SYSTEM=[SYS_MINGW]
    ;;
*)
    AC_MSG_WARN( [Unsupported host OS, assuming to be an UNIX variant])
    AC_DEFINE( [HOST_SYSTEM], [SYS_UNIX])
    ac_c_define_HOST_SYSTEM=[SYS_UNIX]
    ;;
esac
host_system=$ac_c_define_HOST_SYSTEM

case $target_os in
linux*)
    AC_DEFINE( [SYSTEM], [SYS_LINUX], [Define the target system.])
    ;;
freebsd*)
    AC_DEFINE( [SYSTEM], [SYS_FREEBSD])
    ;;
darwin*)
    AC_DEFINE( [SYSTEM], [SYS_MAC])
    ;;
cygwin*)
    AC_DEFINE( [SYSTEM], [SYS_CYGWIN])
    ;;
mingw*)
    AC_DEFINE( [SYSTEM], [SYS_MINGW])
    ;;
*)
    AC_MSG_WARN( Unsupported target OS, assuming to be an UNIX variant)
    AC_DEFINE( [SYSTEM], [SYS_UNIX])
    ;;
esac

AC_DEFINE( [COMPILER], [INDEPENDENT], [Define the target compiler.])
AC_MSG_CHECKING( for target cpu)

## These are not predefined macros of MCPP.  MCPP will define predefined macros
## on compile time based on the CPU macro, and possibly redefine them at an
## execution time.

case $target_cpu in
x86_64|amd64)
    Target_Cpu=x86_64
    ;;
i?86*)
    Target_Cpu=i386
    ;;
powerpc64|ppc64)
    Target_Cpu=ppc64
    ;;
powerpc|ppc|ppc7400)
    Target_Cpu=ppc
    ;;
*)
    Target_Cpu=$target_cpu
    ;;
esac

AC_MSG_RESULT( $Target_Cpu)
AC_DEFINE_UNQUOTED( [CPU], "$Target_Cpu", [Define the cpu-specific-macro.])

##
## END PART 1 MCPP DEFS
##

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

dnl Enables OpenMP in the souffle compiler and interpreter
AC_OPENMP
AS_VAR_APPEND(CXXFLAGS, [" $OPENMP_CFLAGS "])
AC_MSG_CHECKING( for target cpu)

## These are not predefined macros of MCPP.  MCPP will define predefined macros
## on compile time based on the CPU macro, and possibly redefine them at an
## execution time.

case $target_cpu in
x86_64|amd64)
    Target_Cpu=x86_64
    ;;
i?86*)
    Target_Cpu=i386
    ;;
powerpc64|ppc64)
    Target_Cpu=ppc64
    ;;
powerpc|ppc|ppc7400)
    Target_Cpu=ppc
    ;;
*)
    Target_Cpu=$target_cpu
    ;;
esac

AC_MSG_RESULT( $Target_Cpu)
AC_DEFINE_UNQUOTED( [CPU], "$Target_Cpu", [Define the cpu-specific-macro.])

AC_DEFINE_UNQUOTED( [OBJEXT], ["$ac_cv_objext"],
				       [Define the suffix of object file.])

if test x$ac_cv_c_compiler_gnu = xyes; then

    AC_DEFINE( [HOST_COMPILER], [GNUC], [Define the host compiler.])

    AC_DEFINE( [HOST_CMP_NAME], "GCC", [Define the host compiler name.])

    AC_MSG_CHECKING( for value of __GNUC__)
    gcc_maj_ver=`echo '__GNUC__' | $CPP - | sed 's/^ *//' | $EGREP -v '^#|^$' | sed 's/ *//'`
    AC_MSG_RESULT( $gcc_maj_ver)
    AC_DEFINE_UNQUOTED( [GCC_MAJOR_VERSION], "$gcc_maj_ver",
        [Define gcc major version.])
    AC_MSG_CHECKING( for value of __GNUC_MINOR__)
    gcc_min_ver=`echo __GNUC_MINOR__ | $CPP - | sed 's/^ *//' | $EGREP -v '^#|^$' | sed 's/ *//'`
    AC_MSG_RESULT( $gcc_min_ver)
    AC_DEFINE_UNQUOTED( [GCC_MINOR_VERSION], "$gcc_min_ver",
        [Define gcc minor version.])

    cw=`pwd`
    if test x$srcdir = "x." || test x$srcdir = x; then
        src_dir=$cw
    else
        src_dir=$srcdir
    fi
    if test $ac_c_define_HOST_SYSTEM = SYS_CYGWIN; then
        AC_CACHE_CHECK( the root directory,
            ac_cv_cygwin_root,
            [ac_cv_cygwin_root=`$src_dir/config/cygwin_root`]
        )
        AC_DEFINE_UNQUOTED( [CYGWIN_ROOT_DIRECTORY], "$ac_cv_cygwin_root",
            [Define root directory of CYGWIN.])
    fi
    if test $ac_c_define_HOST_SYSTEM = SYS_MINGW; then
        AC_CACHE_CHECK( the root directory,
            ac_cv_msys_root,
            cd /tmp
            cat > getcwd.c <<\_EOF
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
int main( int argc, char ** argv)
{   char    buf@<:@ PATH_MAX@:>@;
    getcwd( buf, PATH_MAX);
    puts( buf);
    return 0;
}
_EOF
            $CC getcwd.c
            cd /
            [ac_cv_msys_root=`$src_dir/config/mingw_root`]
        )
        AC_CACHE_CHECK( the /mingw directory,
            ac_cv_mingw_dir,
            cd /mingw
            [ac_cv_mingw_dir=`$src_dir/config/mingw_root`]
        )
        cd $cw
        rm -f /tmp/getcwd.c /tmp/a.exe
        AC_DEFINE_UNQUOTED( [MSYS_ROOT_DIRECTORY], "$ac_cv_msys_root",
            [Define root directory of MSYS.])
        AC_DEFINE_UNQUOTED( [MINGW_DIRECTORY], "$ac_cv_mingw_dir",
            [Define /mingw directory.])
    fi

else    # not gcc

# mcpp_AC_C_DEFINE( SYSTEM | CPU | COMPILER, STD | OLD, $enableval)
# Specify the target-specific predefined macro.
AC_DEFUN([mcpp_AC_C_DEFINE], [
    name=`echo $3 | sed 's/=.*//'`
    mcpp_macro=$1_SP_$2
    stripped_mcpp_macro=`echo $mcpp_macro | sed 's/STD//'`
    if test x$stripped_mcpp_macro != x$mcpp_macro; then
        stripped_name=`echo $name | sed 's/^_//'`
        if test x$stripped_name = x$name; then
            AC_MSG_ERROR( The macro name '$name' should begin with '_'.)
        fi
    else
        stripped_name=`echo $name | sed 's/^@<:@A-Za-z@:>@//'`
        if test x$stripped_name = x$name; then
            AC_MSG_ERROR( The macro name '$name' should begin with an alphabetic letter.)
        fi
    fi
    if test x$name = x$3; then
        [replace=1]
    else
        [replace=`echo $3 | sed 's/^.*=//'`]
    fi
    AC_DEFINE_UNQUOTED( [$mcpp_macro], "$name")
    AC_DEFINE_UNQUOTED( [${mcpp_macro}_VAL], "$replace")
    mcpp_ac_c_define_$1_$2=$name
])

# mcpp_AC_C_PREDEF( SYSTEM | CPU | COMPILER, STD | OLD, additional description)
# Specify the target-specific predefined macro via mcpp_AC_C_DEFINE.
AC_DEFUN([mcpp_AC_C_PREDEF], [
    AC_ARG_ENABLE( $1-$2-macro,
        AC_HELP_STRING( --enable-$1-$2-macro=name@<:@=replace@:>@,
            [specify the $1-specific $2-style predefined macro.  $3]),
        if test x$enableval = xyes || test x$enableval = xno; then
            AC_MSG_ERROR( This option requires an argument as 'name@<:@=replace@:>@'.)
        fi
        mcpp_AC_C_DEFINE( $1, $2, $enableval)
        AH_TEMPLATE( [$1_SP_$2],
            [Define the name of $1-specific $2-style predefined macro.])
        AH_TEMPLATE( [$1_SP_$2_VAL],
            [Define the value of $1-specific $2-style predefined macro.])
    )
])

mcpp_AC_C_PREDEF( [SYSTEM], [STD],
    [In all this and the following 5 options,
    'STD'-style name begins with an '_',
    'OLD'-style name begins with an alphabetic and
    if '=replace' is missing the macro is defined as 1.]
)
mcpp_AC_C_PREDEF( [SYSTEM], [OLD])
mcpp_AC_C_PREDEF( [CPU], [STD])
mcpp_AC_C_PREDEF( [CPU], [OLD])
mcpp_AC_C_PREDEF( [COMPILER], [STD])
mcpp_AC_C_PREDEF( [COMPILER], [OLD])

fi

dnl Check which header file CPP includes,
dnl relative to CURRENT or SOURCE directory.

# AC_C_INCLUDE_PREF( CURRENT | SOURCE | NEXT)
# Check the include preference ('NEXT' means CURRENT & SOURCE)
AC_DEFUN([AC_C_INCLUDE_PREF], [
    AC_EGREP_CPP( $1, [
#include "../next/header.h"
],
        [ac_cv_c_include_pref=$1],
    )
])

AC_CACHE_CHECK( include preference - relative to current or source directory,
    ac_cv_c_include_pref,
    mkdir mcpp-tmp
    cd mcpp-tmp
    mkdir dir ../next ../next/dir
    echo '#include "dir/what.h"'    > ../next/header.h
    echo '#include "next.h"'        >> ../next/header.h
    echo 'CURRENT'  > dir/what.h
    echo 'SOURCE'   > ../next/dir/what.h
    echo 'NEXT'     > ../next/next.h
    AC_C_INCLUDE_PREF( [SOURCE])
    if test x$ac_cv_c_include_pref != xSOURCE; then
        AC_C_INCLUDE_PREF( [CURRENT])
        if test x$ac_cv_c_include_pref = xCURRENT; then
            AC_C_INCLUDE_PREF( [NEXT])
            if test x$ac_cv_c_include_pref = xNEXT; then
                [ac_cv_c_include_pref="(CURRENT & SOURCE)"]
            fi
        else
            AC_MSG_ERROR( Failed to include header file.)
        fi
    fi
    cd ..
    rm -rf mcpp-tmp next
    if test $ac_c_define_HOST_SYSTEM = SYS_CYGWIN \
            || test $ac_c_define_HOST_SYSTEM = SYS_MINGW; then
        sleep 1         # wait for cleanup
    fi
)
AC_DEFINE_UNQUOTED( [SEARCH_INIT], $ac_cv_c_include_pref,
    [Define include preference.])


# END MCPP CPU CHECK


AC_CONFIG_TESTDIR([tests])
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  interfaces/Makefile
  tests/Makefile
  tests/atlocal
])
AC_CONFIG_LINKS([include/souffle/CompiledRamOptions.h:src/CompiledRamOptions.h])
AC_CONFIG_LINKS([include/souffle/CompiledRamIndexUtils.h:src/CompiledRamIndexUtils.h])
AC_CONFIG_LINKS([include/souffle/CompiledRamRelation.h:src/CompiledRamRelation.h])
AC_CONFIG_LINKS([include/souffle/CompiledRamTuple.h:src/CompiledRamTuple.h])
AC_CONFIG_LINKS([include/souffle/CompiledRamRecord.h:src/CompiledRamRecord.h])
AC_CONFIG_LINKS([include/souffle/CompiledSouffle.h:src/CompiledSouffle.h])
AC_CONFIG_LINKS([include/souffle/SouffleInterface.h:src/SouffleInterface.h])
AC_CONFIG_LINKS([include/souffle/ParallelUtils.h:src/ParallelUtils.h])
AC_CONFIG_LINKS([include/souffle/RamTypes.h:src/RamTypes.h])
AC_CONFIG_LINKS([include/souffle/AstTypes.h:src/AstTypes.h])
AC_CONFIG_LINKS([include/souffle/BTree.h:src/BTree.h])
AC_CONFIG_LINKS([include/souffle/Trie.h:src/Trie.h])
AC_CONFIG_LINKS([include/souffle/BinaryRelation.h:src/BinaryRelation.h])
AC_CONFIG_LINKS([include/souffle/BlockList.h:src/BlockList.h])
AC_CONFIG_LINKS([include/souffle/UnionFind.h:src/UnionFind.h])
AC_CONFIG_LINKS([include/souffle/Table.h:src/Table.h])
AC_CONFIG_LINKS([include/souffle/IterUtils.h:src/IterUtils.h])
AC_CONFIG_LINKS([include/souffle/Util.h:src/Util.h])
AC_CONFIG_LINKS([include/souffle/Macro.h:src/Macro.h])
AC_CONFIG_LINKS([include/souffle/RamLogger.h:src/RamLogger.h])
AC_CONFIG_LINKS([include/souffle/Explain.h:src/Explain.h])
AC_CONFIG_LINKS([include/souffle/SignalHandler.h:src/SignalHandler.h])
AC_CONFIG_LINKS([include/souffle/IODirectives.h:src/IODirectives.h])
AC_CONFIG_LINKS([include/souffle/IOSystem.h:src/IOSystem.h])
AC_CONFIG_LINKS([include/souffle/gzfstream.h:src/gzfstream.h])
AC_CONFIG_LINKS([include/souffle/ReadStream.h:src/ReadStream.h])
AC_CONFIG_LINKS([include/souffle/ReadStreamCSV.h:src/ReadStreamCSV.h])
AC_CONFIG_LINKS([include/souffle/ReadStreamSQLite.h:src/ReadStreamSQLite.h])
AC_CONFIG_LINKS([include/souffle/WriteStream.h:src/WriteStream.h])
AC_CONFIG_LINKS([include/souffle/WriteStreamCSV.h:src/WriteStreamCSV.h])
AC_CONFIG_LINKS([include/souffle/WriteStreamSQLite.h:src/WriteStreamSQLite.h])
AC_CONFIG_LINKS([include/souffle/SymbolMask.h:src/SymbolMask.h])
AC_CONFIG_LINKS([include/souffle/SymbolTable.h:src/SymbolTable.h])

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
  [ src/souffle-compile src/souffle-config interfaces/souffle-compilelib debian/changelog ],
  [ chmod +x src/souffle-compile src/souffle-config interfaces/souffle-compilelib ]
)
