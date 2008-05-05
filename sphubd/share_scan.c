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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <event.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "share.h"
#include "nfkc.h"
#include "log.h"
#include "notifications.h"

#include "globals.h"
#include "ui_send.h"

typedef struct share_scan_directory share_scan_directory_t;
struct share_scan_directory
{
    LIST_ENTRY(share_scan_directory) link;
    char *dirpath;
};

typedef struct share_scan_state share_scan_state_t;
struct share_scan_state
{
    LIST_HEAD(, share_scan_directory) directories;
    share_t *share;
    struct event ev;
    share_mountpoint_t *mp;
};

#define SHARE_STAT_TO_INODE(st) (uint64_t)(((uint64_t)st->st_size << 32) | st->st_ino)

static void share_scan_schedule_event(share_scan_state_t *ctx);

static int share_skip_file(const char *filename)
{
    if(filename[0] == '.')
        return 1;
    if(strchr(filename, '$') != NULL)
        return 1;
    if(strchr(filename, '|') != NULL)
        return 1;
    return 0;
}

static void share_scan_add_file(share_scan_state_t *ctx,
        const char *filepath, struct stat *stbuf)
{
    share_file_t *f = calloc(1, sizeof(share_file_t));
    f->path = strdup(filepath);
    f->name = strrchr(f->path, '/') + 1; /* points into f->path ! */
    f->type = share_filetype(f->name);
    f->size = stbuf->st_size;
    f->inode = SHARE_STAT_TO_INODE(stbuf);
    f->mtime = stbuf->st_mtime;

    /* DEBUG("adding file [%s], inode %llu", f->path, f->inode); */

    /* is it already hashed? */
    bool already_hashed = false;
    struct tth_inode *ti = tth_store_lookup_inode(global_tth_store, f->inode);

    if(ti == NULL)
    {
        /* unhashed */
    }
    else
    {
        /* hashed or duplicate */

	struct tth_entry *td = tth_store_lookup(global_tth_store, ti->tth);

	if(td == NULL || ti->mtime != stbuf->st_mtime)
	{
	    /* This is either an inode without a TTH, which is useless,
	     * or an obsolete inode (modified file). */
	    DEBUG("removing obsolete inode %llu", f->inode);
	    tth_store_remove_inode(global_tth_store, f->inode);
#if 0 /* FIXME: should we remove the TTH too? */
	    if(td)
		tth_store_remove(global_tth_store, ti->tth);
#endif
	}
	else
        {
	    already_hashed = true;

            /* DEBUG("Found TTH, inode = %llu", td->inode); */
	    if(td->active_inode == 0)
	    {
		/* TTH is not active, claim this TTH for this inode */
		tth_store_set_active_inode(global_tth_store,
		    ti->tth, f->inode);
	    }
            else if(td->active_inode != f->inode)
            {
                /* DEBUG("inode collision, duplicate"); */
		/* check if the original is shared */
		share_file_t *original_file =
		    share_lookup_file_by_inode(ctx->share, td->active_inode);
		if(original_file)
		{
		    /* ok, keep as duplicate */
		    DEBUG("Setting inode %llu [%s] as duplicate of inode %llu",
			    f->inode, f->path, td->active_inode);
		    f->duplicate_inode = td->active_inode;

		    /* update the mount statistics */
		    ctx->mp->stats.nduplicates++;
		    ctx->mp->stats.dupsize += f->size;
		}
		else
		{
		    /* original not shared, switch with duplicate */
		    /* (this can only happen if shares has been removed live) */
		    tth_store_set_active_inode(global_tth_store,
			    ti->tth, f->inode);
		}
            }
            /* else we found the same file again: rehash (or circular link?) */
        }
    }

    if(f->duplicate_inode == 0)
    {
	if(already_hashed)
	{
	    /* Insert it in the tree. */
	    RB_INSERT(file_tree, &ctx->share->files, f);

	    /* update the mount statistics */
	    ctx->mp->stats.nfiles++;
	    ctx->mp->stats.size += f->size;

	    /* add it to the bloom filter */
	    bloom_add_filename(ctx->share->bloom, f->name);
	}
	else
	{
	    /* Insert it in the unhashed tree. */
	    RB_INSERT(file_tree, &ctx->share->unhashed_files, f);
	}

	/* add it to the inode hash */
	share_add_to_inode_table(ctx->share, f);
    }

    /* update the mount statistics */
    ctx->mp->stats.ntotfiles++;
    ctx->mp->stats.totsize += f->size;

    nc_send_share_file_added_notification(nc_default(),
        ctx->share, f, ctx->mp);
}

static char *share_scan_absolute_path(const char *dirpath,
        const char *filename)
{
#if 0
    /* GError *utf_err = 0; */
    char *utf8_filename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
    if(utf8_filename == 0)
    {
        WARNING("encoding conversion failed (skipping '%s/%s')",
                /*utf_err->message,*/ dirpath, filename);
        /* g_error_free(utf_err); */
        /* utf_err = 0; */
        return NULL;
    }
#endif

    if(!g_utf8_validate(filename, -1, NULL))
    {
        WARNING("Unknown encoding in filename [%s]", filename);
        return NULL;
    }
    /* FIXME: if filename is not in utf-8, try to convert from some
     * local encoding (locale) */

#ifdef __APPLE__ /* FIXME: is this correct? */
    /* normalize the string (handles different decomposition) */
    /* Can we be sure this is on a HFS+ partition? Nope... */
    char *utf8_filename = g_utf8_normalize(filename, -1,
            G_NORMALIZE_DEFAULT);
#else
    char *utf8_filename = strdup(filename);
#endif

    char *filepath;
    asprintf(&filepath, "%s/%s", dirpath, utf8_filename);
    free(utf8_filename);

    return filepath;
}

static void share_scan_push_directory(share_scan_state_t *ctx,
        const char *dirpath)
{
    share_scan_directory_t *d = calloc(1, sizeof(share_scan_directory_t));
    d->dirpath = strdup(dirpath);

    LIST_INSERT_HEAD(&ctx->directories, d, link);
}

/* Scans the files in the given directory and adds found files. Pushes any
 * subdirectories found to the ctx->directories stack. */
static void share_scan_context(share_scan_state_t *ctx,
        const char *dirpath)
{
    DIR *fsdir;
    struct dirent *dp;

    if(strcmp(dirpath, global_incomplete_directory) == 0)
    {
	INFO("Refused to share incomplete download directory [%s]",
	    dirpath);

	ui_send_status_message(NULL, NULL,
	    "Refused to share incomplete download directory '%s'",
	    dirpath);

	return;
    }
    /* DEBUG("scanning directory [%s]", dirpath); */

    fsdir = opendir(dirpath);
    if(fsdir == 0)
    {
        WARNING("%s: %s", dirpath, strerror(errno));
        return;
    }

    while((dp = readdir(fsdir)) != NULL)
    {
        const char *filename = dp->d_name;
        if(share_skip_file(filename))
        {
            /* INFO("- skipping file %s/%s", dirpath, filename); */
            continue;
        }

        char *filepath = share_scan_absolute_path(dirpath, filename);
        if(filepath == NULL)
            continue;

        struct stat stbuf;
        if(stat(filepath, &stbuf) == 0)
        {
            if(S_ISDIR(stbuf.st_mode))
            {
                share_scan_push_directory(ctx, filepath);
            }
            else if(S_ISREG(stbuf.st_mode))
            {
                if(stbuf.st_size == 0)
                    INFO("- skipping zero-sized file '%s'", filepath);
                else
                {
                    share_scan_add_file(ctx, filepath, &stbuf);
                }
            }
            else /* neither directory nor regular file */
            {
                INFO("- skipping file %s (not a regular file)", filename);
            }
        }
        else
        {
            /* stat failed */
            WARNING("%s: %s", filepath, strerror(errno));
        }

        free(filepath);
    }
    closedir(fsdir);
}

static void share_scan_event(int fd, short why, void *user_data)
{
    share_scan_state_t *ctx = user_data;
    int i;

    /* Scan 5 directories in each event. */
    for(i = 0; i < 5; i++)
    {
        share_scan_directory_t *d = LIST_FIRST(&ctx->directories);
        if(d == NULL)
        {
            INFO("Done scanning directory [%s]", ctx->mp->local_root);
	    INFO("bloom filter is %.1f%% filled",
		bloom_filled_percent(ctx->share->bloom));
            nc_send_share_scan_finished_notification(nc_default(),
                    ctx->mp->local_root);
            ctx->share->uptodate = 0;
            ctx->mp->scan_in_progress = 0;
            free(ctx);

	    ctx->share->scanning--;
	    return_if_fail(ctx->share->scanning >= 0);

            return;
        }

        char *dirpath = d->dirpath;
        LIST_REMOVE(d, link);
        free(d);

        share_scan_context(ctx, dirpath);
        free(dirpath);
    }

    share_scan_schedule_event(ctx);
}

static void share_scan_schedule_event(share_scan_state_t *ctx)
{
    if(event_initialized(&ctx->ev))
    {
        event_del(&ctx->ev);
    }
    else
    {
        evtimer_set(&ctx->ev, share_scan_event, ctx);
    }

    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
    event_add(&ctx->ev, &tv);
}

int share_scan(share_t *share, share_mountpoint_t *mp)
{
    return_val_if_fail(share, -1);
    return_val_if_fail(mp, -1);
    return_val_if_fail(mp->scan_in_progress == 0, -1);

    /* Keep a counter to indicate for the myinfo update event that
     * we should wait until rescanning is done. Otherwise we risk
     * sending out a too low share size that gets us kicked.
     */
    share->scanning++;

    share_scan_state_t *ctx = calloc(1, sizeof(share_scan_state_t));

    LIST_INIT(&ctx->directories);
    ctx->share = share;
    ctx->mp = mp;

    /* reset mountpoint statistics */
    memset(&mp->stats, 0, sizeof(share_stats_t));
    mp->scan_in_progress = 1;

    share_scan_push_directory(ctx, mp->local_root);
    share_scan_schedule_event(ctx);

    return 0;
}

