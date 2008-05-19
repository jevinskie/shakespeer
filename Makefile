default: all

SUBDIRS = splib spclient sphubd cli gui

TOP=.
include common.mk

all-local: config.mk version.h ${EXTERN_DEPENDS}
check-local:
clean-local:

config.mk: configure ${TOP}/support/configure.sub
	CFLAGS="$(EXTERN_CFLAGS)" sh ./configure

REPO		?= shakespeer
REPO_URL	?= http://shakespeer.googlecode.com/svn

ifneq ($(TAG),)
  RELEASE_DIR=release-build-$(TAG)
  DIST_VERSION:=$(VERSION)
else
  RELEASE_DIR=release-build-HEAD
  DIST_VERSION:=snapshot-$(shell date +"%Y%m%d")
endif

tag-release:
	$(MAKE) release TAG=$(VERSION)

release:
	mkdir -p $(RELEASE_DIR)
	cd $(RELEASE_DIR) && \
	if test -d $(REPO)/.svn; then \
	  cd $(REPO) && \
	  echo "updating sources..." && \
	  svn up; \
	else \
	  echo "getting sources..." && \
	  svn checkout http://shakespeer.googlecode.com/svn/tags/$(TAG) $(REPO) && \
	  cd $(REPO) ; \
	fi && $(MAKE) all BUILD_PROFILE=release && $(MAKE) check

release-package:
	@echo Creating disk image...
	cd $(RELEASE_DIR)/$(REPO) && \
		/bin/sh support/mkdmg "$(DIST_VERSION)" . ../.. $(REPO)
	@echo Creating source tarball...
	svn export --force $(RELEASE_DIR)/$(REPO) $(PACKAGE)-$(DIST_VERSION) && \
	tar -czf $(PACKAGE)-$(DIST_VERSION).tar.gz $(PACKAGE)-$(DIST_VERSION) && \
	rm -rf $(PACKAGE)-$(DIST_VERSION)

tag-dmg: tag-release release-package

dmg: release release-package

snapshot:
	$(MAKE) release REPO_URL="`pwd`"
	
snapshot-package: snapshot
	$(MAKE) release-package REPO_URL="`pwd`"

dist:
	svn export --force $(PACKAGE)-$(DIST_VERSION) && \
	tar -czf $(PACKAGE)-$(DIST_VERSION).tar.gz $(PACKAGE)-$(DIST_VERSION) && \
	rm -rf $(PACKAGE)-$(DIST_VERSION)

version.h: version.mk
	echo '#ifndef _version_h_' > version.h
	echo '#define _version_h_' >> version.h
	sed -n 's/\([^=]*\)=\(.*\)/#define \1 "\2"/p' version.mk >> version.h
	echo '#endif' >> version.h
