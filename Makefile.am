# Souffle - A Datalog Compiler
# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

# add macros
ACLOCAL_AMFLAGS = -I m4

# directories
SUBDIRS = src tests include

# add doxygen support to the makefile
include $(top_srcdir)/aminclude.am

# add doxygen configuration to the distribution
EXTRA_DIST = doxygen.cfg

# clean up the autoconf cache
distclean-local:
	-rm -rf autom4te.cache

# Debian packaging
if DEBIAN_PACKAGE
dpkg_dir = packaging
dist_src = $(PACKAGE)-$(VERSION).tar.gz
deb_src  = $(PACKAGE)_$(VERSION).orig.tar.gz

package:
	@which "$(DEBUILD)" 2>&1 > /dev/null || { echo "No debuild executable found" 1>&2 && exit 1; }
	rm -rf $(dpkg_dir)
	$(MKDIR_P) $(dpkg_dir)
	$(MAKE) dist
	tar -xf $(dist_src) -C $(dpkg_dir)
	cp -r debian $(dpkg_dir)/$(distdir)
	mv $(dist_src) $(dpkg_dir)/$(deb_src)
	cd $(dpkg_dir)/$(distdir) && DEB_BUILD_OPTIONS=nocheck $(DEBUILD) -us -uc

clean-local:
	-rm -rf $(dpkg_dir)
endif

# MacOS packaging
if OSX_PACKAGE
package:
	@test -d $(distdir) \
		&& { echo "$(distdir) directory exists. (Re)move it before creating a MacOS package" 2>&1 && exit 1; } \
		|| true
	$(MAKE) distdir \
		&& pushd $(distdir) \
		&& $(MKDIR_P) `pwd`/pkg \
		&& ./configure --prefix `pwd`/pkg \
		&& $(MAKE) -j8 install \
		&& $(PKGBUILD) \
			--root `pwd`/pkg \
			--identifier com.oracle.souffle \
			--version $(VERSION) \
			--install-location /usr/local \
			$(abs_top_builddir)/$(distdir).pkg \
		&& popd \
		&& rm -rf $(distdir)

clean-local:
	-rm -f $(abs_top_builddir)/$(distdir).pkg
endif

completionsdir=$(datadir)/bash-completion/completions
completions_DATA=utilities/bash-completion/completions/souffle

# Man pages
dist_man1_MANS = $(wildcard $(srcdir)/man/*.1)

#Linting
lint: format tidy

# Run clang-format on all modified cpp/h files
format:
	cd $(abs_top_srcdir); \
	cpplist='`git diff --diff-filter=dr --name-only *.cpp *.h` `git diff --diff-filter=dr --name-only *.cpp *.h`'; \
	for file in $$cpplist; \
	do \
		clang-format -i $$file; \
	done;

# Run clang-tidy on all modified cpp files
tidy:
	cd $(abs_top_srcdir); \
	cpplist="`git diff --cached --diff-filter=dr --name-only *cpp` `git diff --diff-filter=dr --name-only *cpp`"; \
	cd $(abs_top_builddir)/src; \
	for file in $$cpplist; \
	do \
		clang-tidy $(abs_top_srcdir)/$$file --quiet --extra-arg=-std=c++17 -- -I$(abs_top_builddir)/src -I. -I$(abs_top_srcdir)/src -I$(abs_top_builddir)/include -I$(abs_top_builddir) $(CXXFLAGS) ;\
	done;

# Run clang-format on all cpp/h files known to git
format-all:
	cd $(abs_top_srcdir); \
	cpplist=`git ls-files *.cpp *.h`; \
	for file in $$cpplist; \
	do \
		clang-format -i $$file; \
	done;

# Run clang-tidy on all cpp files known to git
tidy-all:
	cd $(abs_top_srcdir); \
	cpplist=`git ls-files *.cpp`; \
	cd $(abs_top_builddir)/src; \
	for file in $$cpplist; \
	do \
		clang-tidy $(abs_top_srcdir)/$$file --quiet --extra-arg=-std=c++17 -- -I$(abs_top_builddir)/src -I. -I$(abs_top_srcdir)/src -I$(abs_top_builddir)/include -I$(abs_top_builddir) $(CXXFLAGS) ;\
	done;

