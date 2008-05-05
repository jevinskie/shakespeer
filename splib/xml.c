/*
 * Copyright (c) 2006 Martin Hedenfalk <martin.hedenfalk@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "xml.h"
#include "nfkc.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <iconv.h>

/* So far unused */
int xml_iconv_convert(void *data, const char *s)
{
    iconv_t cd = (iconv_t *)data;

    int uc;

    const char *inp = s;
    char *outp = (char *)&uc;

    size_t insize = 1;
    size_t outsize = 2;

    size_t rc = iconv(cd, &inp, &insize, &outp, &outsize);
    if(rc == (size_t)-1)
        return -1;
    return uc;
}

void xml_iconv_release(void *data)
{
    iconv_t cd = (iconv_t *)data;

    iconv_close(cd);
}

int xml_unknown_encoding_handler(void *encodingHandlerData,
                                 const XML_Char *name,
                                 XML_Encoding *info)
{
    iconv_t cd;

#ifdef __BIG_ENDIAN__
    cd = iconv_open("UCS-2BE", name);
#else
    cd = iconv_open("UCS-2LE", name);
#endif
    if(cd == (iconv_t)-1)
    {
        return 0; /* can't handle this encoding */
    }
    
    int i;
    for(i = 0; i < 256; i++)
    {
        char inbuf[4], outbuf[2];
        size_t insize = 1, outsize = 2;

        inbuf[0] = i;
        const char *inp = inbuf;
        char *outp = outbuf;

        size_t rc = iconv(cd, &inp, &insize, &outp, &outsize);
        if(rc == (size_t)-1)
        {
            switch(errno)
            {
                case EILSEQ:
                    /* inp points to the beginning of an invalid sequence */
                    info->map[i] = -1;
                    break;

                case EINVAL:
                    /* incomplete byte sequence encountered */
                    /* not implemented yet */
                    info->map[i] = -1;
                    break;

                case E2BIG:
                    /* expat can only handle 16-bit unicode values...? */
                    info->map[i] = -1;
                    break;
            }
        }
        else
        {
            info->map[i] = *((short *)outbuf);
        }
    }

    info->data = cd;
    info->convert = xml_iconv_convert;
    info->release = xml_iconv_release;

    return 1;
}

static void xml_decl_handler(void *user_data, const char *version,
        const char *encoding, int standalone)
{
    xml_ctx_t *ctx = user_data;

    if(encoding && ctx->encoding == NULL)
    {
        ctx->encoding = strdup(encoding);
    }
}

static void xml_open_func(void *data, const char *el, const char **attr)
{
    xml_ctx_t *ctx = data;

    if(ctx->open_func)
        ctx->open_func(ctx->user_data, el, attr);
}

static void xml_close_func(void *data, const char *el)
{
    xml_ctx_t *ctx = data;

    if(ctx->close_func)
        ctx->close_func(ctx->user_data, el);
}

xml_ctx_t *xml_init_fp(FILE *fp,
        xml_open_func_t open_func,
        xml_close_func_t close_func,
        void *user_data)
{
    struct xml_ctx *ctx = calloc(1, sizeof(struct xml_ctx));

    ctx->fp = fp;
    ctx->user_data = user_data;
    ctx->open_func = open_func;
    ctx->close_func = close_func;

    ctx->parser = XML_ParserCreate(NULL);
    XML_SetUserData(ctx->parser, ctx);
    XML_SetUnknownEncodingHandler(ctx->parser,
            xml_unknown_encoding_handler, NULL);
    XML_SetElementHandler(ctx->parser, xml_open_func, xml_close_func);
    XML_SetXmlDeclHandler(ctx->parser, xml_decl_handler);

    return ctx;
}

static char *xml_read_chunk(xml_ctx_t *ctx, size_t *chunk_size_ret)
{
    if(ctx->fp == NULL || chunk_size_ret == NULL)
        return NULL;

    static unsigned char chunk[8192];
    size_t read_size = fread(chunk, 1, sizeof(chunk), ctx->fp);

    *chunk_size_ret = read_size;

    if(ctx->encoding && strcasecmp(ctx->encoding, "WINDOWS-1252") == 0)
    {
        size_t i;
        for(i = 0; i < read_size; i++)
        {
            if(chunk[i] == 0x1E ||
               chunk[i] == 0x0E ||
               chunk[i] == 0x81 ||
               chunk[i] == 0x8D ||
               chunk[i] == 0x8F ||
               chunk[i] == 0x90 ||
               chunk[i] == 0x9D)
            {
                printf("replacing 0x%02X with '?'\n", chunk[i]);
                chunk[i] = '?';
            }
        }
    }

    return (char *)chunk;
}

int xml_parse_chunk(xml_ctx_t *ctx)
{
    size_t chunk_size = 0;
    char *chunk = xml_read_chunk(ctx, &chunk_size);

    if(chunk == NULL)
    {
        return 1;
    }

    if(XML_Parse(ctx->parser, chunk, chunk_size, chunk_size == 0)
            == XML_STATUS_ERROR)
    {
#ifdef TEST
        printf("Parse error %i at line %i, col %i, offset %u: %s\n",
                (int)XML_GetErrorCode(ctx->parser),
                (int)XML_GetCurrentLineNumber(ctx->parser),
                (int)XML_GetCurrentColumnNumber(ctx->parser),
                (unsigned)XML_GetCurrentByteIndex(ctx->parser),
                XML_ErrorString(XML_GetErrorCode(ctx->parser)));
        printf("offending character: 0x%02X\n", chunk[XML_GetCurrentByteIndex(ctx->parser) % 8192]);
#endif
        return -1;
    }

    return chunk_size == 0 ? 1 : 0;
}

void xml_ctx_free(xml_ctx_t *ctx)
{
    if(ctx)
    {
        XML_ParserFree(ctx->parser);
        free(ctx->encoding);
        free(ctx);
    }
}

#ifdef TEST

#include "xstr.h"

#define fail_unless(test) \
    do { if(!(test)) { \
        fprintf(stderr, \
                "----------------------------------------------\n" \
                "%s:%d: test FAILED: %s\n" \
                "----------------------------------------------\n", \
                __FILE__, __LINE__, #test); \
        exit(1); \
    } } while(0)

int got_root = 0;
int got_node = 0;
int got_foo = 0;
int ntags = 0;
char bar[32];

void open_tag(void *data, const char *el, const char **attr)
{
    fail_unless(data == (void *)0xDEADBEEF);
    fail_unless(el);
    fail_unless(attr);

    ++ntags;

    if(strcmp(el, "root") == 0)
    {
        got_root++;
        fail_unless(attr[0] == NULL);
    }
    else if(strcmp(el, "node") == 0)
    {
        got_node++;
        fail_unless(attr[0] == NULL);
    }
    else if(strcmp(el, "foo") == 0)
    {
        got_foo++;
        fail_unless(attr[0] && attr[1]);
        fail_unless(strcmp(attr[0], "bar") == 0);

        xstrlcpy(bar, attr[1], sizeof(bar));

        /* fail_unless(strcmp(attr[1], "baz") == 0); */
    }
    else
    {
        fail_unless(0);
    }
}

void close_tag(void *data, const char *el)
{
    fail_unless(data == (void *)0xDEADBEEF);
    fail_unless(el);

    if(got_foo)
    {
        fail_unless(strcmp(el, "foo") == 0);
        got_foo--;
    }
    else if(got_node)
    {
        fail_unless(strcmp(el, "node") == 0);
        got_node--;
    }
    else if(got_root)
    {
        fail_unless(strcmp(el, "root") == 0);
        got_root--;
    }
}

void test_encoding(const char *encoding, const char *bar_value, const char *expected_bar)
{
    printf("----- Testing encoding %s\n", encoding);
    
    FILE *fp = tmpfile();
    /* FILE *fp = fopen("/tmp/xml.tmp", "w+"); */
    fail_unless(fp);

    fail_unless(fprintf(fp,
                "<?xml version=\"1.0\" encoding=\"%s\"?>"
                "<root>"
                "  <node>"
                "    <foo bar=\"%s\"></foo>"
                "  </node>"
                "</root>", encoding, bar_value) > 0);
    rewind(fp);

    xml_ctx_t *ctx = xml_init_fp(fp, open_tag, close_tag, (void *)0xDEADBEEF);
    fail_unless(ctx);

    fail_unless(xml_parse_chunk(ctx) == 0);
    fail_unless(xml_parse_chunk(ctx) == 1);

    fail_unless(ntags == 3);
    fail_unless(strcmp(bar, expected_bar) == 0);

    xml_ctx_free(ctx);

    memset(bar, 0, sizeof(bar));

    fail_unless(fclose(fp) == 0);
}

int main(void)
{
    test_encoding("UTF-8",
            "[\xc3\xa5\xc3\xa4\xc3\xb6]-(a\xcc\x8a""a\xcc\x88""o\xcc\x88)",
            "[\xc3\xa5\xc3\xa4\xc3\xb6]-(a\xcc\x8a""a\xcc\x88""o\xcc\x88)");
    ntags = 0;
    test_encoding("ISO-8859-1", "[\xe5\xe4\xf6]-\x9F", "[\xc3\xa5\xc3\xa4\xc3\xb6]-\xc2\x9f");
    ntags = 0;
    test_encoding("WINDOWS-1252", "[\xe5\xe4\xf6]-\x80", "[\xc3\xa5\xc3\xa4\xc3\xb6]-\xE2\x82\xAC");
    ntags = 0;

    return 0;
}

#endif

