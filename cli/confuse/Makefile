all: libconfuse.a

CFLAGS+=-DPACKAGE_VERSION="\"2.5\"" -DPACKAGE_STRING="\"libConfuse 2.5\""
CFLAGS+=-DHAVE_STRING_H -DHAVE_STDLIB_H -DHAVE_UNISTD_H -DHAVE_STRDUP -DHAVE_STRCASECMP

OBJS=confuse.o lexer.o

libconfuse.a: ${OBJS}
	rm -f $@
	ar cru $@ $^
	ranlib $@

lexer.c: lexer.l
	lex -Pcfg_yy -o$@ $<

clean:
	rm -f ${OBJS} libconfuse.a lexer.c *~

celan: clean
