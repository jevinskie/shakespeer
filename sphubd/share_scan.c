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

#define _GNU_SOURCE /* needed for asprintf */

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

#define SHARE_STAT_TO_INODE(st) (uint64_t)(((uint64_t)st->st_dev << 32) | st->st_ino)

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

    /* g_debug("adding file [%s], inode %llu", f->path, f->inode); */

    /* is it already hashed? */
    int already_hashed = 0;
    tth_inode_t *ti = tthdb_lookup_inode(f->inode);

    if(ti == NULL)
    {
        /* unhashed */
    }
    else
    {
        /* hashed or duplicate */

        /* g_debug("Found inode %llu: tth = [%s]", f->inode, ti->tth); */

        struct tthdb_data *td = tthdb_lookup(ti->tth);
        if(td)
        {
            /* g_debug("Found TTH, inode = %llu", td->inode); */
            if(td->inode != f->inode)
            {
                /* g_debug("wrong inode, duplicate"); */
                if(ti->size != f->size || ti->mtime != f->mtime)
                {
                    /* obsolete, unhashed */
                    /* g_debug("obsolete duplicate"); */
                    tthdb_remove_inode(f->inode);
                }
                else
                {
                    /* duplicate, check if the original is shared */
                    share_file_t *original_file =
                        share_lookup_file_by_inode(ctx->share, td->inode);
                    if(original_file)
                    {
                        /* ok, keep as duplicate */
                        g_debug("Setting [%s] as duplicate of inode %llu",
                                f->path, td->inode);
                        f->duplicate_inode = td->inode;

                        /* update the mount statistics */
                        ctx->mp->stats.nduplicates++;
                        ctx->mp->stats.dupsize += f->size;
                    }
                    else
                    {
                        /* original not shared, switch with duplicate */
                        /* g_debug("original not shared, switching"); */
                        td->inode = f->inode;
                        td->mtime = f->mtime;
                        tthdb_update(ti->tth, td);
                        already_hashed = 1;
                    }
                }
            }
            else
            {
                /* g_debug("correct inode, match"); */
                if(td->size != f->size || td->mtime != f->mtime)
                {
                    /* obsolete, unhashed */
                    /* g_debug("obsolete"); */
                    tthdb_remove(ti->tth);
                    tthdb_remove_inode(f->inode);
                }
                else
                {
                    /* hashed */
                    already_hashed = 1;
                }
            }
        }
        else
        {
            /* unhashed */
            tthdb_remove_inode(f->inode);
        }
    }

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

    /* update the mount statistics */
    ctx->mp->stats.ntotfiles++;
    ctx->mp->stats.totsize += f->size;

    /* add it to the inode hash */
    share_add_to_inode_table(ctx->share, f);

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
        g_warning("encoding conversion failed (skipping '%s/%s')",
                /*utf_err->message,*/ dirpath, filename);
        /* g_error_free(utf_err); */
        /* utf_err = 0; */
        return NULL;
    }
#endif

    if(!g_utf8_validate(filename, -1, NULL))
    {
        g_warning("Unknown encoding in filename [%s]", filename);
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
	g_info("Refused to share incomplete download directory [%s]",
	    dirpath);

	ui_send_status_message(NULL, NULL,
	    "Refused to share incomplete download directory '%s'",
	    dirpath);

	return;
    }
    /* g_debug("scanning directory [%s]", dirpath); */

    fsdir = opendir(dirpath);
    if(fsdir == 0)
    {
        g_warning("%s: %s", dirpath, strerror(errno));
        return;
    }

    while((dp = readdir(fsdir)) != NULL)
    {
        const char *filename = dp->d_name;
        if(share_skip_file(filename))
        {
            /* g_message("- skipping file %s/%s", dirpath, filename); */
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
                    g_info("- skipping zero-sized file '%s'", filepath);
                else
                {
                    share_scan_add_file(ctx, filepath, &stbuf);
                }
            }
            else /* neither directory nor regular file */
            {
                g_info("- skipping file %s (not a regular file)", filename);
            }
        }
        else
        {
            /* stat failed */
            g_warning("%s: %s", filepath, strerror(errno));
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
            g_message("Done scanning directory [%s]", ctx->mp->local_root);
            nc_send_share_scan_finished_notification(nc_default(),
                    ctx->mp->local_root);
            ctx->share->uptodate = 0;
            ctx->mp->scan_in_progress = 0;
            free(ctx);
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

