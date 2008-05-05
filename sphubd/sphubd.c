/*
 * Copyright 2004-2005 Martin Hedenfalk <martin@bzero.se>
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

#define _GNU_SOURCE /* needed for asprintf */

#include <assert.h>
#include <signal.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <evdns.h>

#include "base32.h"
#include "client.h"
#include "globals.h"
#include "hub.h"
#include "notifications.h"
#include "queue.h"
#include "queue_match.h"
#include "search_listener.h"
#include "share.h"
#include "sphashd_client.h"
#include "sphubd.h"
#include "ui.h"
#include "log.h"
#include "extra_slots.h"
#include "dbenv.h"
#include "extip.h"

static char *socket_filename = NULL;

static void handle_share_rescan_event(int fd, short why, void *data)
{
    share_rescan(global_share);
    set_share_rescan_interval(-1);
}

void set_share_rescan_interval(int interval)
{
    static struct event share_rescan_event;
    static int last_interval = 3600;

    if(event_initialized(&share_rescan_event))
    {
        evtimer_del(&share_rescan_event);
    }
    else
    {
        evtimer_set(&share_rescan_event, handle_share_rescan_event, NULL);
    }

    if(interval < 0)
    {
        interval = last_interval;
    }

    if(interval > 0)
    {
        g_debug("starting share rescanner with interval %i seconds", interval);
        last_interval = interval;
        struct timeval tv = {.tv_sec = interval, .tv_usec = 0};
        evtimer_add(&share_rescan_event, &tv);
    }
    else
    {
        g_debug("disabling share rescanner");
    }
}

/* returns 0 if connection attempted */
static int connect_trigger_callback(const char *nick, void *user_data)
{
    /* first verify that the user is logged in on a hub
     */
    hub_t *hub = hub_find_by_nick(nick);
    if(hub == 0 || hub->reconnect_attempt > 0)
    {
        return -1; /* no connect attempted, user is not logged in */
    }

    cc_t *cc = cc_find_by_nick_and_direction(nick, CC_DIR_DOWNLOAD);

    if(cc == NULL)
    {
        /* no ongoing client connection with this nick, initiate one */
        g_info("sending connection request to nick %s", nick);

        if(hub->me->passive)
        {
            if(hub_user_is_passive(hub, nick))
            {
                /* both are passive, unable to connect */
                ui_send_status_message(NULL, hub->address,
                        "Unable to connect to %s: both are passive", nick);
                return 0;
            }
            else
            {
                hub_send_command(hub, "$RevConnectToMe %s %s|",
                        hub->me->nick, nick);
            }
        }
        else
        {
            hub_send_command(hub, "$ConnectToMe %s %s:%u|",
                    nick, hub->me->ip, global_port);
        }
        return 0; /* yes, we did a connect attempt */
    }

    return -1; /* no connect attempted, already connected to this nick */
}

void download_trigger(int fd, short why, void *data)
{
    /* Look for a waiting download that matches an idle download channel. Start
     * if found.
     */
    cc_trigger_download();

    /* re-register the event */
    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    evtimer_add(data, &tv);
}

/* Start (or restart) the client listener on the given port. Returns 0 on
 * success, or -1 on failure. Specify port == 0 to close client listener.
 */
int start_client_listener(int port)
{
    static struct event client_listener_event;
    static int started = 0;
    static int client_listener_fd = -1;
    static int current_port = -1;
    xerr_t *err = 0;

    return_val_if_fail(port >= 0, -1);

    if(started)
    {
        if(current_port == port)
        {
            g_info("already listening on port %i", current_port);
            return 0;
        }
        event_del(&client_listener_event);
        g_debug("closing client listener fd %i", client_listener_fd);
        close(client_listener_fd);
        started = 0;
        current_port = -1;
        client_listener_fd = -1;
    }

    if(port > 0)
    {
        client_listener_fd = io_bind_tcp_socket(port, &err);
        if(client_listener_fd == -1)
        {
            ui_send_status_message(NULL, NULL, "Unable to use TCP port %u: %s",
                    port, err->message);
            return -1;
        }
        g_debug("adding client connection listener on file descriptor %d",
                client_listener_fd);
        event_set(&client_listener_event, client_listener_fd,
                EV_READ|EV_PERSIST, cc_accept_connection, NULL);
        event_add(&client_listener_event, NULL);
        started = 1;
        current_port = port;
    }

    return 0;
}

static void handle_search_response_notification(nc_t *nc, const char *channel,
        nc_search_response_t *notification, void *user_data)
{
    return_if_fail(notification);
    return_if_fail(notification->response);
    search_response_t *response = notification->response;
    return_if_fail(response->hub);

    if(response->id == 0)
    {
        g_warning("valid search response, but no matching search ID");
    }
    else if(response->id > 0)
    {
        /* Ignore search id -1, it's used internally for auto-searches
         */

        hub_t *hub = response->hub;
        user_t *user = hub_lookup_user(hub, response->nick);
        ui_send_search_response(NULL, 
                response->id, hub->address, response->nick, response->filename,
                response->type, response->size, response->openslots,
                response->totalslots, response->tth, user ? user->speed : "");
    }
}

/* set port = 0 to create a passive search listener */
int start_search_listener(int port)
{
    static int started = 0;
    static int current_port = -1;

    if(started)
    {
        if(current_port == port)
        {
            g_info("already listening on port %i", current_port);
            return 0;
        }
        search_listener_close(global_search_listener);
    }
    else
    {
        nc_add_search_response_observer(nc_default(),
                handle_search_response_notification, NULL);
    }

    global_search_listener = search_listener_new(port);

    if(global_search_listener)
    {
        current_port = port;
        started = 1;
        return 0;
    }
    else
    {
        ui_send_status_message(NULL, NULL, "Unable to use UDP port %i: %s",
                port, strerror(errno));
    }
    return -1;
}

static void shutdown_sphubd_event(int fd, short condition, void *data) __attribute (( noreturn ));

static void shutdown_sphubd_event(int fd, short condition, void *data)
{
    /* close all client connections and exit */
    g_message("shutting down");
    if(socket_filename && unlink(socket_filename) != 0)
    {
        g_warning("failed to unlink socket file '%s': %s",
                socket_filename, strerror(errno));
    }

    sp_remove_pid(global_working_directory, "sphubd");

    ui_close_all_connections();
    cc_close_all_connections();
    hub_close_all_connections();
    hs_shutdown();
    tthdb_close();
    queue_close();
    extra_slots_close();
    close_default_db_environment();

    /* be nice to valgrind */
    free(global_working_directory);
    free(argv0_path);
    sp_log_close();

    exit(6);
}

void shutdown_sphubd(void)
{
    shutdown_sphubd_event(0, EV_SIGNAL, NULL);
}

static void handle_download_finished_notification(nc_t *nc, const char *channel,
        nc_download_finished_t *notification, void *user_data)
{
    return_if_fail(notification);
    return_if_fail(notification->filename);

    if(global_incomplete_directory == 0 || global_download_directory == 0)
    {
        /* strange? */
        return;
    }

    if(strcmp(global_incomplete_directory, global_download_directory) == 0)
    {
        /* same directory */
        return;
    }

    if(access(global_download_directory, F_OK) != 0)
    {
        g_warning("Download directory doesn't exist, won't move complete file");
        return;
    }

    queue_target_t *qt = queue_lookup_target(notification->filename);
    return_if_fail(qt);

    queue_directory_t *qd = NULL;
    if(qt->target_directory[0])
    {
        /* this target belongs to a directory download */
        qd = queue_db_lookup_directory(qt->target_directory);
        return_if_fail(qd);
        if(qd->nleft > 1 && !global_move_partial_directories)
        {
            /* There are more than this file left in the directory, and we
             * don't want to move partial directories. */
            g_debug("skipping moving partial directory [%s]",
                    qd->target_directory);
            return;
        }
    }

    char *source = 0;
    char *target = 0;

    if(global_move_partial_directories || qd == NULL)
    {
        asprintf(&source, "%s/%s",
                global_incomplete_directory, notification->filename);
        asprintf(&target, "%s/%s",
                global_download_directory, notification->filename);

        /* make sure the target directory exists */
        char *dir = strdup(target);
        char *e = strrchr(dir, '/');
        assert(e);
        *e = 0;
        mkpath(dir);
        free(dir);
    }
    else
    {
        return_if_fail(qd->nleft == 1);
        asprintf(&source, "%s/%s",
                global_incomplete_directory, qd->target_directory);
        asprintf(&target, "%s/%s",
                global_download_directory, qd->target_directory);
    }

    g_debug("moving [%s] to download directory [%s]", source, target);

    if(rename(source, target) != 0)
    {
        ui_send_status_message(NULL, NULL, "Unable to move file %s: %s",
                source, strerror(errno));
    }
    else
    {
        if(qd && qd->nleft == 1 && !global_move_partial_directories)
        {
            /* the directory is complete, remove the (filesystem) directory */
            char *target_directory;
            asprintf(&target_directory, "%s/%s",
                    global_incomplete_directory, qd->target_directory);
            if(rmdir(target_directory) != 0)
            {
                ui_send_status_message(NULL, NULL,
                        "Unable to remove directory %s: %s",
                        target_directory, strerror(errno));
            }
            free(target_directory);
        }
    }

    free(target);
    free(source);
}

int main(int argc, char **argv)
{
    /* if non-positive, don't listen for UI connections on a TCP socket */
    int ui_tcp_port = -1;
    char *p, *e;

    p = strdup(argv[0]);
    e = strrchr(p, '/');
    if(e)
        *e = 0;
    else
    {
        free(p);
        p = strdup(".");
    }
    argv0_path = absolute_path(p);
    free(p);

    int foreground = 0;

    const char *debug_level = "message";
    int c;
    while((c = getopt(argc, argv, "w:d:fp:h")) != EOF)
    {
        switch(c)
        {
            case 'w':
                global_working_directory = verify_working_directory(optarg);
                break;
            case 'd':
                debug_level = optarg;
                break;
            case 'p':
                ui_tcp_port = strtol(optarg, NULL, 10);
                break;
            case 'f':
                foreground = 1;
                break;
            case 'h':
                printf("syntax: sphubd -d <none|warning|message|info|debug>\n"
                        "               -w <working directory>\n"
                        "               -p <ui listen port>\n");
                return 2;
            case '?':
            default:
                /* skip unknown options */
                g_warning("Unknown option -%c, skipped", c);
                break;
        }
    }

    srand(time(0));

    if(global_working_directory == NULL)
        global_working_directory = get_working_directory();
    global_incomplete_directory = tilde_expand_path("~");
    global_download_directory = tilde_expand_path("~");

#if 0
    global_id_generator = strdup("ShakesPeer");
    global_id_tag = strdup("SP");
    global_id_lock = strdup("ShakesPeer");
    global_id_version = strdup(VERSION);
#else
    global_id_generator = strdup("DC++");
    global_id_tag = strdup("++");
    global_id_lock = strdup("DCPLUSPLUS");
    global_id_version = strdup("0.691");
#endif

    sp_log_init(global_working_directory, "sphubd");
    sp_log_set_level(debug_level);

    pid_t pid = sp_get_pid(global_working_directory, "sphubd");
    if(pid != -1)
    {
        g_warning("sphubd already running as pid %u, aborting startup", pid);
        return 14;
    }

    g_message("starting up, version = %s, log level = %s",
            VERSION, debug_level);

    /* Put ourselves in the background
     */
    if(!foreground && sp_daemonize() != 0)
    {
        g_warning("failed to daemonize: %s", strerror(errno));
        exit(1);
    }
    sp_write_pid(global_working_directory, "sphubd");

    /* Initialize the event library */
    event_init();
    evdns_init();
    /* need two priorities */
    event_priority_init(2);
    g_message("using libevent version %s, method %s",
            event_get_version(), event_get_method());

    /* install signal handlers
     */
    struct event sigterm_event;
    signal_set(&sigterm_event, SIGTERM, shutdown_sphubd_event, NULL);
    signal_add(&sigterm_event, NULL);

    struct event sigint_event;
    signal_set(&sigint_event, SIGINT, shutdown_sphubd_event, NULL);
    signal_add(&sigint_event, NULL);

    /* initialize the database
     */
    xerr_t *err = 0;

    g_debug("initializing share");
    global_share = share_new();
    return_val_if_fail(global_share, 7);
    share_tth_init_notifications(global_share);
    if(tthdb_open() != 0)
    {
	g_warning("failed to open TTH database, aborting startup");
	shutdown_sphubd();
    }

    extra_slots_init();

    g_debug("initializing queue");
    queue_init();
    queue_match_init();
    queue_auto_search_init();

    /* start database checkpointing event */
    db_schedule_maintenance();
    db_prune_logfiles();

    asprintf(&socket_filename, "%s/sphubd", global_working_directory);

    /* start hashing daemon
     */
    if(hs_start() != 0)
    {
        return 6;
    }

    hub_list_init();
    cc_list_init();
    ui_list_init();

    /* create a socket for user interface connections
     */
    int ui_fd = io_bind_unix_socket(socket_filename);
    if(ui_fd == -1)
        return 1;
    g_debug("adding local UI listener on file descriptor %d", ui_fd);
    struct event ui_connect_event;
    event_set(&ui_connect_event, ui_fd, EV_READ|EV_PERSIST,
            ui_accept_connection, NULL);
    event_add(&ui_connect_event, NULL);

    if(ui_tcp_port > 0)
    {
        int ui_tcp_fd = io_bind_tcp_socket(ui_tcp_port, &err);
        if(ui_tcp_fd != -1)
        {
            g_debug("adding remote UI listener on file descriptor %d", ui_tcp_fd);
            struct event ui_tcp_connect_event;
            event_set(&ui_tcp_connect_event, ui_tcp_fd, EV_READ|EV_PERSIST,
                    ui_accept_connection, NULL);
            event_add(&ui_tcp_connect_event, NULL);
        }
        else
        {
            g_warning("failed to bind port %i for remote UI connections",
                    ui_tcp_port);
        }
    }

    set_share_rescan_interval(-1);
    hub_start_myinfo_updater();
    cc_set_transfer_stats_interval(-1);

    nc_add_download_finished_observer(nc_default(),
            handle_download_finished_notification, NULL);

    g_debug("starting download trigger");
    struct event download_trigger_event;
    evtimer_set(&download_trigger_event, download_trigger,
            &download_trigger_event);
    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    evtimer_add(&download_trigger_event, &tv);

    queue_connect_schedule_trigger(connect_trigger_callback);

    extip_init();

    g_debug("starting main loop");
    event_dispatch();
    g_warning("main loop returned");
    
    return 0;
}

