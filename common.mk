# common makefile for all subdirectories
# must define TOP before including this file, for example:
# TOP = ..
# include $(TOP)/common.mk

default: all

-include $(TOP)/config.mk

# Force building local Berkeley DB
HAS_BDB=no

include $(TOP)/version.mk
include $(TOP)/extern.mk

# set to yes if you want to build the command line interface
WANT_CLI=no

CFLAGS+=-g -O2 -Wall -Werror -DVERSION=\"$(VERSION)\" -DPACKAGE=\"$(PACKAGE)\"
CFLAGS+=-I$(TOP)/splib -I${TOP}/spclient

os := $(shell uname)

ifeq ($(os),Linux)
# Required for large file support on Linux
CFLAGS+=-D_FILE_OFFSET_BITS=64
endif

# Disable coredumps for public releases
ifneq ($(BUILD_PROFILE),release)
CFLAGS+=-DCOREDUMPS_ENABLED=1
endif

# Build a Universal Binary on Mac OS X
ifeq ($(os),Darwin)
ifeq ($(BUILD_PROFILE),release)
UB_CFLAGS=-isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch i386 -arch ppc
UB_LDFLAGS=-Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386
endif
endif

# Berkeley DB on Linux needs pthread
# We also need to link with -lresolv
ifeq ($)os),Linux)
LIBS+=-lpthread -lresolv
endif

# search for xcodebuild in path
pathsearch = $(firstword $(wildcard $(addsuffix /$(1),$(subst :, ,$(PATH)))))
XCODE := $(call pathsearch,xcodebuild)

# automatic dependency files go into this directory
DEPDIR = .deps
df = $(DEPDIR)/$(*F)

# automatic dependency generation from http://make.paulandlesley.org/autodep.html
# depends on gcc features (the -MD option)

COMPILE = \
	mkdir -p $(DEPDIR); \
	CMD="${CC} -Wp,-MD,$(df).d -c -o $@ $< ${CFLAGS} ${UB_CFLAGS}"; $$CMD || \
		{ echo "command was: $$CMD"; false; } && \
	cp $(df).d $(df).P && \
	sed -e 's/\#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $(df).d >> $(df).P && \
	rm -f $(df).d

%.o: %.c
	@echo "compiling $<"
	@$(COMPILE)

%.o: %.m
	@echo "compiling $<"
	@$(COMPILE)

%_test.o: %.c
	@echo "compiling tests in $<"
	@$(COMPILE)

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
clean: clean-local clean-deps
celan: clean
check: check-local

clean-deps:
	rm -rf $(DEPDIR)

check-local: $(check_PROGRAMS)
	@for test in $(TESTS); do \
		chmod 0755 ./$$test && \
		echo "=====[ running $$test ]=====" && \
		if ./$$test ; then \
			echo "PASSED: $$test"; \
		else \
			echo "FAILED: $$test"; exit 1; \
		fi; \
	done

OBJS=$(SOURCES:.c=.o)
ifeq ($(os),Darwin)
ALLSOURCES := $(wildcard *.[cm])
else
ALLSOURCES := $(wildcard *.c)
endif

-include $(wildcard *.c:.c=.P) $(wildcard *.m:.m=.P)

