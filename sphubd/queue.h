/*
 * Copyright 2004 Martin Hedenfalk <martin@bzero.se>
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

#ifndef _queue_h_
#define _queue_h_

#define QUEUE_TARGET_ACTIVE 1
#define QUEUE_TARGET_AUTO_MATCHED 2
#define QUEUE_DIRECTORY_RESOLVED 4

#define QUEUE_TARGET_MAXPATH 256
#define QUEUE_SOURCE_MAXNICK 32

#include <time.h>
#include <db.h> /* for db_seq_t */

typedef struct queue_target queue_target_t;
struct queue_target /* indexed by filename */
{
    char filename[QUEUE_TARGET_MAXPATH];
    char target_directory[QUEUE_TARGET_MAXPATH];
    uint64_t size;
    char tth[40];
    unsigned flags;
    time_t ctime;
    int priority;
    uint64_t seq;
};

typedef struct queue_source queue_source_t;
struct queue_source /* indexed by nick */
{
    char target_filename[QUEUE_TARGET_MAXPATH];
    char source_filename[QUEUE_TARGET_MAXPATH];
};

typedef struct queue_filelist queue_filelist_t;
struct queue_filelist /* indexed by nick */
{
    unsigned flags;
    int priority;
};

typedef struct queue_directory queue_directory_t;
struct queue_directory /* indexed by target_directory */
{
    char target_directory[QUEUE_TARGET_MAXPATH];
    char source_directory[QUEUE_TARGET_MAXPATH];
    char nick[QUEUE_SOURCE_MAXNICK];
    unsigned flags;
    unsigned nfiles;
    unsigned nleft;
};

typedef struct queue queue_t;
struct queue
{
    char *nick;
    char *source_filename;
    char *target_filename;
    char *tth;
    uint64_t size;
    uint64_t offset;
    int is_filelist;
    int is_directory;
    int auto_matched;
    int64_t target_id;
    int64_t source_id;
};

int queue_init(void);
int queue_close(void);
void queue_free(queue_t *queue);

int queue_add_target(queue_target_t *qt);
int queue_update_target(queue_target_t *qt);
int queue_db_remove_target(const char *target_filename);
int queue_remove_target(const char *target_filename);
queue_target_t *queue_lookup_target(const char *target_filename);
queue_target_t *queue_lookup_target_by_tth(const char *tth);

int queue_add_source(const char *nick, queue_source_t *qs);
int queue_remove_sources(const char *target_filename);
int queue_remove_sources_by_nick(const char *nick);

int queue_db_add_filelist(const char *nick, queue_filelist_t *qf);
int queue_update_filelist(const char *nick, queue_filelist_t *qf);
int queue_remove_filelist(const char *nick);

int queue_add_filelist(const char *nick, int auto_matched_filelist);

int queue_db_add_directory(const char *target_directory, queue_directory_t *qd);
int queue_db_update_directory(const char *target_directory, queue_directory_t *qd);
queue_directory_t *queue_db_lookup_directory(const char *target_directory);
queue_directory_t *queue_db_lookup_unresolved_directory_by_nick(const char *nick);
int queue_db_remove_directory(const char *target_directory);

queue_filelist_t *queue_lookup_filelist(const char *nick);

int queue_add_internal(const char *nick, const char *remote_filename,
        uint64_t size, const char *local_filename, const char *tth,
        int auto_matched_filelist, const char *target_directory);

void queue_set_size(queue_t *queue, uint64_t size);
void queue_set_target_active(queue_t *queue, int flag);
void queue_set_filelist_active(queue_t *queue, int flag);
queue_t *queue_get_next_source_for_nick(const char *nick);
int queue_has_source_for_nick(const char *nick);
int queue_set_target_filename(queue_t *queue, const char *filename);
void queue_set_source_filename(queue_t *queue, const char *filename);
int queue_remove_directory(const char *target_directory);

void queue_send_to_ui(void);

int queue_remove_source(const char *local_filename, const char *nick);
int queue_add(const char *nick, const char *remote_filename, uint64_t size,
        const char *local_filename, const char *tth);
int queue_add_directory(const char *nick,
        const char *source_directory,
        const char *target_directory);
int queue_remove_nick(const char *nick);
int queue_set_priority(const char *target_filename, unsigned int priority);

/* queue_auto_search.c
 */
void queue_auto_search_init(void);
void queue_schedule_auto_search_sources(int enable);

/* queue_resolve.c
 */
int queue_resolve_directory(const char *nick,
        const char *source_directory,
        const char *target_directory,
        unsigned *nfiles_p);

/* queue_connect.c
 */
typedef int (*queue_connect_callback_t)(const char *nick, void *user_data);

void queue_connect_set_interval(int seconds);
void queue_connect_schedule_trigger(queue_connect_callback_t callback_function);

#endif

