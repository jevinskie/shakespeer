
# 1 = name, 2 = version, 3 = url, 4 = md5
define download
	@echo "downloading $(3)..."
	@cp ../${1}-${2}.tar.gz . || \
	cp ../../${1}-${2}.tar.gz . || \
	cp ../../../${1}-${2}.tar.gz . || \
	curl ${3} -o ${1}-${2}.tar.gz || \
	wget ${3} || \
	fetch ${3} || \
	ftp ${3}
	@echo "verifying checksum..."
	@md5=`(md5 ${1}-${2}.tar.gz || \
	    md5sum ${1}-${2}.tar.gz) 2>/dev/null | \
	    sed 's/.*\([0-9a-z]\{32\}\).*/\1/'` && \
	test "$$md5" = "${4}" || \
	{ echo "Wrong MD5 checksum on downloaded file."; false; }
endef

# 1 = name, 2 = version
define unpack
	rm -rf ${1}-install ${1}-build-${2}
	mkdir ${1}-build-${2}
	mkdir ${1}-install
	cd ${1}-build-${2} && \
	{ \
          tar zxf ../${1}-${2}.tar.gz || \
	  (gzcat ../${1}-${2}.tar.gz | tar xf - ); \
	}
endef

EVENT_VER=1.2a
EVENT_URL=http://monkey.org/~provos/libevent-${EVENT_VER}.tar.gz
EVENT_MD5=bb0b731b0c1bfa7267b8cd6192982d68
libevent-${EVENT_VER}.tar.gz:
	$(call download,libevent,${EVENT_VER},${EVENT_URL},${EVENT_MD5})
libevent: libevent-build-${EVENT_VER}/stamp
libevent-build-${EVENT_VER}/stamp: libevent-${EVENT_VER}.tar.gz
	$(call unpack,libevent,${EVENT_VER})
	cwd=`pwd` && cd libevent-build-${EVENT_VER}/libevent-${EVENT_VER} && \
	    CFLAGS="${UB_CFLAGS}" \
	    ./configure --disable-shared \
	                --enable-static \
	                --prefix=$$cwd/libevent-install && \
	    ${MAKE} install
	touch libevent-build-${EVENT_VER}/stamp

EXPAT_VER=2.0.0
EXPAT_URL=http://switch.dl.sourceforge.net/sourceforge/expat/expat-${EXPAT_VER}.tar.gz
EXPAT_MD5=d945df7f1c0868c5c73cf66ba9596f3f
expat-${EXPAT_VER}.tar.gz:
	$(call download,expat,${EXPAT_VER},${EXPAT_URL},${EXPAT_MD5})
expat: expat-build-${EXPAT_VER}/stamp
expat-build-${EXPAT_VER}/stamp: expat-${EXPAT_VER}.tar.gz
	$(call unpack,expat,${EXPAT_VER})
	cwd=`pwd` && cd expat-build-${EXPAT_VER}/expat-${EXPAT_VER} && \
	    CFLAGS="${UB_CFLAGS}" \
	    ./configure --disable-shared \
	                --enable-static \
	                --prefix=$$cwd/expat-install && \
	    ${MAKE} install
	touch expat-build-${EXPAT_VER}/stamp

ICONV_VER=1.9.1
ICONV_URL=http://ftp.gnu.org/pub/gnu/libiconv/libiconv-${ICONV_VER}.tar.gz
ICONV_MD5=0c99a05e0c3c153bac1c960f78711155
libiconv-${ICONV_VER}.tar.gz:
	$(call download,libiconv,${ICONV_VER},${ICONV_URL},${ICONV_MD5})
libiconv: libiconv-build-${ICONV_VER}/stamp
libiconv-build-${ICONV_VER}/stamp: libiconv-${ICONV_VER}.tar.gz
	$(call unpack,libiconv,${ICONV_VER})
	cwd=`pwd` && cd libiconv-build-${ICONV_VER}/libiconv-${ICONV_VER} && \
	    CFLAGS="${UB_CFLAGS}" \
	    ./configure --disable-shared \
	                --enable-static \
	                --prefix=$$cwd/libiconv-install && \
	    ${MAKE} install
	touch libiconv-build-${ICONV_VER}/stamp

BZ2_VER=1.0.1
BZ2_URL=http://www.digistar.com/bzip2/v100/bzip2-${BZ2_VER}.tar.gz
BZ2_MD5=770135dc94369cb3eb6013ed505c8dc5
bzip2-${BZ2_VER}.tar.gz:
	$(call download,bzip2,${BZ2_VER},${BZ2_URL},${BZ2_MD5})
bzip2: bzip2-build-${BZ2_VER}/stamp
bzip2-build-${BZ2_VER}/stamp: bzip2-${BZ2_VER}.tar.gz
	$(call unpack,bzip2,${BZ2_VER})
	cwd=`pwd` && cd bzip2-build-${BZ2_VER}/bzip2-${BZ2_VER} && \
	    CFLAGS="${UB_CFLAGS}" \
	    ${MAKE} install PREFIX=$$cwd/bzip2-install
	touch bzip2-build-${BZ2_VER}/stamp

DB_VER=4.5.20.NC
DB_URL=http://download-uk.oracle.com/berkeley-db/db-${DB_VER}.tar.gz
DB_MD5=1bfa6256f8d546b97bef1f448ab09875
db-${DB_VER}.tar.gz:
	$(call download,db,${DB_VER},${DB_URL},${DB_MD5})
db: db-build-${DB_VER}/stamp
db-build-${DB_VER}/stamp: db-${DB_VER}.tar.gz
	$(call unpack,db,${DB_VER})
	cwd=`pwd` && cd db-build-${DB_VER}/db-${DB_VER}/build_unix && \
	    CFLAGS="${UB_CFLAGS}" \
	    ../dist/configure --disable-shared \
	                      --enable-static \
			      --disable-queue \
			      --disable-verify \
			      --enable-statistics \
			      --disable-replication \
	                      --disable-cryptography \
	                      --prefix=$$cwd/db-install && \
	    ${MAKE} install
	touch db-build-${DB_VER}/stamp

ifneq (${HAS_BDB},yes)
EXTERN_DEPENDS+=db
BDB_CFLAGS=-I${TOP}/db-install/include
BDB_LDFLAGS=-L${TOP}/db-install/lib
BDB_LIBS=-ldb-4.5
endif

ifneq (${HAS_BZ2},yes)
EXTERN_DEPENDS+=bzip2
BZ2_CFLAGS=-I${TOP}/bzip2-install/include
BZ2_LDFLAGS=-L${TOP}/bzip2-install/lib
BZ2_LIBS=-lbz2
endif

ifneq (${HAS_ICONV},yes)
EXTERN_DEPENDS+=libiconv
ICONV_CFLAGS=-I${TOP}/libiconv-install/include
ICONV_LDFLAGS=-L${TOP}/libiconv-install/lib
ICONV_LIBS=-liconv
endif

ifneq (${HAS_EXPAT},yes)
EXTERN_DEPENDS+=expat
EXPAT_CFLAGS=-I${TOP}/expat-install/include
EXPAT_LDFLAGS=-L${TOP}/expat-install/lib
EXPAT_LIBS=-lexpat
endif

ifneq (${HAS_LIBEVENT},yes)
EXTERN_DEPENDS+=libevent
LIBEVENT_CFLAGS=-I${TOP}/libevent-install/include
LIBEVENT_LDFLAGS=-L${TOP}/libevent-install/lib
LIBEVENT_LIBS=-levent
endif

CFLAGS+=${BZ2_CFLAGS}
CFLAGS+=${ICONV_CFLAGS}
CFLAGS+=${LIBEVENT_CFLAGS}
CFLAGS+=${EXPAT_CFLAGS}
CFLAGS+=${BDB_CFLAGS}

LIBS=-L${TOP}/spclient -lspclient -L${TOP}/splib -lsplib \
     ${ICONV_LDFLAGS} ${ICONV_LIBS} \
     ${BZ2_LDFLAGS} ${BZ2_LIBS} \
     ${EXPAT_LDFLAGS} ${EXPAT_LIBS} \
     ${LIBEVENT_LDFLAGS} ${LIBEVENT_LIBS} \
     ${BDB_LDFLAGS} ${BDB_LIBS}

