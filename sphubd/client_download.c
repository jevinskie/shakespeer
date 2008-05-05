/*
 * Copyright 2005 Martin Hedenfalk <martin@bzero.se>
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

#include <sys/stat.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "log.h"
#include "globals.h"
#include "client.h"
#include "bz2.h"
#include "he3.h"
#include "notifications.h"
#include "xerr.h"
#include "xstr.h"

/* Sends a download request for the current download queue (assumes
 * cc->current_queue is already set). Chooses request command based on the
 * peers capabilities.
 *
 * returns 0 on success, any other value on error
 */
static int cc_send_download_request(cc_t *cc)
{
    return_val_if_fail(cc, -1);
    return_val_if_fail(cc->current_queue, -1);

    queue_t *queue = cc->current_queue;
    cc->state = CC_STATE_REQUEST;
    cc->last_activity = time(0);

    if(cc->has_adcget)
    {
        if(queue->is_filelist)
        {
            return cc_send_command_as_is(cc, "$ADCGET file %s 0 -1|",
                    queue->source_filename);
        }
        else
        {
            const char *base = "";
            const char *request_filename = queue->source_filename;
            if(queue->tth && cc->has_tthf)
            {
                base = "TTH/";
                request_filename = queue->tth;
            }
            /* fetching of TTH leafdata is not enabled, since we're not even
             * verifying the TTH on downloaded files yet */
#if 0
            if(cc->has_tthl && queue->tth && cc->fetch_leaves == 0)
            {
                cc->fetch_leaves = 1;
                return cc_send_command_as_is(cc, "$ADCGET tthl %s%s 0 -1|",
                        base, request_filename);
            }
            else
#endif
            {
                return cc_send_command_as_is(cc, "$ADCGET file %s%s %llu %llu|",
                        base, request_filename,
                        queue->offset, queue->size - queue->offset);
            }
        }
    }
    else if(cc->has_xmlbzlist && queue->size > 0 && !queue->is_filelist)
    {
        return cc_send_command_as_is(cc, "$UGetBlock %llu %llu %s|",
                queue->offset, queue->size - queue->offset,
                queue->source_filename);
    }
    else
    {
        return cc_send_command(cc, "$Get %s$%llu|",
                queue->source_filename, queue->offset + 1);
    }
}

int cc_request_download(cc_t *cc)
{
    return_val_if_fail(cc, -1);
    return_val_if_fail(cc->current_queue == NULL || cc->fetch_leaves == 2, -1);

    queue_t *queue = cc->current_queue;
    while(queue == NULL)
    {
        queue = queue_get_next_source_for_nick(cc->nick);
        if(queue == NULL)
        {
            /* no more files to download */
            return -1;
        }

        queue->offset = 0ULL;

        if(queue->is_directory)
        {
            if(queue_resolve_directory(cc->nick, queue->source_filename,
                        queue->target_filename, NULL) != 0)
            {
                /* Directory was not directly resolvable, but it should be! */
                /* When the directory is added to the queue, it is directly
                 * resolved if the filelist is directly available. Otherwise
                 * the filelist is queued and should be downloaded before the
                 * directory */
                g_warning("unresolvable directory [%s] ???",
                        queue->source_filename);
            }
        }
        else if(queue->is_filelist)
        {
            char *safe_nick = strdup(cc->nick);
            str_replace_set(safe_nick, "/", '_');

            if(cc->has_xmlbzlist)
            {
                asprintf(&queue->target_filename, "%s/files.xml.%s.bz2",
                        global_working_directory, safe_nick);
                queue->source_filename = strdup("files.xml.bz2");
            }
            else
            {
                asprintf(&queue->target_filename, "%s/MyList.%s.DcLst",
                        global_working_directory, safe_nick);
                queue->source_filename = strdup("MyList.DcLst");
            }
            free(safe_nick);

            break;
        }
        else
        {
            struct stat stbuf;

            char *target = 0;
            asprintf(&target, "%s/%s",
                    global_storage_directory, queue->target_filename);

            if(stat(target, &stbuf) == 0)
            {
                g_message("local file in storage dir already exists: [%s]",
                        target);
                /* what if size differs? */
                queue_remove_target(queue->target_filename);
                free(target);
            }
            else
            {
                free(target);
                asprintf(&target, "%s/%s",
                        global_download_directory, queue->target_filename);

                int rc = stat(target, &stbuf);
                free(target);
                if(rc == 0)
                {
                    /* file already exists, resume */
                    queue->offset = stbuf.st_size;
                    if(queue->offset >= queue->size)
                    {
                        g_message("local file has larger or equal size than remote,"
                                " can't resume, removing queue");

                        nc_send_download_finished_notification(nc_default(),
                                queue->target_filename);

                        queue_remove_target(queue->target_filename);
                    }
                    else
                    {
                        /* resume download */
                        break;
                    }
                }
                else
                {
                    /* regular file, doesn't already exist */
                    break;
                }
            }
        }

        /* try again */

        queue_free(queue);
        queue = NULL;
    }

    cc->current_queue = queue;
    if(cc_send_download_request(cc) != 0)
    {
        cc_close_connection(cc);
        return -1;
    }

    if(queue->is_filelist)
        cc->filesize = 0ULL;
    else
        cc->filesize = queue->size;
    cc->offset = queue->offset;

    /* tell the download queue that we're now handling this request */
    if(queue->is_filelist)
        queue_set_filelist_active(queue, 1);
    else
        queue_set_target_active(queue, 1);

    return 0;
}

void cc_finish_download(cc_t *cc)
{
    g_info("finished downloading file");
    if(close(cc->local_fd) != 0)
    {
        g_warning("close: %s", strerror(errno));
    }
    cc->local_fd = -1;

    return_if_fail(cc->current_queue);

    if(cc->current_queue->is_filelist)
    {
        char *filepath = cc->current_queue->target_filename;
        char *filepath_noext = strdup(filepath);
        char *ext = strrchr(filepath_noext, '.');
        return_if_fail(ext);
        /* strip the .bz2 or .DcLst suffix */
        *ext++ = 0;

        /* decompress the filelist */
        g_debug("decompressing filelist [%s] -> [%s]",
                filepath, filepath_noext);
        xerr_t *err = 0;
        if(strcmp(ext, "bz2") == 0)
            bz2_decode(filepath, filepath_noext, &err);
        else
            he3_decode(filepath, filepath_noext, &err);
        if(err)
        {
            g_warning("failed to decompress filelist: %s", xerr_msg(err));
            ui_send_status_message(NULL, cc->hub->address,
                    "failed to decompress filelist: %s", xerr_msg(err));
            xerr_free(err);
        }
        else
        {
            nc_send_filelist_finished_notification(nc_default(),
                    cc->hub->address, cc->current_queue->nick,
                    filepath_noext, cc->current_queue->auto_matched);
        }

        free(filepath_noext);
    }
    else if(cc->fetch_leaves != 1)
    {
        nc_send_download_finished_notification(nc_default(),
                cc->current_queue->target_filename);
        ui_send_download_finished(NULL, cc->current_queue->target_filename);
    }

    if(cc->fetch_leaves != 1)
    {
        if(cc->current_queue->is_filelist)
        {
            queue_remove_filelist(cc->current_queue->nick);
        }
        else
        {
            queue_remove_target(cc->current_queue->target_filename);
        }
        queue_free(cc->current_queue);
        cc->current_queue = NULL;
        cc->fetch_leaves = 0;
    }
    else
    {
        cc->fetch_leaves = 2;
    }

    cc->state = CC_STATE_READY;
    cc->last_activity = time(0);

    /* Request another file if there is one in queue for us */
    cc_request_download(cc);
}

static int cc_download_write(cc_t *cc, char *buf, size_t bytes_read)
{
    return_val_if_fail(cc, -1);
    return_val_if_fail(buf, -1);

    if(write(cc->local_fd, buf, bytes_read) == -1)
    {
        g_warning("write failed: %s", strerror(errno));
        return -1;
    }

    cc->bytes_done += bytes_read;

    return 0;
}

int cc_start_download(cc_t *cc)
{
    return_val_if_fail(cc, -1);
    return_val_if_fail(cc->current_queue, -1);

    g_debug("global_download_directory = [%s]", global_download_directory);

    int dl_dir_exists;
    if(cc->current_queue->is_filelist)
    {
        dl_dir_exists = 1;
    }
    else
    {
        dl_dir_exists = global_download_directory &&
            access(global_download_directory, F_OK) == 0;
    }

    if(!dl_dir_exists)
    {
        g_warning("download directory [%s] doesn't exist, refuses to download",
                global_download_directory);
        ui_send_status_message(NULL, cc->hub->address,
                "Download directory '%s' doesn't exist"
                " (unattached external harddrive?)", global_download_directory);
        return -1;
    }

    cc->bytes_done = 0;

    char *target = 0; /* complete, absolute target path in local filesystem */

    if(cc->fetch_leaves == 1)
    {
        asprintf(&target, "%s/%s.tthl",
                global_download_directory, cc->current_queue->target_filename);
    }
    else if(cc->current_queue->is_filelist)
    {
        target = strdup(cc->current_queue->target_filename);
    }
    else
    {
        asprintf(&target, "%s/%s",
               global_download_directory, cc->current_queue->target_filename);
    }

    g_debug("target: [%s]", target);

    /* make sure target (local) directory exists */
    char *local_dir = strdup(target);
    char *e = strrchr(local_dir, '/');
    assert(e);
    *++e = 0;
    g_debug("mkpath(%s)", local_dir);
    mkpath(local_dir);
    free(local_dir);

    cc->local_fd = open(target, O_RDWR | O_CREAT, 0644);
    g_debug("opened %s for writing, fd = %d", target, cc->local_fd);
    free(target);
    target = cc->current_queue->target_filename;
    if(cc->local_fd == -1)
    {
        g_warning("%s: %s", target, strerror(errno));
        return -1;
    }

    if(lseek(cc->local_fd, cc->offset, SEEK_SET) == -1)
    {
        g_warning("lseek failed: %s", strerror(errno));
        return -1;
    }

    cc->transfer_start_time = time(0);
    cc->last_transfer_activity = time(0);

    cc->state = CC_STATE_BUSY;
    ui_send_download_starting(NULL, cc->hub->address, cc->nick,
            cc->current_queue->source_filename, target, cc->current_queue->size);

    /* write any leftover bytes from previous read (in command mode) */
    cc_download_read(cc);

    return 0;
}

void cc_download_read(cc_t *cc)
{
    struct evbuffer *input_buffer = EVBUFFER_INPUT(cc->bufev);
    size_t input_data_len = EVBUFFER_LENGTH(input_buffer);

    if(input_data_len == 0)
    {
        return;
    }

    size_t maxsize = cc->bytes_to_transfer - cc->bytes_done;
    if(input_data_len > maxsize)
    {
        input_data_len = maxsize;
    }

    char *input_data = (char *)EVBUFFER_DATA(input_buffer);

    if(cc_download_write(cc, input_data, input_data_len) != 0)
    {
        cc_close_connection(cc);
    }
    else
    {
        evbuffer_drain(input_buffer, input_data_len);

        cc->last_transfer_activity = time(0);

        if(cc->bytes_done >= cc->bytes_to_transfer)
        {
            cc_finish_download(cc);
        }
    }

#if 0
    static char buf[8192];
    size_t nbytes = sizeof(buf);
    size_t maxsize = cc->bytes_to_transfer - cc->bytes_done;
    if(nbytes > maxsize)
    {
        nbytes = maxsize;
    }

    ssize_t rc = read(fd, buf, nbytes);

    if(rc < 0)
    {
        g_warning("error: %s", strerror(errno));
        cc_close_connection(cc);
        return;
    }
    else if(rc == 0)
    {
        g_warning("end-of-file on fd %d", fd);
        cc_close_connection(cc);
        return;
    }
    else
    {
        if(cc_download_write(cc, buf, rc) != 0)
        {
            return;
        }
    }

    if(cc->bytes_done >= cc->bytes_to_transfer)
    {
        cc_finish_download(cc);
    }
#endif
}

