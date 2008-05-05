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

release:
	mkdir -p release-build
	cd release-build && \
	if test -d shakespeer/_darcs; then \
	  cd shakespeer && \
	  darcs pull -a -v http://darcs.bzero.se/shakespeer ; \
	else \
	  darcs get --partial -v http://darcs.bzero.se/shakespeer && \
	  cd shakespeer ; \
	fi && $(MAKE) all BUILD_PROFILE=release

dmg: release
	cd release-build/shakespeer && /bin/sh support/mkdmg "$(VERSION)" . ../..

dist:
	darcs dist -d ${PACKAGE}-${VERSION}

version.h: version.mk
	echo '#ifndef _version_h_' > version.h
	echo '#define _version_h_' >> version.h
	sed -n 's/\([^=]*\)=\(.*\)/#define \1 "\2"/p' version.mk >> version.h
	echo '#endif' >> version.h

