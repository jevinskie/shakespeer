TOP=..
include ${TOP}/common.mk

ifeq (${WANT_CLI},yes)
bin_PROGRAMS=shakespeer
endif

CFLAGS+=-Ilibtecla/tecla-local-install/include -Iconfuse
LDFLAGS+=-Llibtecla/tecla-local-install/lib -Lconfuse
LIBS+=-lconfuse -ltecla -lcurses

shakespeer_SOURCES = cmd.c ctx-filelist.c ctx-hub.c \
		     ctx-hublist.c ctx-main.c \
		     ctx-queue.c shakespeer.c cfg.c

all-local: .tecla.build confuse/libconfuse.a ${bin_PROGRAMS}

confuse/libconfuse.a:
	${MAKE} -C confuse libconfuse.a

LIBTECLA_VERSION=1.6.1
LIBTECLA_URL=http://www.astro.caltech.edu/~mcs/tecla/libtecla-${LIBTECLA_VERSION}.tar.gz 
libtecla-${LIBTECLA_VERSION}.tar.gz:
	cp ../libtecla-${LIBTECLA_VERSION}.tar.gz . || \
	cp ../../libtecla-${LIBTECLA_VERSION}.tar.gz . || \
	cp ../../../libtecla-${LIBTECLA_VERSION}.tar.gz . || \
	curl ${LIBTECLA_URL} -o libtecla-${LIBTECLA_VERSION}.tar.gz || \
	wget ${LIBTECLA_URL} || \
	fetch ${LIBTECLA_URL} || \
	ftp ${LIBTECLA_URL}

.tecla.build: tecla-display_message.patch libtecla-${LIBTECLA_VERSION}.tar.gz
	rm -rf libtecla && tar zxvf libtecla-${LIBTECLA_VERSION}.tar.gz && \
	cd libtecla && patch -p1 < ../tecla-display_message.patch && \
	./configure --without-man-pages \
		    --disable-dependency-tracking \
		    --prefix=`pwd`/tecla-local-install && \
	 $(MAKE) install_lib install_inc && cd .. && \
	touch .tecla.build && \
       	ranlib libtecla/tecla-local-install/lib/libtecla.a

noinst_HEADERS=shakespeer.h cfg.h
AM_CFLAGS=$(CLI_CFLAGS) -Ilibtecla/tecla-local-install/include \
	  ${EXPAT_CFLAGS} ${LIBEVENT_CFLAGS} \
	  ${ICONV_CFLAGS} ${BZ2_CFLAGS}
AM_LDFLAGS=${EXPAT_LDFLAGS} ${LIBEVENT_LDFLAGS} \
	   ${ICONV_LDFLAGS} ${CURL_LDFLAGS} ${BZ2_LDFLAGS}

AM_LDFLAGS+=-bind_at_load

shakespeer_LDFLAGS=-L/usr/lib ${EXPAT_LDFLAGS}
shakespeer_LDADD = $(top_builddir)/spclient/libspclient.a \
		   $(top_builddir)/splib/libsplib.a $(CLI_LIBS) \
		   libtecla/tecla-local-install/lib/libtecla.a -lcurses \
		   ${EXPAT_LIBS} ${LIBEVENT_LIBS} \
		   ${CURL_LIBS} ${ICONV_LIBS} ${BZ2_LIBS}

shakespeer_OBJS=${shakespeer_SOURCES:.c=.o}
shakespeer: ${shakespeer_OBJS} \
	    ${TOP}/spclient/libspclient.a \
	    ${TOP}/splib/libsplib.a
	${CC} -o $@ ${CFLAGS} $^ ${LDFLAGS} ${LIBS}


INCLUDES = -I$(top_srcdir)/splib -I$(top_srcdir)/spclient
EXTRA_DIST=tecla-display_message.patch libtecla-1.6.1.tar.gz
CLEANFILES=*~
DISTCLEANFILES=.tecla.build

clean-local:
	rm -f shakespeer *.o
	-${MAKE} -C libtecla clean

distclean: clean
	rm -rf .tecla.build libtecla


