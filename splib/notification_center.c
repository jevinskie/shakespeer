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

/* $Id: notification_center.c,v 1.8 2006/03/26 09:59:53 mhe Exp $ */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "notification_center.h"

nc_t *nc_new(void)
{
    nc_t *nc = calloc(1, sizeof(nc_t));
    return nc;
}

nc_t *nc_default(void)
{
    static nc_t *default_nc = NULL;
    if(default_nc == NULL)
    {
        default_nc = nc_new();
    }
    return default_nc;
}

static nc_observer_t *nc_observer_new(const char *channel,
        nc_callback_t callback, void *user_data)
{
    nc_observer_t *ob = calloc(1, sizeof(nc_observer_t));
    ob->channel = strdup(channel);
    ob->callback = callback;
    ob->user_data = user_data;

    return ob;
}

static void nc_observer_free(nc_observer_t *ob)
{
    if(ob)
    {
        free(ob->channel);
        free(ob);
    }
}

void nc_add_observer(nc_t *nc, const char *channel,
        nc_callback_t callback, void *user_data)
{
    assert(nc);
    assert(channel);
    assert(callback);

    nc_observer_t *ob = nc_observer_new(channel, callback, user_data);
    LIST_INSERT_HEAD(&nc->observers, ob, next);
}

void nc_remove_observer(nc_t *nc, const char *channel, nc_callback_t callback)
{
    assert(nc);
    assert(channel);
    assert(callback);

    nc_observer_t *ob;
    LIST_FOREACH(ob, &nc->observers, next)
    {
        if(ob->callback == callback && strcmp(ob->channel, channel) == 0)
        {
            LIST_REMOVE(ob, next);
            nc_observer_free(ob);
            break;
        }
    }
}

void nc_send_notification(nc_t *nc, const char *channel, void *data)
{
    assert(nc);
    assert(channel);

    nc_observer_t *ob;
    LIST_FOREACH(ob, &nc->observers, next)
    {
        if(strcmp(channel, ob->channel) == 0)
        {
            ob->callback(nc, channel, data, ob->user_data);
        }
    }
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

int sample_callback_called = 0;

void sample_callback(nc_t *nc, const char *channel, void *data, void *user_data)
{
    fail_unless(nc);
    fail_unless(nc == nc_default());
    fail_unless(channel);
    fail_unless(user_data == nc);
    fail_unless(strcmp(channel, "sample channel") == 0);

    fail_unless(data);
    fail_unless(strcmp((const char *)data, "sample data") == 0);
    ++sample_callback_called;
}

int main(int argc, char **argv)
{
    /* create the shared, default notification center */
    nc_t *nc = nc_default();

    /* send a notification without any observers */
    nc_send_notification(nc, "sample channel", "sample data");
    fail_unless(sample_callback_called == 0);

    /* add an observers */
    nc_add_observer(nc, "sample channel", sample_callback, nc);

    /* notify all observers */
    nc_send_notification(nc, "sample channel", "sample data");
    fail_unless(sample_callback_called == 1);

    /* remove the observer */
    nc_remove_observer(nc, "sample channel", sample_callback);
    nc_send_notification(nc, "sample channel", "sample data");
    fail_unless(sample_callback_called == 1);

    return 0;
}

#endif

