/*
 * Copyright 2006 Martin Hedenfalk <martin.hedenfalk@gmail.com>
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

#include <string.h>

#include "dstring.h"

#define QUOTE_DEFAULT_CHARS " \t\n[](){}!"

/*
 * Quotes <string> using backslashes. <quotechars> is a string with characters
 * that need to be quoted.  Returns a new string (should be freed by the
 * caller). If <quotechars> is NULL, a default set of characters are used.
 * The quote character itself (backslash) is always quoted.
 */
char *str_quote_backslash(const char *string, const char *quotechars)
{
    if(string == NULL)
        return NULL;

    if(quotechars == NULL)
        quotechars = QUOTE_DEFAULT_CHARS;

    dstring_t *ds = dstring_new(NULL);
    const char *p;
    for(p = string; *p; p++)
    {
        char c = *p;
        if(c == '\\' || strchr(quotechars, c) != NULL)
        {
            dstring_append_char(ds, '\\');
        }
        dstring_append_char(ds, c);
    }

    char *ret = ds->string;
    dstring_free(ds, 0);

    return ret;
}

char *str_unquote(const char *str, int len)
{
    if(len <= 0)
        len = strlen(str);

    char *rs = malloc(len + 1);
    const char *pstr = str;
    const char *str_end = str + len;
    char *prs = rs;
    int state = 0;

    while(*pstr && pstr < str_end)
    {
        switch(state)
        {
            case 0:
                if(*pstr == '\'')
                    state = 1;
                else if(*pstr == '\"')
                    state = 2;
                else
                {
                    if(*pstr == '\\' && *++pstr)
                        ;
                    *prs++ = *pstr;
                }
                break;
            case 1:
                if(*pstr == '\'')
                    state = 0;
                else
                    *prs++ = *pstr;
                break;
            case 2:
                if(*pstr == '\"')
                    state = 0;
                else
                {
                    if(*pstr == '\\' && *++pstr)
                        ;
                    *prs++ = *pstr;
                }
                break;
        }
        pstr++;
    }

    *prs = 0;

    return rs;
}

#ifdef TEST

#include <stdio.h>

#define fail_unless(test) \
    do { if(!(test)) { \
        fprintf(stderr, \
                "----------------------------------------------\n" \
                "%s:%d: test FAILED: %s\n" \
                "----------------------------------------------\n", \
                __FILE__, __LINE__, #test); \
        exit(1); \
    } } while(0)

int main(void)
{
    char *u = str_unquote("a\\ quoted\\ string", 9);
    fail_unless(u);
    fail_unless(strcmp(u, "a quoted") == 0);
    free(u);

    u = str_unquote("a\\ quoted\\ string", -1);
    fail_unless(u);
    fail_unless(strcmp(u, "a quoted string") == 0);
    free(u);

    u = str_unquote("a 'quoted' \"string\"", -1);
    fail_unless(u);
    fail_unless(strcmp(u, "a quoted string") == 0);
    free(u);

    u = str_quote_backslash("an unquoted   string", " q");
    fail_unless(u);
    fail_unless(strcmp(u, "an\\ un\\quoted\\ \\ \\ string") == 0);
    free(u);

    return 0;
}

#endif

