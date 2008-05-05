/*
 * Copyright 2006 Martin Hedenfalk <martin@bzero.se>
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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "share.h"
#include "globals.h"
#include "notifications.h"
#include "tthdb.h"

/***** handling TTHs and leaf data *****/

static void share_insert_file(share_t *share, share_mountpoint_t *mp,
        share_file_t *file)
{
    if(share_lookup_file(share, file->path) == NULL)
    {
        if(share_lookup_unhashed_file(share, file->path) != NULL)
        {
            /* move the file from the unhashed tree to the hashed tree */
            RB_REMOVE(file_tree, &share->unhashed_files, file);
        }
        else
        {
            g_warning("File [%s] not in unhashed tree!?", file->path);
        }

        RB_INSERT(file_tree, &share->files, file);
    }
    else
    {
        g_warning("File [%s] already in hashed tree!?", file->path);
    }

    /* update mountpoint statistics */
    mp->stats.size += file->size;
    mp->stats.nfiles++;

    /* add the file to the bloom filter */
    bloom_add_filename(share->bloom, file->name);
}

void handle_tth_available_notification(nc_t *nc,
        const char *channel,
        nc_tth_available_t *notification,
        void *user_data)
{
    share_t *share = user_data;
    share_file_t *file = notification->file;

    struct tthdb_data *td = malloc(sizeof(struct tthdb_data) +
            notification->leafdata_len);
    td->inode = file->inode;
    td->size = file->size;
    td->mtime = file->mtime;
    td->leafdata_len = notification->leafdata_len;
    memcpy(td->leafdata, notification->leafdata,
            notification->leafdata_len);

    /*g_debug("adding tth for file [%s], inode %llu",
            file->path, file->inode);*/
    int rc = tthdb_add(notification->tth, td);
    free(td);

    share_mountpoint_t *mp = share_lookup_local_root(share, file->path);
    if(mp == NULL)
    {
        g_warning("Mountpoint disappeared!");
        return;
    }

    share->uptodate = 0;

    if(rc == 0)
    {
        share_insert_file(share, mp, file);
    }
    else if(rc == 1)
    {
        /* constraint violation; TTH not unique */
        /* update this offending file to be a duplicate of the original */

        struct tthdb_data *original_tth = tthdb_lookup(notification->tth);
        if(original_tth)
        {
            /* duplicate, check if original is shared */
            share_file_t *original_file =
                share_lookup_file_by_inode(share, original_tth->inode);
            if(original_file)
            {
                /* original shared, keep as duplicate */
                file->duplicate_inode = original_tth->inode;
                mp->stats.nduplicates++;
                mp->stats.dupsize += file->size;
            }
            else
            {
                /* original not shared, switch duplicates */
                original_tth->inode = file->inode;
                original_tth->mtime = file->mtime;
                tthdb_update(notification->tth, original_tth);

                share_insert_file(share, mp, file);
            }
        }
        else
        {
            g_warning("THIS SHOULD NOT HAPPEN! INCONSISTENT TTH DATABASE?");
        }
    }
    else if(rc == -1)
    {
        g_warning("share_add_tth failed");
    }
}

void share_tth_init_notifications(share_t *share)
{
    nc_add_tth_available_observer(nc_default(),
            handle_tth_available_notification,
            share);
}

