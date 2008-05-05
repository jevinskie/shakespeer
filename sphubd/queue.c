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

#define _GNU_SOURCE /* needed for asprintf */

#include <db.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "queue.h"
#include "xstr.h"
#include "log.h"
#include "notifications.h"
#include "dbenv.h"

extern DB *queue_target_db;
extern DB *queue_source_db;
extern DB *queue_filelist_db;
extern DB *queue_directory_db;

queue_t *queue_get_next_source_for_nick(const char *nick)
{
    queue_t *queue = NULL;
    return_val_if_fail(nick, NULL);

    /* first check if there is a filelist to download */
    queue_filelist_t *qf = queue_lookup_filelist(nick);
    if(qf && (qf->flags & QUEUE_TARGET_ACTIVE) == 0)
    {
        /* fill in a queue_t from the qf */
        queue = calloc(1, sizeof(queue_t));
        queue->nick = xstrdup(nick);
        queue->is_filelist = true;
        queue->auto_matched = ((qf->flags & QUEUE_TARGET_AUTO_MATCHED)
                == QUEUE_TARGET_AUTO_MATCHED);
        return queue;
    }

    /* next check if there is a directory that needs to be resolved */
    queue_directory_t *qd = queue_db_lookup_unresolved_directory_by_nick(nick);
    if(qd)
    {
        /* fill in a queue_t from the qf */
        queue = calloc(1, sizeof(queue_t));
        queue->nick = xstrdup(nick);
        queue->is_directory = true;
        queue->target_filename = xstrdup(qd->target_directory);
        queue->source_filename = xstrdup(qd->source_directory);
        return queue;
    }

    /* go through all targets where nick is a source
     */
    DBC *qsc;
    return_val_if_fail(
            queue_source_db->cursor(queue_source_db, NULL, &qsc, 0) == 0, NULL);

    DBT key, val;
    memset(&val, 0, sizeof(DBT));
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)nick;
    key.size = strlen(nick) + 1;

    queue_source_t *qs_candidate = NULL;
    int qt_candidate_priority = 0;
    uint64_t qt_candidate_seq = 0;

    u_int32_t flags = DB_SET;
    while(qsc->c_get(qsc, &key, &val, flags) == 0)
    {
        flags = DB_NEXT;

        if(strcmp(nick, key.data) != 0)
            break;

        queue_source_t *qs = val.data;

        queue_target_t *qt = queue_lookup_target(qs->target_filename);
        if(qt == NULL)
            continue;

        if((qt->flags & QUEUE_TARGET_ACTIVE) == QUEUE_TARGET_ACTIVE)
            /* skip targets already active */
            continue;

        if(qt->priority == 0)
            /* skip paused targets */
            continue;

        if(qs_candidate == NULL ||
           qt->priority > qt_candidate_priority ||
           qt->seq < qt_candidate_seq)
        {
            free(qs_candidate);
            qs_candidate = malloc(sizeof(queue_source_t));
            memcpy(qs_candidate, qs, sizeof(queue_source_t));
            qt_candidate_priority = qt->priority;
            qt_candidate_seq = qt->seq;
        }
    }

    qsc->c_close(qsc);

    if(qs_candidate)
    {
        queue_target_t *qt = queue_lookup_target(qs_candidate->target_filename);

        queue = calloc(1, sizeof(queue_t));
        queue->nick = xstrdup(nick);
        queue->source_filename = xstrdup(qs_candidate->source_filename);
        queue->target_filename = xstrdup(qs_candidate->target_filename);
        queue->tth = xstrdup(qt->tth);
        queue->size = qt->size;
        queue->is_filelist = false;
        queue->auto_matched = ((qt->flags & QUEUE_TARGET_AUTO_MATCHED)
                == QUEUE_TARGET_AUTO_MATCHED);

        free(qs_candidate);
    }

    return queue;
}

void queue_free(queue_t *queue)
{
    if(queue)
    {
        free(queue->nick);
        free(queue->target_filename);
        free(queue->source_filename);
        free(queue->tth);
        free(queue);
    }
}

int queue_has_source_for_nick(const char *nick)
{
    queue_t *queue = queue_get_next_source_for_nick(nick);
    g_debug("queue = %p", queue);
    if(queue)
    {
        queue_free(queue);
        return 1;
    }
    return 0;
}

/* Add a file to the download queue. Adds both a target and a source.
 */
int queue_add_internal(const char *nick, const char *source_filename,
        uint64_t size, const char *target_filename,
        const char *tth, int auto_matched_filelist,
        const char *target_directory)
{
    return_val_if_fail(nick, -1);
    return_val_if_fail(target_filename, -1);
    return_val_if_fail(source_filename, -1);

    /* ignore zero-sized files */
    if(size == 0)
    {
        g_message("Ignoring zero-sized file [%s]", target_filename);
        return 0;
    }

    /* add a queue target
     *
     * first lookup if there already exists a target
     */

    queue_target_t *qt = NULL;
    if(tth && *tth)
    {
        /* No need to match on the filename, TTH (and size?) is enough.
         *
         * This way this new target will just be added as a new source
         * instead of a new target.
         */
        qt = queue_lookup_target_by_tth(tth);
        if(qt && qt->size != size)
        {
            /* Size must also match */
            g_warning("TTH matches but size doesn't");
            qt = NULL;
        }
    }
    else
    {
        qt = queue_lookup_target(target_filename);
        if(qt && (qt->size != size || qt->tth[0] == 0))
        {
            /* Size must also match, and there should be no TTH (?) */
            qt = NULL;
        }
    }

    unsigned int default_priority = 3;

    /* FIXME: set priority based on filesize */

    if(qt == NULL)
    {
        queue_target_t new_qt;
        memset(&new_qt, 0, sizeof(queue_target_t));
        qt = &new_qt;

        xstrlcpy(qt->filename, target_filename, QUEUE_TARGET_MAXPATH);
        qt->size = size;
        if(tth)
            xstrlcpy(qt->tth, tth, sizeof(qt->tth));
        time(&qt->ctime);
        qt->priority = default_priority;

        if(target_directory)
            xstrlcpy(qt->target_directory, target_directory, QUEUE_TARGET_MAXPATH);

        if(queue_add_target(qt) != 0)
        {
            return -1;
        }

        /* notify UI:s */
        nc_send_queue_target_added_notification(nc_default(),
                qt->filename, qt->size, qt->tth, qt->priority);
    }

    /* setup a source and add it to the queue */

    queue_source_t qs;
    memset(&qs, 0, sizeof(queue_source_t));
    xstrlcpy(qs.target_filename, qt->filename, QUEUE_TARGET_MAXPATH);
    xstrlcpy(qs.source_filename, source_filename, QUEUE_TARGET_MAXPATH);

    if(queue_add_source(nick, &qs) != 0)
    {
        return -1;
    }

    /* notify ui:s
     */
    nc_send_queue_source_added_notification(nc_default(),
            target_filename, nick, source_filename);

    return 0;
}

int queue_add(const char *nick, const char *source_filename,
        uint64_t size, const char *target_filename, const char *tth)
{
    return_val_if_fail(nick, -1);
    return_val_if_fail(target_filename, -1);
    return_val_if_fail(source_filename, -1);
    return queue_add_internal(nick, source_filename, size, target_filename,
            tth, 0, NULL);
}

int queue_add_filelist(const char *nick, int auto_matched_filelist)
{
    return_val_if_fail(nick, -1);

    queue_filelist_t qf;
    memset(&qf, 0, sizeof(queue_filelist_t));
    qf.priority = 5;
    if(auto_matched_filelist)
        qf.flags |= QUEUE_TARGET_AUTO_MATCHED;

#if 0
        if(is_filelist && auto_matched_filelist == 0 && \
           (qt->flags & QUEUE_TARGET_AUTO_MATCHED) == QUEUE_TARGET_AUTO_MATCHED)
        {
            /* If a filelist target has been added by the auto-search
             * feature, and before the download is complete the user
             * decided to download it manually, reset the auto_matched flag
             * so it is presented to the UI. */
            qt->flags &= ~QUEUE_TARGET_AUTO_MATCHED;
            queue_update_target(qt);
        }
#endif

    queue_filelist_t *qfx = queue_lookup_filelist(nick);

    int rc = queue_db_add_filelist(nick, &qf);
    if(rc == 0 && qfx == NULL)
    {
        nc_send_filelist_added_notification(nc_default(), nick, qf.priority);
    }

    return rc;
}

/* remove all sources with nick */
int queue_remove_nick(const char *nick)
{
    return_val_if_fail(nick, -1);

    return queue_remove_sources_by_nick(nick);
}

/* remove nick as a source for the target filename */
int queue_remove_source(const char *target_filename, const char *nick)
{
    return_val_if_fail(target_filename, -1);
    return_val_if_fail(nick, -1);

    DB_TXN *txn;
    return_val_if_fail(db_transaction(&txn) == 0, -1);

    DBC *qsc;
    queue_source_db->cursor(queue_source_db, txn, &qsc, 0);
    txn_return_val_if_fail(qsc, -1);

    DBT key, val;
    memset(&val, 0, sizeof(DBT));
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)nick;
    key.size = strlen(nick) + 1;

    u_int32_t flags = DB_SET;
    while(qsc->c_get(qsc, &key, &val, flags) == 0)
    {
        flags = DB_NEXT;

        if(strcmp(nick, key.data) != 0)
            break;

        queue_source_t *qs = val.data;
        if(strcmp(target_filename, qs->target_filename) == 0)
        {
            g_debug("removing source [%s], target [%s]",
                    nick, qs->target_filename);
            if(qsc->c_del(qsc, 0) != 0)
	    {
		g_warning("failed to delete entry");
		qsc->c_close(qsc);
		txn->abort(txn);
		return -1;
	    }

            nc_send_queue_source_removed_notification(nc_default(),
                    qs->target_filename, nick);

            break; /* there should be only one (nick, target) pair */
        }
    }

    txn_return_val_if_fail(qsc->c_close(qsc) == 0, -1);
    return_val_if_fail(txn->commit(txn, 0), -1);

    return 0;
}

void queue_set_target_active(queue_t *queue, int flag)
{
    return_if_fail(queue);
    return_if_fail(queue->is_filelist == 0);
    return_if_fail(queue->is_directory == 0);
    return_if_fail(queue->target_filename);

    queue_target_t *qt = queue_lookup_target(queue->target_filename);
    return_if_fail(qt);

    if(flag)
        qt->flags |= QUEUE_TARGET_ACTIVE;
    else
        qt->flags &= ~QUEUE_TARGET_ACTIVE;

    queue_update_target(qt);
}

void queue_set_filelist_active(queue_t *queue, int flag)
{
    return_if_fail(queue);
    return_if_fail(queue->is_filelist == 1);

    queue_filelist_t *qf = queue_lookup_filelist(queue->nick);
    return_if_fail(qf);

    if(flag)
        qf->flags |= QUEUE_TARGET_ACTIVE;
    else
        qf->flags &= ~QUEUE_TARGET_ACTIVE;

    queue_update_filelist(queue->nick, qf);
}

void queue_set_size(queue_t *queue, uint64_t size)
{
    return_if_fail(queue);
    return_if_fail(queue->target_filename);
    return_if_fail(queue->is_filelist == 0);
    return_if_fail(queue->is_directory == 0);

    queue_target_t *qt = queue_lookup_target(queue->target_filename);
    return_if_fail(qt);

    qt->size = size;
    if(queue_update_target(qt) == 0)
        queue->size = size;
}

int queue_set_priority(const char *target_filename, unsigned int priority)
{
    if(priority > 5)
    {
        g_warning("called with invalid priority %i", priority);
        return -1;
    }

    queue_target_t *qt = queue_lookup_target(target_filename);
    if(qt)
    {
        if(qt->priority != priority)
        {
            qt->priority = priority;
            if(queue_update_target(qt) == 0)
            {
                nc_send_queue_priority_changed_notification(nc_default(),
                        target_filename, priority);
            }
        }
        else
        {
            g_info("target [%s] already has priority %i", target_filename, priority);
        }
    }

    return 0;
}

void queue_send_to_ui(void)
{
    /* Send all filelists
     */
    DBC *qfc;
    queue_filelist_db->cursor(queue_filelist_db, NULL, &qfc, 0);

    DBT key, val;
    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    while(qfc->c_get(qfc, &key, &val, DB_NEXT) == 0)
    {
        queue_filelist_t *qf = val.data;
        nc_send_filelist_added_notification(nc_default(),
                (char *)key.data, qf->priority);
    }
    qfc->c_close(qfc);

    /* Send all directories
     */
    DBC *qdc;
    queue_directory_db->cursor(queue_directory_db, NULL, &qdc, 0);

    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    while(qdc->c_get(qdc, &key, &val, DB_NEXT) == 0)
    {
        queue_directory_t *qd = val.data;
        nc_send_queue_directory_added_notification(nc_default(),
                qd->target_directory, qd->nick);
    }
    qdc->c_close(qdc);

    /* Send all targets
     */
    DBC *qtc;
    queue_target_db->cursor(queue_target_db, NULL, &qtc, 0);

    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    while(qtc->c_get(qtc, &key, &val, DB_NEXT) == 0)
    {
        queue_target_t *qt = val.data;
        nc_send_queue_target_added_notification(nc_default(),
                (char *)key.data, qt->size, qt->tth, qt->priority);
    }
    qtc->c_close(qtc);

    /* Send all sources
     */
    DBC *qsc;
    queue_source_db->cursor(queue_source_db, NULL, &qsc, 0);

    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    while(qsc->c_get(qsc, &key, &val, DB_NEXT) == 0)
    {
        queue_source_t *qs = val.data;

        nc_send_queue_source_added_notification(nc_default(),
                qs->target_filename, (char *)key.data, qs->source_filename);
    }
    qsc->c_close(qsc);
}

#ifdef TEST

#include "globals.h"
#include "unit_test.h"

int got_filelist_notification = 0;
int got_target_removed_notification = 0;

void handle_filelist_added_notification(nc_t *nc, const char *channel,
        nc_filelist_added_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->nick);
    g_debug("added filelist for %s", data->nick);
    fail_unless(strcmp(data->nick, "bar") == 0);
    got_filelist_notification = 1;
}

void handle_queue_target_removed_notification(nc_t *nc, const char *channel,
        nc_queue_target_removed_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->target_filename);
    g_debug("removed target %s", data->target_filename);
    /* fail_unless(strcmp(data->target_directory, "target/directory") == 0); */
    got_target_removed_notification = 1;
}

void test_setup(void)
{
    global_working_directory = "/tmp/sp-queue-test.d";
    system("/bin/rm -rf /tmp/sp-queue-test.d");
    system("mkdir /tmp/sp-queue-test.d");

    fail_unless(queue_init() == 0);

    fail_unless(queue_add("foo", "remote/path/to/file.img", 17471142,
                "file.img", "IP4CTCABTUE6ZHZLFS2OP5W7EMN3LMFS65H7D2Y") == 0);

    got_filelist_notification = 0;
}

void test_teardown(void)
{
    queue_close();
    system("/bin/rm -rf /tmp/sp-queue-test.d");
}

/* Just add a file to the queue and check that we can get it back. */
void test_add_file(void)
{
    test_setup();

    /* set the target paused */
    fail_unless(queue_set_priority("file.img", 0) == 0);

    /* is there anything to download from "foo"? */
    queue_t *q = queue_get_next_source_for_nick("foo");
    /* no, the only queued file is paused */
    fail_unless(q == NULL);

    /* increase the priority */
    fail_unless(queue_set_priority("file.img", 3) == 0);

    q = queue_get_next_source_for_nick("foo");
    fail_unless(q);
    fail_unless(q->nick);
    fail_unless(strcmp(q->nick, "foo") == 0);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote/path/to/file.img") == 0);
    fail_unless(q->target_filename);
    fail_unless(strcmp(q->target_filename, "file.img") == 0);
    fail_unless(q->tth);
    fail_unless(strcmp(q->tth, "IP4CTCABTUE6ZHZLFS2OP5W7EMN3LMFS65H7D2Y") == 0);
    fail_unless(q->size == 17471142ULL);
    fail_unless(q->offset == 0ULL);
    fail_unless(q->is_filelist == 0);
    fail_unless(q->is_directory == 0);
    fail_unless(q->auto_matched == 0);

    /* mark this file as active (ie, it is currently being downloaded) */
    queue_set_target_active(q, 1);
    queue_free(q);

    /* there shouldn't be any more sources for foo, the one and only file is
     * already active */
    fail_unless(queue_has_source_for_nick("foo") == 0);

    test_teardown();

    puts("PASS: queue: adding targets");
}

/* Add another source to the target and check the new source. Also remove the
 * target and verify both sources are empty. */
void test_add_source(void)
{
    test_setup();

    /* The source and target filenames are different, but the size and TTH
     * matches. This should override the filenames, and only add this file as
     * another source to the existing target. */
    fail_unless(queue_add("bar", "another/path/to_the/same-file.img", 17471142,
                "same-file.img", "IP4CTCABTUE6ZHZLFS2OP5W7EMN3LMFS65H7D2Y") == 0);

    /* is there anything to download from "bar"? */
    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->target_filename);
    fail_unless(strcmp(q->target_filename, "file.img") == 0);
    fail_unless(q->source_filename);
    g_debug("source = [%s]", q->source_filename);
    fail_unless(strcmp(q->source_filename,
                "another/path/to_the/same-file.img") == 0);

    /* mark this file as active (ie, it is currently being downloaded) */
    queue_set_target_active(q, 1);

    /* this file is being downloaded from "bar", nothing to do for "foo" */
    fail_unless(queue_has_source_for_nick("foo") == 0);
    queue_free(q);

    got_target_removed_notification = 0;
    fail_unless(queue_remove_target("file.img") == 0);
    fail_unless(got_target_removed_notification == 1);

    /* the target is removed, nothing to do for both sources */
    fail_unless(queue_has_source_for_nick("foo") == 0);
    fail_unless(queue_has_source_for_nick("bar") == 0);

    test_teardown();

    puts("PASS: queue: adding sources");
}

/* Tests that the download order is correct.
 */
void test_queue_order(void)
{
    test_setup();

    fail_unless(queue_add("bar", "remote_file_0", 4096, "local_file_0",
                "ZXCVZXCVZXCVZXCVZXCVP5W7EMN3LMFS65H7D2Y") == 0);
    fail_unless(queue_add("bar", "remote_file_2", 4096, "local_file_2",
                "QWERQWERQWRQWERLFS2OP5W7EMN3LMFS65H7D2Y") == 0);
    fail_unless(queue_add("bar", "remote_file_1", 4096, "local_file_1",
                "ASDFASDFASDFASDFFS2OP5W7EMN3LMFS65H7D2Y") == 0);

    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_0") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("local_file_0") == 0);

    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_2") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("local_file_2") == 0);

    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_1") == 0);
    queue_free(q);

    test_teardown();

    puts("PASS: queue: order");
}

void test_priorities(void)
{
    test_setup();

    fail_unless(queue_add("bar", "remote_file_0", 4096, "local_file_0",
                "ZXCVZXCVZXCVZXCVZXCVP5W7EMN3LMFS65H7D2Y") == 0);
    fail_unless(queue_add("bar", "remote_file_2", 4096, "local_file_2",
                "QWERQWERQWRQWERLFS2OP5W7EMN3LMFS65H7D2Y") == 0);
    fail_unless(queue_add("bar", "remote_file_1", 4096, "local_file_1",
                "ASDFASDFASDFASDFFS2OP5W7EMN3LMFS65H7D2Y") == 0);

    fail_unless(queue_set_priority("local_file_2", 4) == 0);
    fail_unless(queue_set_priority("local_file_1", 2) == 0);
    fail_unless(queue_set_priority("local_file_0", 1) == 0);

    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_2") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("local_file_2") == 0);

    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_1") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("local_file_1") == 0);

    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_0") == 0);
    queue_free(q);

    test_teardown();

    puts("PASS: queue: priorities");
}

void test_filelist_dups(void)
{
    test_setup();

    fail_unless(queue_add_filelist("bar", 0) == 0);
    fail_unless(got_filelist_notification == 1);

    /* check that we can get the filelist */
    queue_filelist_t *qf = queue_lookup_filelist("bar");
    fail_unless(qf);

    /* If we add the filelist again, we should not get a duplicate notification */
    got_filelist_notification = 0;
    fail_unless(queue_add_filelist("bar", 0) == 0);
    fail_unless(got_filelist_notification == 0);

    test_teardown();
    puts("PASS: queue: filelist duplicates");
}

int main(void)
{
    sp_log_set_level("debug");

    nc_add_filelist_added_observer(nc_default(),
            handle_filelist_added_notification, NULL);
    nc_add_queue_target_removed_observer(nc_default(),
            handle_queue_target_removed_notification, NULL);

    test_add_file();
    test_add_source();
    test_queue_order();
    test_priorities();
    test_filelist_dups();

    return 0;
}

#endif

