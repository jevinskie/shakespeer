/*
 * Copyright 2004-2006 Martin Hedenfalk <martin@bzero.se>
 *
 * This file is part of ShakesPeer.
 *
 * ShakesPeer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ShakesPeer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ShakesPeer; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>

#include "search_listener.h"
#include "notifications.h"
#include "log.h"
#include "globals.h"
#include "filelist.h"
#include "queue.h"
#include "xstr.h"

#include "ui.h"

static bool queue_match_search_response = true;
static bool queue_auto_download_filelists = true;

typedef struct queue_match_filelist_data queue_match_filelist_data_t;
struct queue_match_filelist_data
{
    char *nick;
    char *filelist_path;
    fl_xml_ctx_t *fl_ctx;
    fl_dir_t *root;
    fl_dir_t *dir;
    struct event ev;
};

static void queue_match_filelist_schedule_event(
        queue_match_filelist_data_t *udata);

static void queue_match_filelist_callback(const char *path, const char *tth,
        uint64_t size, void *user_data)
{
    queue_match_filelist_data_t *udata = user_data;

    queue_target_t *qt = queue_lookup_target_by_tth(tth);
    if(qt && qt->size == size)
    {
        g_debug("Found matching queue target [%s], adding source '%s'",
                qt->filename, udata->nick);

        queue_source_t qs;
        memset(&qs, 0, sizeof(qs));
        xstrlcpy(qs.target_filename, qt->filename, sizeof(qs.target_filename));
        xstrlcpy(qs.source_filename, path, sizeof(qs.source_filename));

        if(queue_add_source(udata->nick, &qs) == 0)
        {
            nc_send_queue_source_added_notification(nc_default(),
                    qt->filename, udata->nick, path);
        }
    }
}

static void queue_match_filelist_event(int fd, short why, void *data)
{
    queue_match_filelist_data_t *udata = data;
    return_if_fail(udata);

    if(udata->fl_ctx == NULL)
    {
        udata->fl_ctx = fl_xml_prepare_file(udata->filelist_path,
                queue_match_filelist_callback, udata);
        if(udata->fl_ctx == NULL)
        {
            g_warning("failed to read xml filelist for nick [%s]", udata->nick);
            free(udata->nick);
            free(udata->filelist_path);
            free(udata);
            return;
        }
    }

    if(fl_parse_xml_chunk(udata->fl_ctx) != 0)
    {
        fl_free_dir(udata->fl_ctx->root);
        fl_xml_free_context(udata->fl_ctx);

        g_debug("done matching queue against %s's filelist", udata->nick);
        free(udata->nick);
        free(udata->filelist_path);
        free(udata);
    }
    else
    {
        /* re-schedule the event */
        queue_match_filelist_schedule_event(udata);
    }
}

static void queue_match_filelist_schedule_event(
        queue_match_filelist_data_t *udata)
{
    return_if_fail(udata);

    if(!event_initialized(&udata->ev))
    {
        evtimer_set(&udata->ev, queue_match_filelist_event, udata);
        event_priority_set(&udata->ev, 2);
    }

    struct timeval tv = {.tv_sec = 0, .tv_usec = 500};
    evtimer_add(&udata->ev, &tv);
}

void queue_match_filelist(const char *filelist_path, const char *nick)
{
    g_debug("matching against %s's filelist [%s]", nick, filelist_path);
    
    queue_match_filelist_data_t *udata = calloc(1, sizeof(queue_match_filelist_data_t));
    udata->nick = strdup(nick);
    udata->filelist_path = strdup(filelist_path);

    queue_match_filelist_schedule_event(udata);
}

static void queue_handle_search_response_notification(nc_t *nc,
        const char *channel,
        nc_search_response_t *notification,
        void *user_data)
{
    if(!queue_match_search_response)
    {
        return;
    }

    search_response_t *resp = notification->response;
    return_if_fail(resp);

    /* TTH is required for matching */
    if(resp->tth == NULL)
    {
        return;
    }

    g_debug("matching on TTH '%s', size %llu", resp->tth, resp->size);

    queue_target_t *qt = queue_lookup_target_by_tth(resp->tth);
    if(qt && qt->size == resp->size)
    {
        g_debug("Found matching queue target [%s], adding source '%s'",
                qt->filename, resp->nick);

        if(resp->id == -1 && global_auto_match_filelists)
        {
            /* This search response originated from an auto-search for new
             * sources. If this file is part of a directory download, it is
             * quite possible that this source has more matches for our queue.
             *
             * So check the filelist so we can match against all his files.
             */

            /* FIXME: check for directory download */

            char *existing_filelist = find_filelist(global_working_directory,
                    resp->nick);
            if(existing_filelist)
            {
                /* great, we already got the filelist, match against it */
                g_info("auto-matching against filelist from [%s]", resp->nick);
                queue_match_filelist(existing_filelist, resp->nick);
                free(existing_filelist);
            }
            else if(queue_auto_download_filelists)
            {
                /* we have to download it first (matching is done
                 * automagically) */

                ui_send_status_message(NULL, NULL, 
                        "queueing filelist from [%s] for auto-matching",
                        resp->nick);
                queue_add_filelist(resp->nick, 1 /* auto-matched */);
            }
        }

        queue_source_t qs;
        memset(&qs, 0, sizeof(qs));
        xstrlcpy(qs.target_filename, qt->filename, sizeof(qs.target_filename));
        xstrlcpy(qs.source_filename, resp->filename, sizeof(qs.source_filename));

        if(queue_add_source(resp->nick, &qs) == 0)
        {
            nc_send_queue_source_added_notification(nc_default(),
                    qt->filename, resp->nick, resp->filename);
        }
    }
}

static void queue_handle_filelist_finished_notification(nc_t *nc,
        const char *channel,
        nc_filelist_finished_t *notification,
        void *user_data)
{
    /* match the downloaded filelist against the queue (on TTH and size)
     * TTH:s are only available in xml filelists */
    if(is_filelist(notification->filename) == FILELIST_XML)
    {
        queue_match_filelist(notification->filename, notification->nick);
    }
    else if(notification->auto_matched == 1)
    {
#ifdef BLAH /* FIXME */
        ui_send_status_message(NULL, notification->hub_address,
                "User %s doesn't support auto-matching filelists",
                notification->nick);
#endif
    }
}

void queue_match_init(void)
{
    /* register for notifications */
    nc_add_search_response_observer(nc_default(),
            queue_handle_search_response_notification, NULL);

    nc_add_filelist_finished_observer(nc_default(),
            queue_handle_filelist_finished_notification, NULL);
}

