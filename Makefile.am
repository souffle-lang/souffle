# Souffle - A Datalog Compiler
# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

# add macros
ACLOCAL_AMFLAGS = -I m4

# directories
if JAVAI
java_dirs=interfaces
else
java_dirs=
endif

SUBDIRS = src tests $(java_dirs)

# add doxygen support to the makefile
include $(top_srcdir)/aminclude.am

# add doxygen configuration to the distribution
EXTRA_DIST = doxygen.cfg

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
	tar -xvf $(dist_src) -C $(dpkg_dir)
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
			--scripts $(abs_top_srcdir)/osx \
			$(abs_top_builddir)/$(distdir).pkg \
		&& popd \
		&& rm -rf $(distdir)

clean-local:
	-rm -f $(abs_top_builddir)/$(distdir).pkg
endif

# Man pages
dist_man1_MANS = $(wildcard $(srcdir)/man/*.1)
