# Souffle - A Datalog Compiler
# Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

SUBDIRS = interface/functors

EXTRA_DIST =  $(srcdir)/*.at package.m4 $(TESTSUITE) atlocal.in $(srcdir)/swig $(srcdir)/evaluation $(srcdir)/semantic $(srcdir)/syntactic $(srcdir)/interface $(srcdir)/profile $(srcdir)/provenance

package.m4: $(top_srcdir)/configure.ac
	@{                                      \
	  echo '# Signature of the current package.'; \
	  echo 'm4_define([AT_PACKAGE_NAME],      [@PACKAGE_NAME@])'; \
	  echo 'm4_define([AT_PACKAGE_TARNAME],   [@PACKAGE_TARNAME@])'; \
	  echo 'm4_define([AT_PACKAGE_VERSION],   [@PACKAGE_VERSION@])'; \
	  echo 'm4_define([AT_PACKAGE_STRING],    [@PACKAGE_STRING@])'; \
	  echo 'm4_define([AT_PACKAGE_BUGREPORT], [@PACKAGE_BUGREPORT@])'; \
	} >package.m4

TESTSUITE = $(srcdir)/testsuite

check-local: atconfig atlocal $(TESTSUITE) python-tests
	test -e $(abs_top_builddir)/include || ln -s $(abs_top_srcdir)/src/include $(abs_top_builddir)/include
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS)

python-tests:
	$(NOX) -f $(top_srcdir)/noxfile.py --reuse-existing-virtualenvs -s tests -- --souffle-path=$(abs_top_srcdir)/src/souffle

installcheck-local: atconfig atlocal $(TESTSUITE)
	$(NOX) -f $(top_srcdir)/noxfile.py -s tests -- --souffle-path=$(abs_top_srcdir)/src/souffle
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' \
	$(TESTSUITEFLAGS)

clean-local:
	test ! -f '$(TESTSUITE)' ||  $(SHELL) '$(TESTSUITE)' --clean
	rm -rf $(abs_top_builddir)/include
	rm -f testsuite
	rm -f package.m4

distclean-local:
	rm -f atconfig

AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(srcdir)/swig.at $(srcdir)/testsuite.at $(srcdir)/syntactic.at $(srcdir)/semantic.at $(srcdir)/evaluation.at $(srcdir)/interface.at $(srcdir)/profile.at $(srcdir)/provenance.at package.m4
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@
