# Copyright (c) 2013-15, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

DIR = @abs_top_srcdir@/prof
dist_bin_SCRIPTS = souffle-profile 
nodist_bin_SCRIPTS = build/libs/souffleprof.jar
JSRC = $(wildcard src/com/oracle/souffleprof/*.java)
EXTRA_DIST = $(souffleprof_src) 

build/libs/souffleprof.jar: $(JSRC)
	$(DIR)/gradlew --project-dir=$(DIR) -PbuildDir=@abs_top_builddir@/prof/build

clean-local:
	$(DIR)/gradlew clean --project-dir=$(DIR) -PbuildDir=@abs_top_builddir@/prof/build
