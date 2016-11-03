# Souffle - A Datalog Compiler
# (c) 2016 by the Souffle Development Team
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

SUFFIXES = .cpp .h .cc .o .a .class .java

souffle_jar = com.soufflelang.souffle.jar
dist_bin_SCRIPTS = souffle-compilelib

# Java source files
java_sources = $(wildcard $(srcdir)/jni/java/*.java)

# Distribute the Java sources
EXTRA_DIST = examples $(java_sources)

jni_sources = \
  jni/src/Arg.cpp \
  jni/src/Clause.cpp \
  jni/src/Data.cpp \
  jni/src/PrimData.cpp \
  jni/src/Relation.cpp \
  jni/src/Solver.cpp \
  jni/src/Atom.cpp \
  jni/src/Const.cpp \
  jni/src/Executor.cpp \
  jni/src/PrimType.cpp \
  jni/src/Result.cpp \
  jni/src/Var.cpp \
  jni/src/BinOp.cpp \
  jni/src/Constraint.cpp \
  jni/src/Negation.cpp \
  jni/src/Program.cpp \
  jni/src/SConst.cpp

# Heades generated from the Java class files
cpp_headers = $(patsubst jni/src/%.cpp, $(builddir)/jni/include/Jni_%.h, $(jni_sources))

BUILT_SOURCES = $(cpp_headers)

$(souffle_jar): $(java_sources)
	$(JAVAC) -d $(builddir) $?
	$(JAR) -cf $@ $(builddir)/com/soufflelang/souffle/*.class

all-local: $(souffle_jar)

$(cpp_headers): jni/include/Jni_%.h : jni/src/%.cpp $(souffle_jar)
	$(JAVAH) -jni -o $@ com.soufflelang.souffle.$(basename $(notdir $<))

# libsoufflejni.a library
lib_LTLIBRARIES = libsoufflejni.la
libsoufflejni_la_SOURCES = $(jni_sources) \
  src/AstBuilder.cpp	\
  src/Interfaces.cpp	\
  include/AstBuilder.h \
  include/Executor.h \
  include/InterfaceResult.h \
  include/Interfaces.h \
  include/Logger.h \
  jni/include/conversion.h  \
  jni/include/handle.h

libsoufflejni_la_CXXFLAGS = $(JNIFLAGS) \
  -I@srcdir@/include -I@builddir@/jni/include -I@srcdir@/jni/include \
	-I@top_srcdir@ -I@top_srcdir@/src -I@top_srcdir@/include

libsoufflejni_la_LIBADD = @top_builddir@/src/libsouffle.la

# Cleanup
CLEANFILES = $(souffle_jar) $(BUILT_SOURCES)

clean-local:
	rm -rf $(builddir)/com
