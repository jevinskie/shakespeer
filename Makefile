default: all

SUBDIRS = splib spclient sphubd cli gui

TOP=.
include common.mk

all-local: config.mk ${EXTERN_DEPENDS}
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

