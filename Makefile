default: all

SUBDIRS = splib spclient sphubd cli gui

TOP=.
include common.mk

all-local: config.mk version.h ${EXTERN_DEPENDS}
check-local:
clean-local:

config.mk: configure ${TOP}/support/configure.sub
	sh ./configure

config-osx:
	cp -f config-osx.mk config.mk

dmg: all
	/bin/sh ${TOP}/support/mkdmg

dist:
	darcs dist -d ${PACKAGE}-${VERSION}

version.h: version.mk
	echo '#ifndef _version_h_' > version.h
	echo '#define _version_h_' >> version.h
	sed -n 's/\([^=]*\)=\(.*\)/#define \1 "\2"/p' version.mk >> version.h
	echo '#endif' >> version.h

