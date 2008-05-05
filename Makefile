default: all

SUBDIRS = splib spclient sphubd cli gui

TOP=.
include common.mk

all-local: config.mk version.h ${EXTERN_DEPENDS}
check-local:
clean-local:

config.mk: configure ${TOP}/support/configure.sub
	CFLAGS="$(UB_CFLAGS)" sh ./configure

config-osx:
	cp -f config-osx.mk config.mk

REPO		?= shakespeer
REPO_URL	?= http://darcs.bzero.se/$(REPO)

ifneq ($(TAG),)
TAG_OPTION=--tag=$(TAG)
RELEASE_DIR=release-build-$(TAG)
DIST_VERSION:=$(VERSION)
else
TAG_OPTION=
RELEASE_DIR=release-build-HEAD
DIST_VERSION:=snapshot-$(shell date +"%Y%m%d")
endif

tag-release:
	$(MAKE) release TAG=$(VERSION)

release:
	mkdir -p $(RELEASE_DIR)
	cd $(RELEASE_DIR) && \
	if test -d $(REPO)/_darcs; then \
	  cd $(REPO) && \
	  echo "updating sources..." && \
	  darcs pull -a -v $(TAG_OPTION) $(REPO_URL); \
	else \
	  echo "getting sources..." && \
	  darcs get --partial -v $(TAG_OPTION) $(REPO_URL) && \
	  cd $(REPO) ; \
	fi && $(MAKE) all BUILD_PROFILE=release && $(MAKE) check

release-package:
	@echo Creating disk image...
	cd $(RELEASE_DIR)/$(REPO) && \
		/bin/sh support/mkdmg "$(DIST_VERSION)" . ../.. $(REPO)
	@echo Creating source tarball...
	darcs dist --repodir=$(RELEASE_DIR)/$(REPO) -d $(PACKAGE)-$(DIST_VERSION) && \
	mv $(RELEASE_DIR)/$(REPO)/$(PACKAGE)-$(DIST_VERSION).tar.gz .

tag-dmg: tag-release release-package

dmg: release release-package

dist:
	darcs dist -d $(PACKAGE)-$(DIST_VERSION)

version.h: version.mk
	echo '#ifndef _version_h_' > version.h
	echo '#define _version_h_' >> version.h
	sed -n 's/\([^=]*\)=\(.*\)/#define \1 "\2"/p' version.mk >> version.h
	echo '#endif' >> version.h

