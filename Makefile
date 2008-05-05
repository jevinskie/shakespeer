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

REPO_URL=http://darcs.bzero.se/shakespeer

ifneq ($(TAG),)
TAG_OPTION=--tag=$(TAG)
RELEASE_DIR=release-build-$(TAG)
else
TAG_OPTION=
RELEASE_DIR=release-build-HEAD
endif

tag-release:
	$(MAKE) release TAG=$(VERSION)

release:
	mkdir -p $(RELEASE_DIR)
	cd $(RELEASE_DIR) && \
	if test -d shakespeer/_darcs; then \
	  cd shakespeer && \
	  darcs pull -a -v $(TAG_OPTION) $(REPO_URL); \
	else \
	  darcs get --partial -v $(TAG_OPTION) $(REPO_URL) && \
	  cd shakespeer ; \
	fi && $(MAKE) all BUILD_PROFILE=release

tag-dmg: tag-release
	cd $(RELEASE_DIR)/shakespeer && \
		/bin/sh support/mkdmg "$(VERSION)" . ../..

dmg: release
	cd $(RELEASE_DIR)/shakespeer && \
		/bin/sh support/mkdmg "$(VERSION)" . ../..

dist:
	darcs dist -d $(PACKAGE)-$(VERSION)

version.h: version.mk
	echo '#ifndef _version_h_' > version.h
	echo '#define _version_h_' >> version.h
	sed -n 's/\([^=]*\)=\(.*\)/#define \1 "\2"/p' version.mk >> version.h
	echo '#endif' >> version.h

