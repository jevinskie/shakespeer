/* $Id: notification_center.h,v 1.5 2006/04/09 12:54:31 mhe Exp $ */

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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include "sys_queue.h"

typedef struct notification_center nc_t;
typedef struct nc_observer nc_observer_t;
typedef void (*nc_callback_t)(nc_t *nc, const char *channel,
        void *data, void *user_data);

struct nc_observer
{
    char *channel;
    nc_callback_t callback;
    void *user_data;
    LIST_ENTRY(nc_observer) next;
};

struct notification_center
{
    LIST_HEAD(, nc_observer) observers;
};

nc_t *nc_new(void);
nc_t *nc_default(void);

void nc_add_observer(nc_t *nc, const char *channel,
        nc_callback_t callback, void *user_data);
void nc_remove_observer(nc_t *nc, const char *channel,
        nc_callback_t callback);
void nc_send_notification(nc_t *nc, const char *channel, void *data);

