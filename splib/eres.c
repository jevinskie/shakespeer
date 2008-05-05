/*
 * Copyright (c) 2006 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*	$OpenBSD: gethostnamadr.c,v 1.68 2005/08/06 20:30:03 espie Exp $ */
/*-
 * Copyright (c) 1985, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#include <sys/types.h>
#include <netinet/in.h>
#define BIND_8_COMPAT
#include <arpa/nameser.h>
#include <resolv.h>
#include <sys/time.h>
#include <event.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "xstr.h"

#if !defined(IN6ADDRSZ)
# define IN6ADDRSZ 16
#endif

#define ERES_TIMEOUT 17 /* timeout in seconds */
#define MAXPACKET 1024

#define	MAXALIASES	35
static char *host_aliases[MAXALIASES];
static char hostbuf[BUFSIZ+1];

#define	MAXADDRS	35
static char *h_addr_ptrs[MAXADDRS + 1];

#define MULTI_PTRS_ARE_ALIASES 1	/* XXX - experimental */

static const char AskedForGot[] =
			  "gethostby*.getanswer: asked for \"%s\", got \"%s\"\n";

typedef union
{
	int32_t al;
	char ac;
} align;

typedef union {
	HEADER hdr;
	u_char buf[MAXPACKET];
} querybuf;

extern int res_opt(int, u_char *, int, int);
static void eres_event_handler(int fd, short why, void *arg);

typedef void (*eres_callback_t)(int error, struct hostent *host, int af, void *user_data);

struct eres_data
{
    char *name;
    int af;

    int fd;
    int query_id;
    int query_len;
    querybuf query;
    int ns_index;
    struct hostent host;

    struct event ev;
    eres_callback_t callback;
    void *user_data;
};

static void eres_free_data(struct eres_data *ed)
{
    if(ed)
    {
        if(event_initialized(&ed->ev))
        {
            event_del(&ed->ev);
        }
        free(ed->name);
        free(ed);
    }
}

static int
_hokchar(const char *p)
{
	char c;

	/*
	 * Many people do not obey RFC 822 and 1035.  The valid
	 * characters are a-z, A-Z, 0-9, '-' and . But the others
	 * tested for below can happen, and we must be more permissive
	 * than the resolver until those idiots clean up their act.
	 * We let '/' through, but not '..'
	 */
	while ((c = *p++)) {
		if (('a' <= c && c <= 'z') ||
		    ('A' <= c && c <= 'Z') ||
		    ('0' <= c && c <= '9'))
			continue;
		if (strchr("-_/", c))
			continue;
		if (c == '.' && *p != '.')
			continue;
		return 0;
	}
	return 1;
}

static struct hostent *
getanswer(struct eres_data *ed, 
        const querybuf *answer, int anslen, const char *qname, int qtype)
{
    const HEADER *hp;
    const u_char *cp, *eom;
    char tbuf[MAXDNAME];
    char *bp, **ap, **hap, *ep;
    int type, class, ancount, qdcount, n;
    int haveanswer, had_error, toobig = 0;
    const char *tname;

    tname = qname;
    ed->host.h_name = NULL;
    eom = answer->buf + anslen;
    /*
     * find first satisfactory answer
     */
    hp = &answer->hdr;
    ancount = ntohs(hp->ancount);
    qdcount = ntohs(hp->qdcount);
    bp = hostbuf;
    ep = hostbuf + sizeof hostbuf;
    cp = answer->buf + HFIXEDSZ;
    if (qdcount != 1) {
        h_errno = NO_RECOVERY;
        return (NULL);
    }
    n = dn_expand(answer->buf, eom, cp, bp, ep - bp);
    if ((n < 0) || !_hokchar(bp)) {
        h_errno = NO_RECOVERY;
        return (NULL);
    }
    cp += n + QFIXEDSZ;
    if (qtype == T_A || qtype == T_AAAA)
    {
        /* res_send() has already verified that the query name is the
         * same as the one we sent; this just gets the expanded name
         * (i.e., with the succeeding search-domain tacked on).
         */
        n = strlen(bp) + 1;		/* for the \0 */
        ed->host.h_name = bp;
        bp += n;
        /* The qname can be abbreviated, but h_name is now absolute. */
        qname = ed->host.h_name;
    }
    ap = host_aliases;
    *ap = NULL;
    ed->host.h_aliases = host_aliases;
    hap = h_addr_ptrs;
    *hap = NULL;
    ed->host.h_addr_list = h_addr_ptrs;
    haveanswer = 0;
    had_error = 0;
    while (ancount-- > 0 && cp < eom && !had_error)
    {
        n = dn_expand(answer->buf, eom, cp, bp, ep - bp);
        if ((n < 0) || !_hokchar(bp))
        {
            had_error++;
            continue;
        }
        cp += n;			/* name */
        if (cp >= eom)
            break;
        GETSHORT(type, cp);
        /* cp += INT16SZ;			|+ type +| */
        if (cp >= eom)
            break;
        GETSHORT(class, cp);
        /* cp += INT16SZ + INT32SZ;	|+ class, TTL +| */
        cp += INT32SZ;          	/* class, TTL */
        if (cp >= eom)
            break;
        GETSHORT(n, cp);
        /* cp += INT16SZ;			|+ len +| */
        if (cp >= eom)
            break;
        if (type == T_SIG)
        {
            /* XXX - ignore signatures as we don't use them yet */
            cp += n;
            continue;
        }
        if (class != C_IN)
        {
            /* XXX - debug? syslog? */
            cp += n;
            continue;		/* XXX - had_error++ ? */
        }
        if ((qtype == T_A || qtype == T_AAAA) && type == T_CNAME)
        {
            if (ap >= &host_aliases[MAXALIASES-1])
                continue;
            n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
            if ((n < 0) || !_hokchar(tbuf)) {
                had_error++;
                continue;
            }
            cp += n;
            /* Store alias. */
            *ap++ = bp;
            n = strlen(bp) + 1;	/* for the \0 */
            bp += n;
            /* Get canonical name. */
            n = strlen(tbuf) + 1;	/* for the \0 */
            if (n > ep - bp) {
                had_error++;
                continue;
            }
            xstrlcpy(bp, tbuf, ep - bp);
            ed->host.h_name = bp;
            bp += n;

            continue;
        }
        if (qtype == T_PTR && type == T_CNAME)
        {
            n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
#ifdef USE_RESOLV_NAME_OK
            if ((n < 0) || !res_hnok(tbuf))
#else
                if ((n < 0) || !_hokchar(tbuf))
#endif
                {
                    had_error++;
                    continue;
                }
            cp += n;
            /* Get canonical name. */
            n = strlen(tbuf) + 1;	/* for the \0 */
            if (n > ep - bp) {
                had_error++;
                continue;
            }
            xstrlcpy(bp, tbuf, ep - bp);
            tname = bp;
            bp += n;
            continue;
        }
        if (type != qtype)
        {
            fprintf(stderr,
                    "gethostby*.getanswer: asked for \"%s %s %s\", got type \"%s\"\n",
                    qname, p_class(C_IN), p_type(qtype),
                    p_type(type));
            cp += n;
            continue;		/* XXX - had_error++ ? */
        }
        switch (type)
        {
            case T_PTR:
                if (strcasecmp(tname, bp) != 0) {
                    fprintf(stderr,
                            AskedForGot, qname, bp);
                    cp += n;
                    continue;	/* XXX - had_error++ ? */
                }
                n = dn_expand(answer->buf, eom, cp, bp, ep - bp);
#ifdef USE_RESOLV_NAME_OK
                if ((n < 0) || !res_hnok(bp))
#else
                    if ((n < 0) || !_hokchar(bp))
#endif
                    {
                        had_error++;
                        break;
                    }
#if MULTI_PTRS_ARE_ALIASES
                cp += n;
                if (!haveanswer)
                    ed->host.h_name = bp;
                else if (ap < &host_aliases[MAXALIASES-1])
                    *ap++ = bp;
                else
                    n = -1;
                if (n != -1) {
                    n = strlen(bp) + 1;	/* for the \0 */
                    bp += n;
                }
                break;
#else
                ed->host.h_name = bp;
                if (_res.options & RES_USE_INET6) {
                    n = strlen(bp) + 1;	/* for the \0 */
                    bp += n;
                    map_v4v6_hostent(&ed->host, &bp, ep);
                }
                h_errno = NETDB_SUCCESS;
                return (&ed->host);
#endif
            case T_A:
            case T_AAAA:
                if (strcasecmp(ed->host.h_name, bp) != 0)
                {
                    fprintf(stderr,
                            AskedForGot, ed->host.h_name, bp);
                    cp += n;
                    continue;	/* XXX - had_error++ ? */
                }
                if (n != ed->host.h_length)
                {
                    cp += n;
                    continue;
                }
                if (type == T_AAAA)
                {
                    struct in6_addr in6;
                    memcpy(&in6, cp, IN6ADDRSZ);
                    if (IN6_IS_ADDR_V4MAPPED(&in6))
                    {
                        cp += n;
                        continue;
                    }
                }
                if (!haveanswer)
                {
                    int nn;

                    ed->host.h_name = bp;
                    nn = strlen(bp) + 1;	/* for the \0 */
                    bp += nn;
                }

                bp += sizeof(align) - ((u_long)bp % sizeof(align));

                if (bp + n >= &hostbuf[sizeof hostbuf])
                {
                    fprintf(stderr, "size (%d) too big\n", n);
                    had_error++;
                    continue;
                }
                if (hap >= &h_addr_ptrs[MAXADDRS-1])
                {
                    if (!toobig++)
                        fprintf(stderr, "Too many addresses (%d)\n", MAXADDRS);
                    cp += n;
                    continue;
                }
                bcopy(cp, *hap++ = bp, n);
                bp += n;
                cp += n;
                break;
        }
        if (!had_error)
            haveanswer++;
    }
    if (haveanswer)
    {
        *ap = NULL;
        *hap = NULL;
        if (!ed->host.h_name)
        {
            n = strlen(qname) + 1;	/* for the \0 */
            if (n > ep - bp)
                goto try_again;
            xstrlcpy(bp, qname, ep - bp);
            ed->host.h_name = bp;
            bp += n;
        }
#if 0
        if (_res.options & RES_USE_INET6)
            map_v4v6_hostent(&ed->host, &bp, ep);
#endif
        h_errno = NETDB_SUCCESS;
        return (&ed->host);
    }
try_again:
    h_errno = TRY_AGAIN;
    return (NULL);
}

static int eres_send(struct eres_data *ed)
{
    if(ed->fd == -1)
    {
        ed->query_len = res_mkquery(QUERY, ed->name, C_IN, ed->host.h_addrtype, NULL, 0, NULL,
                ed->query.buf, MAXPACKET);
#if defined(RES_USE_EDNS0) && defined(RES_USE_DNSSEC)
        if(ed->query_len > 0 && ((_res.options & RES_USE_EDNS0) ||
                    (_res.options & RES_USE_DNSSEC)))
        {
            ed->query_len = res_opt(ed->query_len, ed->query.buf, MAXPACKET, MAXPACKET);
        }
#endif

        if(ed->query_len <= 0)
        {
            h_errno = NO_RECOVERY;
            return ed->query_len;
        }

        if((ed->fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
        {
            h_errno = NETDB_INTERNAL;
            return -1;
        }

        ed->query_id = ed->query.hdr.id;
    }

    if(ed->ns_index >= _res.nscount)
    {
        /* no more name servers to query */
        return -1;
    }

    fprintf(stderr, "querying '%s' about '%s'\n",
            inet_ntoa(_res.nsaddr_list[ed->ns_index].sin_addr), ed->name);
    if(sendto(ed->fd, ed->query.buf, ed->query_len, 0,
                (struct sockaddr *)&(_res.nsaddr_list[ed->ns_index++]),
                sizeof(struct sockaddr)) != ed->query_len)
    {
        h_errno = NETDB_INTERNAL;
        return -1;
    }

    /* add event handler */
    if(!event_initialized(&ed->ev))
    {
        event_set(&ed->ev, ed->fd, EV_READ, eres_event_handler, ed);
    }

    struct timeval tv = {.tv_sec = ERES_TIMEOUT, .tv_usec = 0};
    event_add(&ed->ev, &tv);

    return 0;
}

static void eres_event_handler(int fd, short why, void *arg)
{
    querybuf answer;
    struct eres_data *ed = arg;

    if(why == EV_TIMEOUT)
    {
        /* try next ns */
        if(eres_send(ed) != 0)
        {
            errno = ETIMEDOUT;
            h_errno = NETDB_INTERNAL;
            ed->callback(h_errno, NULL, ed->af, ed->user_data);
            eres_free_data(ed);
        }
        return;
    }

    ssize_t len = recvfrom(ed->fd, answer.buf, sizeof(answer.buf), 0, NULL, NULL);
    if(len < 0)
    {
        /* fixme: try next ns */
        ed->callback(errno, NULL, ed->af, ed->user_data);
        return;
    }

    if(len < HFIXEDSZ)
    {
        /* too short packet */
        /* try next ns */
        if(eres_send(ed) != 0)
        {
            ed->callback(h_errno, NULL, ed->af, ed->user_data);
            eres_free_data(ed);
        }
        return;
    }

    if(answer.hdr.id != ed->query_id)
    {
        struct timeval tv = {.tv_sec = ERES_TIMEOUT, .tv_usec = 0};
        event_add(&ed->ev, &tv);
        return;
    }

    if((answer.hdr.rcode != NOERROR) || (ntohs(answer.hdr.ancount) == 0))
    {
        switch (answer.hdr.rcode)
        {
            case NXDOMAIN:
                h_errno = HOST_NOT_FOUND;
                break;
            case SERVFAIL:
                h_errno = TRY_AGAIN;
                break;
            case NOERROR:
                h_errno = NO_DATA;
                break;
            case FORMERR:
            case NOTIMP:
            case REFUSED:
            default:
                h_errno = NO_RECOVERY;
        }

        /* try next ns */
        if(eres_send(ed) != 0)
        {
            ed->callback(h_errno, NULL, ed->af, ed->user_data);
            eres_free_data(ed);
        }
        return;
    }

    struct hostent *hent = getanswer(ed, &answer, len, ed->name, ed->host.h_addrtype);
    if(hent)
    {
        ed->callback(0 /* success */, hent, ed->af, ed->user_data);
        eres_free_data(ed);
    }
    else
    {
        /* try next ns */
        if(eres_send(ed) != 0)
        {
            ed->callback(h_errno, NULL, ed->af, ed->user_data);
            eres_free_data(ed);
        }
    }
}

int eres_query(const char *host, int af, eres_callback_t cb, void *user_data)
{
    int size, type;

    /* initialize private data */

    if(res_init() != 0)
    {
        return -1;
    }

    switch (af)
    {
        case AF_INET:
            size = INADDRSZ;
            type = T_A;
            break;
        case AF_INET6:
            size = IN6ADDRSZ;
            type = T_AAAA;
            break;
        default:
            h_errno = NETDB_INTERNAL;
            errno = EAFNOSUPPORT;
            return -1;
    }

    /*
     * if there aren't any dots, it could be a user-level alias.
     * this is also done in res_query() since we are not the only
     * function that looks up host names.
     */
    const char *cp;
    if (!strchr(host, '.') && (cp = hostalias(host)))
        host = cp;

    struct eres_data *ed = calloc(1, sizeof(struct eres_data));
    ed->name = strdup(host);
    ed->callback = cb;
    ed->user_data = user_data;
    ed->fd = -1;
    ed->af = af;

    ed->host.h_addrtype = type;
    ed->host.h_length = size;

    if(eres_send(ed) != 0)
    {
        eres_free_data(ed);
        return -1;
    }

    return 0;
}

#ifdef TEST

extern char *__progname;
static int responses = 0, expected_responses = 0;

void dns_callback(int error, struct hostent *host, int af, void *user_data)
{
    char *name = user_data;

    if(error)
    {
        fprintf(stderr, "%s: %s\n",
                name, error == -1 ? strerror(errno) : hstrerror(error));
    }
    else
    {
        int i;
        for(i = 0; host->h_aliases[i]; i++)
        {
            printf("%s is also known as %s\n", name, host->h_aliases[i]);
        }

        printf("addrtype = %i, AF_INET = %i\n", host->h_addrtype, AF_INET);
        
        for(i = 0; host->h_addr_list[i]; i++)
        {
            char dst[128];
            printf("%s has address %s\n",
                    name, inet_ntop(af, host->h_addr_list[i], dst, sizeof(dst)));
        }
    }

    if(++responses >= expected_responses)
    {
        event_loopexit(NULL);
    }
}

int main(int argc, char **argv)
{
    if(argc <= 1)
    {
        printf("syntax: %s name ...\n", __progname);
    }

    event_init();

    int i;
    for(i = 1; i < argc; i++)
    {
        if(eres_query(argv[i], AF_INET, dns_callback, argv[i]) == 0)
        {
            expected_responses++;
        }
        else
        {
            fprintf(stderr, "eres_query(%s) failed\n", argv[i]);
        }
    }

    event_dispatch();

    return 0;
}

#endif

