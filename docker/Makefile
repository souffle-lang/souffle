SOUFFLE_GITHUB_USER="souffle-lang"
SOUFFLE_DOCKERHUB_USER="souffle"
SOUFFLE_GIT_BRANCH="master"
SOUFFLE_GIT_REVISION="HEAD"
SOUFFLE_CONFIGURE_OPTIONS=""
SOUFFLE_CATEGORY ?= "Syntactic,Semantic,FastEvaluation,Profiling,Interface"
# centos7 and oracle linux do not fully support c++11. Currently this only
# affects provenance.
#SOUFFLE_CATEGORY ?= "Syntactic,Semantic,FastEvaluation,Profiling,Provenance,Interface"
SOUFFLE_CONFS ?= "-c -j4 -ra.html,-i -j4 -ra.html"
SOUFFLE_DOCKERFILES ?= "$(shell find . -name Dockerfile)"

.PHONY: all check deploy

all:
	set -ue pipefail; \
		SOUFFLE_DOCKERFILES=$${SOUFFLE_DOCKERFILES:-$(SOUFFLE_DOCKERFILES)}; \
		SOUFFLE_GITHUB_USER=$${SOUFFLE_GITHUB_USER:-$(SOUFFLE_GITHUB_USER)}; \
		SOUFFLE_GIT_BRANCH=$${SOUFFLE_GIT_BRANCH:-$(SOUFFLE_GIT_BRANCH)}; \
		SOUFFLE_GIT_REVISION=$${SOUFFLE_GIT_REVISION:-$(SOUFFLE_GIT_REVISION)}; \
		SOUFFLE_CONFIGURE_OPTIONS=$${SOUFFLE_CONFIGURE_OPTIONS:-$(SOUFFLE_CONFIGURE_OPTIONS)}; \
		SOUFFLE_DOCKERHUB_USER=$${SOUFFLE_DOCKERHUB_USER:-$(SOUFFLE_DOCKERHUB_USER)}; \
		for DOCKERFILE in $${SOUFFLE_DOCKERFILES}; do \
		docker build \
		--rm \
		--tag $${SOUFFLE_DOCKERHUB_USER}/$$(dirname $${DOCKERFILE} | sed 's/^\.\///' | sed 's/\//:/g') \
		--file $${DOCKERFILE} \
		--build-arg SOUFFLE_GITHUB_USER=$${SOUFFLE_GITHUB_USER} \
		--build-arg SOUFFLE_GIT_BRANCH=$${SOUFFLE_GIT_BRANCH} \
		--build-arg SOUFFLE_GIT_REVISION=$${SOUFFLE_GIT_REVISION} \
		--build-arg SOUFFLE_CONFIGURE_OPTIONS=$${SOUFFLE_CONFIGURE_OPTIONS} \
		.; \
		done;

check: all
	set -ue pipefail; \
		SOUFFLE_DOCKERFILES=$${SOUFFLE_DOCKERFILES:-$(SOUFFLE_DOCKERFILES)}; \
		SOUFFLE_CATEGORY=$${SOUFFLE_CATEGORY:-$(SOUFFLE_CATEGORY)}; \
		SOUFFLE_CONFS=$${SOUFFLE_CONFS:-$(SOUFFLE_CONFS)}; \
		SOUFFLE_DOCKERHUB_USER=$${SOUFFLE_DOCKERHUB_USER:-$(SOUFFLE_DOCKERHUB_USER)}; \
		for DOCKERFILE in $${SOUFFLE_DOCKERFILES}; do \
		docker run \
		--interactive \
		--tty \
		--env "SOUFFLE_CATEGORY=$${SOUFFLE_CATEGORY}" \
		--env "SOUFFLE_CONFS=$${SOUFFLE_CONFS}" \
		${SOUFFLE_DOCKERHUB_USER}/$$(dirname $${DOCKERFILE} | sed 's/^\.\///' | sed 's/\//:/g') \
		/bin/bash -c "TESTSUITEFLAGS=-j2 make check -j2"; \
		done;

deploy: all
	set -ue pipefail; \
		SOUFFLE_DOCKERFILES=$${SOUFFLE_DOCKERFILES:-$(SOUFFLE_DOCKERFILES)}; \
		SOUFFLE_DOCKERHUB_USER=$${SOUFFLE_DOCKERHUB_USER:-$(SOUFFLE_DOCKERHUB_USER)}; \
		for DOCKERFILE in $${SOUFFLE_DOCKERFILES}; do \
		docker push $${SOUFFLE_DOCKERHUB_USER}/$$(dirname $${DOCKERFILE} | sed 's/^\.\///' | sed 's/\//:/g'); \
		done;
