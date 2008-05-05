default: all

-include ${TOP}/config.mk

# Force building local Berkeley DB
HAS_BDB=no

include ${TOP}/version.mk
include ${TOP}/extern.mk

# set to yes if you want to build the command line interface
WANT_CLI=yes

CFLAGS+=-g -O2 -Wall -Werror -DVERSION=\"${VERSION}\" -DPACKAGE=\"${PACKAGE}\"
CFLAGS+=-I${TOP}/splib -I${TOP}/spclient

os := $(shell uname)

ifeq (${os},Linux)
# Required for large file support on Linux
CFLAGS+=-D_FILE_OFFSET_BITS=64
endif

# Disable for public releases
CFLAGS+=-DCOREDUMPS_ENABLED=1

# Build a Universal Binary on Mac OS X
ifeq (${os},Darwin)
UB_CFLAGS=-isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch i386 -arch ppc
UB_LDFLAGS=-Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386
endif

# Berkeley DB on Linux needs pthread
# We also need to link with -lresolv
ifeq (${os},Linux)
LIBS+=-lpthread -lresolv
endif

# search for xcodebuild in path
pathsearch = $(firstword $(wildcard $(addsuffix /$(1),$(subst :, ,$(PATH)))))
XCODE := $(call pathsearch,xcodebuild)

%.o: %.c
	@echo "compiling $<"
	@CMD="${CC} -c -o $@ $< ${CFLAGS} ${UB_CFLAGS}"; $$CMD || \
		{ echo "command was: $$CMD"; false; }

%.o: %.m
	@echo "compiling $<"
	@CMD="${CC} -c -o $@ $< ${CFLAGS} ${UB_CFLAGS}"; $$CMD || \
		{ echo "command was: $$CMD"; false; }

%_test.o: %.c
	@echo "compiling tests in $<"
	@CMD="${CC} -c -o $@ $< -DTEST ${CFLAGS} ${UB_CFLAGS}"; $$CMD || \
		{ echo "command was: $$CMD"; false; }

define LINK
	@echo "linking $@"
	@CMD="${CC} -o $@ $^ ${UB_CFLAGS} ${LDFLAGS} ${LIBS}"; $$CMD || \
	  { echo "command was: $$CMD"; false; }
endef

all clean check: 
	@for subdir in $(SUBDIRS); do \
	  echo "making $@ in $$subdir"; \
	  $(MAKE) -C $$subdir $@ || exit 1; \
	done
all: all-local
clean: clean-local
celan: clean
check: check-local

check-local: ${check_PROGRAMS}
	@for test in ${TESTS}; do \
		chmod 0755 ./$$test && \
		if ./$$test ; then \
			echo "PASSED: $$test"; \
		else \
			echo "FAILED: $$test"; exit 1; \
		fi; \
	done

OBJS=${SOURCES:.c=.o}
ALLSOURCES := $(wildcard *.[cm])

depend:
	@echo Generating dependencies in $(shell pwd)...
	@rm -f .deps
	@touch .deps
	@if test -n "${ALLSOURCES}"; then \
       	    ${CC} -MM ${ALLSOURCES} ${CFLAGS} > depend.mk; \
	else \
	    true; \
	fi
	@$(foreach dir,${SUBDIRS},${MAKE} -C ${dir} $@;)

-include depend.mk

