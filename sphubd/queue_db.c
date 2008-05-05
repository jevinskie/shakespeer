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

#include <db.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "xstr.h"

#include "globals.h"
#include "log.h"
#include "queue.h"
#include "notifications.h"
#include "dbenv.h"

#define QUEUE_DB_FILENAME "queue.db"

DB *queue_target_db = NULL;
DB *queue_source_db = NULL;
DB *queue_tth_db = NULL;
DB *queue_filelist_db = NULL;
DB *queue_directory_db = NULL;
DB *queue_directory_nick_db = NULL;
DB *queue_sequence_db = NULL;

static int queue_target_get_tth(DB *sdb, const DBT *pkey,
        const DBT *pdata, DBT *skey)
{
    queue_target_t *qt = (queue_target_t *)pdata->data;

    memset(skey, 0, sizeof(DBT));
    skey->data = (void *)qt->tth;
    skey->size = qt->tth[0] ? 40 : 0; /* strlen(qt->tth) + 1; */

    return 0;
}

static int queue_directory_get_nick(DB *sdb, const DBT *pkey,
        const DBT *pdata, DBT *skey)
{
    queue_directory_t *qt = (queue_directory_t *)pdata->data;

    memset(skey, 0, sizeof(DBT));
    skey->data = (void *)qt->nick;
    skey->size = strlen(qt->nick) + 1;

    return 0;
}

/* resets active state for targets */
static int queue_reset_db(void)
{
    DBC *qtc;
    queue_target_db->cursor(queue_target_db, NULL, &qtc, 0);
    return_val_if_fail(qtc, -1);

    DBT key, val;
    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    while(qtc->c_get(qtc, &key, &val, DB_NEXT) == 0)
    {
        queue_target_t *qt = val.data;
        qt->flags &= ~QUEUE_TARGET_ACTIVE;
        int rc = qtc->c_put(qtc, &key, &val, DB_CURRENT);
        if(rc != 0)
        {
            g_warning("update: %s", db_strerror(rc));
        }
    }

    qtc->c_close(qtc);


    DBC *qfc;
    queue_filelist_db->cursor(queue_filelist_db, NULL, &qfc, 0);
    return_val_if_fail(qfc, -1);

    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    while(qfc->c_get(qfc, &key, &val, DB_NEXT) == 0)
    {
        queue_filelist_t *qf = val.data;
        qf->flags &= ~QUEUE_TARGET_ACTIVE;
        int rc = qfc->c_put(qfc, &key, &val, DB_CURRENT);
        if(rc != 0)
        {
            g_warning("update: %s", db_strerror(rc));
        }
    }

    qfc->c_close(qfc);

    return 0;
}

static int queue_open_db(void)
{
    /* open target database */
    if(open_database(&queue_target_db, QUEUE_DB_FILENAME, "target",
                DB_BTREE, 0) != 0)
        return -1;
    return_val_if_fail(queue_target_db, -1);

    /* open sequence database */
    if(open_database(&queue_sequence_db, QUEUE_DB_FILENAME, "sequence",
                DB_BTREE, 0) != 0)
    {
        queue_target_db->close(queue_target_db, 0);
        queue_target_db = NULL;
        return -1;
    }

    /* open source database */
    if(open_database(&queue_source_db, QUEUE_DB_FILENAME, "source",
               DB_BTREE, DB_DUPSORT) != 0)
    {
        queue_sequence_db->close(queue_sequence_db, 0);
        queue_sequence_db = NULL;
        queue_target_db->close(queue_target_db, 0);
        queue_target_db = NULL;
        return -1;
    }

    /* open filelist database */
    if(open_database(&queue_filelist_db, QUEUE_DB_FILENAME, "filelist",
                DB_BTREE, 0) != 0)
    {
        queue_source_db->close(queue_source_db, 0);
        queue_source_db = NULL;
        queue_sequence_db->close(queue_sequence_db, 0);
        queue_sequence_db = NULL;
        queue_target_db->close(queue_target_db, 0);
        queue_target_db = NULL;
        return -1;
    }

    /* open directory database */
    if(open_database(&queue_directory_db, QUEUE_DB_FILENAME, "directory",
                DB_BTREE, 0) != 0)
    {
        queue_filelist_db->close(queue_filelist_db, 0);
        queue_filelist_db = NULL;
        queue_source_db->close(queue_source_db, 0);
        queue_source_db = NULL;
        queue_sequence_db->close(queue_sequence_db, 0);
        queue_sequence_db = NULL;
        queue_target_db->close(queue_target_db, 0);
        queue_target_db = NULL;
        return -1;
    }

    /* open secondary database to queue_target_db, adds index by TTH */
    if(open_database(&queue_tth_db, QUEUE_DB_FILENAME, "tth_target",
                DB_BTREE, DB_DUPSORT) == 0)
    {
        int rc = queue_target_db->associate(queue_target_db,
                NULL, queue_tth_db, queue_target_get_tth, DB_AUTO_COMMIT);
        if(rc != 0)
            g_warning("tth<->target associate failed: %s", db_strerror(rc));
    }
    else
    {
        return -1;
    }

    /* open secondary database to queue_directory_db, adds index by nick */
    if(open_database(&queue_directory_nick_db, QUEUE_DB_FILENAME, "tth_target",
               DB_BTREE, DB_DUPSORT) == 0)
    {
        int rc = queue_directory_db->associate(queue_directory_db,
                NULL, queue_directory_nick_db, queue_directory_get_nick,
                DB_AUTO_COMMIT);
        if(rc != 0)
            g_warning("directory<->nick associate failed: %s", db_strerror(rc));
    }
    else
    {
        return -1;
    }

    return 0;
}

int queue_init(void)
{
    g_message("Initializing queue, using: " DB_VERSION_STRING);

    const char *db_list[] = {"target", "tth_target", "filelist",
        "source", "directory", NULL};
    if(verify_db(QUEUE_DB_FILENAME, db_list) != 0)
    {
        g_warning("Corrupt queue database!");
        backup_db(QUEUE_DB_FILENAME);
    }

    g_debug("opening databases in file %s", QUEUE_DB_FILENAME);
    int rc = queue_open_db();
    if(rc != 0)
    {
        backup_db(QUEUE_DB_FILENAME);
        rc = queue_open_db();
    }

    if(rc == 0)
    {
        queue_reset_db();
    }

    return rc;
}

int queue_close(void)
{
    int rc = 0;

    rc += close_db(&queue_tth_db, "tth_target");
    rc += close_db(&queue_target_db, "target");
    rc += close_db(&queue_source_db, "source");
    rc += close_db(&queue_filelist_db, "filelist");
    rc += close_db(&queue_directory_nick_db, "directory_nick");
    rc += close_db(&queue_directory_db, "directory");
    rc += close_db(&queue_sequence_db, "sequence");

    return rc == 0 ? 0 : -1;
}

static int queue_add_target_overwrite(queue_target_t *qt, int overwrite_flag)
{
    return_val_if_fail(qt, -1);

    DBT val;
    memset(&val, 0, sizeof(DBT));
    val.data = qt;
    val.size = sizeof(queue_target_t);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = qt->filename;
    key.size = strlen(qt->filename) + 1; /* store terminating nul */

    g_debug("filename [%s]", qt->filename);
    g_debug("tth [%s]", qt->tth);

    int rc = queue_target_db->put(queue_target_db,
            NULL, &key, &val, overwrite_flag ? 0 : DB_NOOVERWRITE);

    if(rc != 0)
    {
        g_warning("failed to add queue target: %s", db_strerror(rc));
    }

    return rc;
}

static unsigned long long queue_get_target_sequence(void)
{
    unsigned long long *retp;
    unsigned long long ret;

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)"target";
    key.size = 7;

    DBT val;
    memset(&val, 0, sizeof(DBT));

    int rc = queue_sequence_db->get(queue_sequence_db, NULL, &key, &val, 0);
    if(rc == -1)
    {
        g_warning("failed to get queue target sequence: %s", db_strerror(rc));
        return 0;
    }
    else if(rc == 1) /* FIXME: constant? */
    {
        return 0;
    }

    /* FIXME: endian issue */
    retp = (unsigned long long *)val.data;
    if(retp)
        ret = *retp + 1;
    else
        ret = 1;

    val.data = &ret;
    val.size = sizeof(unsigned long long);

    rc = queue_sequence_db->put(queue_sequence_db, NULL, &key, &val, 0);
    if(rc != 0)
        g_warning("sequence failed: %s", db_strerror(rc));

    return ret;
}

int queue_add_target(queue_target_t *qt)
{
    return_val_if_fail(qt, -1);

    if(qt->tth[0])
    {
        queue_target_t *qt_tth = queue_lookup_target_by_tth(qt->tth);

        /* This is a target with the same TTH already stored. Reject this
         * one. The caller should really check this and only add another
         * source. */
        return_val_if_fail(qt_tth == NULL, -1);
    }

    char *eof = qt->filename + strlen(qt->filename);
    int left = QUEUE_TARGET_MAXPATH - (eof - qt->filename);
    int n = 0;
    int rc;

    return_val_if_fail(left > 0, -1);

    qt->seq = queue_get_target_sequence();
    time(&qt->ctime);

    while(1)
    {
        rc = queue_add_target_overwrite(qt, 0);
        if(rc != DB_KEYEXIST)
            break;
        snprintf(eof, left, "-%u", ++n);
    }

    return rc == 0 ? 0 : -1;
}

int queue_update_target(queue_target_t *qt)
{
    return_val_if_fail(qt, -1);

    int rc = queue_add_target_overwrite(qt, 1);
    return rc == 0 ? 0 : -1;
}

int queue_add_source(const char *nick, queue_source_t *qs)
{
    return_val_if_fail(nick, -1);
    return_val_if_fail(qs, -1);

    DBT val;
    memset(&val, 0, sizeof(DBT));
    val.data = qs;
    val.size = sizeof(queue_source_t);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)nick;
    key.size = strlen(nick) + 1; /* store terminating nul */

    g_debug("adding nick [%s], source [%s] for target [%s]",
            nick, qs->source_filename, qs->target_filename);

    int rc = queue_source_db->put(queue_source_db, NULL, &key, &val, 0);

    if(rc != 0)
    {
        g_warning("failed to add source [%s]: %s", nick, db_strerror(rc));
    }

    return rc;
}

int queue_db_add_filelist(const char *nick, queue_filelist_t *qf)
{
    return_val_if_fail(qf, -1);

    DBT val;
    memset(&val, 0, sizeof(DBT));
    val.data = qf;
    val.size = sizeof(queue_filelist_t);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)nick;
    key.size = strlen(nick) + 1; /* store terminating nul */

    g_debug("adding filelist for nick [%s]", nick);

    /* will overwrite any existing filelist for this nick */
    int rc = queue_filelist_db->put(queue_filelist_db, NULL, &key, &val, 0);

    if(rc != 0)
    {
        g_warning("failed to add queue filelist: %s", db_strerror(rc));
    }

    return rc;
}

int queue_update_filelist(const char *nick, queue_filelist_t *qf)
{
    return queue_db_add_filelist(nick, qf);
}

int queue_remove_filelist(const char *nick)
{
    return_val_if_fail(nick, -1);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)nick;
    key.size = strlen(nick) + 1;

    int rc = queue_filelist_db->del(queue_filelist_db, NULL, &key, 0);
    if(rc != 0)
    {
        g_warning("failed to delete filelist for [%s]: %s",
                nick, db_strerror(rc));
        return -1;
    }

    /* notify ui:s */
    nc_send_filelist_removed_notification(nc_default(), nick);

    return 0;
}

queue_target_t *queue_lookup_target(const char *target_filename)
{
    return_val_if_fail(target_filename, NULL);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)target_filename;
    key.size = strlen(target_filename) + 1;

    DBT val;
    memset(&val, 0, sizeof(DBT));

    int rc = queue_target_db->get(queue_target_db, NULL, &key, &val, 0);
    if(rc == -1)
    {
        g_warning("failed to get queue target data: %s", db_strerror(rc));
        return NULL;
    }
    else if(rc == 1) /* FIXME: constant? */
    {
        return NULL;
    }

    return val.data;
}

queue_target_t *queue_lookup_target_by_tth(const char *tth)
{
    return_val_if_fail(tth, NULL);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)tth;
    key.size = strlen(tth) + 1;

    DBT val;
    memset(&val, 0, sizeof(DBT));

    int rc = queue_tth_db->get(queue_tth_db, NULL, &key, &val, 0);
    if(rc == -1)
    {
        g_warning("failed to lookup queue target data by tth: %s",
                db_strerror(rc));
        return NULL;
    }
    else if(rc == 1) /* FIXME: constant? */
    {
        return NULL;
    }

    return val.data;
}

queue_filelist_t *queue_lookup_filelist(const char *nick)
{
    return_val_if_fail(nick, NULL);

    DBT key, val;
    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    key.data = (void *)nick;
    key.size = strlen(nick) + 1;

    int rc = queue_filelist_db->get(queue_filelist_db, NULL, &key, &val, 0);
    if(rc == -1)
    {
        g_warning("failed to lookup queue filelist data for nick [%s]: %s",
                nick, db_strerror(rc));
        return NULL;
    }
    else if(rc == 1) /* FIXME: constant? */
    {
        return NULL;
    }

    return val.data;
}

int queue_db_remove_target(const char *target_filename)
{
    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)target_filename;
    key.size = strlen(target_filename) + 1;

    int rc = queue_target_db->del(queue_target_db, NULL, &key, 0);
    if(rc != 0)
    {
        g_warning("failed to delete queue target [%s]: %s",
                target_filename, db_strerror(rc));
        return -1;
    }

    /* notify ui:s */
    nc_send_queue_target_removed_notification(nc_default(), target_filename);

    return 0;
}

int queue_remove_target(const char *target_filename)
{
    return_val_if_fail(target_filename, -1);

    queue_directory_t *qd = NULL;
    queue_target_t *qt = queue_lookup_target(target_filename);
    if(qt == NULL)
    {
        g_warning("Target [%s] doesn't exist", target_filename);
    }
    else if(qt->target_directory[0])
    {
        /* this target belongs to a directory download */
        qd = queue_db_lookup_directory(qt->target_directory);
        if(qd)
        {
            qd->nleft--;
        }
        else
        {
            g_warning("Target directory [%s] doesn't exist!?",
                    qt->target_directory);
        }
    }

    if(queue_db_remove_target(target_filename) != 0)
        return -1;

    /* target removed, now remove all its sources */
    queue_remove_sources(target_filename);

    /* If we're downloading a directory, check if the whole directory is
     * complete. */
    if(qd)
    {
        g_debug("directory [%s] has %u files left",
                qt->target_directory, qd->nleft);
        if(qd->nleft == 0)
        {
            queue_db_remove_directory(qt->target_directory);
        }
        else
        {
            queue_db_update_directory(qt->target_directory, qd);
        }
    }

    return 0;
}

/* This is a possibly expensive operation. */
int queue_remove_sources(const char *target_filename)
{
    return_val_if_fail(target_filename, -1);

    g_debug("removing sources for target [%s]", target_filename);

    DBC *qsc;
    queue_source_db->cursor(queue_source_db, NULL, &qsc, 0);
    return_val_if_fail(qsc, -1);

    DBT key, val;
    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    while(qsc->c_get(qsc, &key, &val, DB_NEXT) == 0)
    {
        queue_source_t *qs = val.data;
        if(strcmp(target_filename, qs->target_filename) == 0)
        {
            g_debug("removing source [%s], target [%s]",
                    (char *)key.data, qs->target_filename);
            qsc->c_del(qsc, 0);
        }
    }

    qsc->c_close(qsc);

    return 0;
}

int queue_remove_sources_by_nick(const char *nick)
{
    return_val_if_fail(nick, -1);

    g_debug("removing sources for nick [%s]", nick);

    DBC *qsc;
    queue_source_db->cursor(queue_source_db, NULL, &qsc, 0);
    return_val_if_fail(qsc, -1);

    DBT key, val;
    memset(&val, 0, sizeof(DBT));
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)nick;
    key.size = strlen(nick) + 1;

    u_int32_t flags = DB_SET;
    while(qsc->c_get(qsc, &key, &val, flags) == 0)
    {
        flags = DB_NEXT;

        if(strcmp(key.data, nick) != 0)
            break;
        
        queue_source_t *qs = val.data;
        g_debug("removing source [%s], target [%s]", nick, qs->target_filename);
        qsc->c_del(qsc, 0);

        nc_send_queue_source_removed_notification(nc_default(),
                qs->target_filename, nick);
    }

    qsc->c_close(qsc);

    return 0;
}

queue_target_t *queue_lookup_target_by_nick(const char *nick)
{
    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)nick;
    key.size = strlen(nick) + 1;

    DBT val;
    memset(&val, 0, sizeof(DBT));

    /* This just looks up the _first_ source for nick */
    int rc = queue_source_db->get(queue_source_db, NULL, &key, &val, 0);
    if(rc == 0)
    {
        queue_source_t *qs = val.data;

        queue_target_t *qt = queue_lookup_target(qs->target_filename);
        return qt;
    }

    return NULL;
}

static int queue_db_put_directory(const char *target_directory,
        queue_directory_t *qd)
{
    return_val_if_fail(target_directory, -1);
    return_val_if_fail(qd, -1);

    DBT val;
    memset(&val, 0, sizeof(DBT));
    val.data = qd;
    val.size = sizeof(queue_directory_t);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)target_directory;
    key.size = strlen(target_directory) + 1; /* store terminating nul */

    int rc = queue_directory_db->put(queue_directory_db,
            NULL, &key, &val, 0 /*DB_NOOVERWRITE*/);

    if(rc != 0)
    {
        g_warning("failed to add queue directory: %s", db_strerror(rc));
        return -1;
    }

    return 0;
}

int queue_db_add_directory(const char *target_directory, queue_directory_t *qd)
{
    return_val_if_fail(target_directory, -1);
    return_val_if_fail(qd, -1);

    g_debug("target_directory [%s]", target_directory);
    g_debug("source_directory [%s]", qd->source_directory);
    g_debug("nick [%s]", qd->nick);

    return queue_db_put_directory(target_directory, qd);
}

int queue_db_update_directory(const char *target_directory,
        queue_directory_t *qd)
{
    return queue_db_put_directory(target_directory, qd);
}

int queue_db_remove_directory(const char *target_directory)
{
    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)target_directory;
    key.size = strlen(target_directory) + 1;

    g_debug("removing directory [%s]", target_directory);

    int rc = queue_directory_db->del(queue_directory_db, NULL, &key, 0);
    if(rc != 0)
    {
        g_warning("failed to delete queue directory [%s]: %s",
                target_directory, db_strerror(rc));
        return -1;
    }

    /* notify ui:s */
    nc_send_queue_directory_removed_notification(nc_default(),
            target_directory);

    return 0;
}

queue_directory_t *queue_db_lookup_directory(const char *target_directory)
{
    return_val_if_fail(target_directory, NULL);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)target_directory;
    key.size = strlen(target_directory) + 1;

    DBT val;
    memset(&val, 0, sizeof(DBT));

    int rc = queue_directory_db->get(queue_directory_db, NULL, &key, &val, 0);
    if(rc == -1)
    {
        g_warning("failed to get queue directory data: %s", db_strerror(rc));
        return NULL;
    }
    else if(rc == 1) /* FIXME: constant? */
    {
        return NULL;
    }

    return val.data;
}

queue_directory_t *queue_db_lookup_unresolved_directory_by_nick(const char *nick)
{
    return_val_if_fail(nick, NULL);

    DBC *qdc;
    queue_directory_nick_db->cursor(queue_directory_nick_db, NULL, &qdc, 0);
    return_val_if_fail(qdc, NULL);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)nick;
    key.size = strlen(nick) + 1;

    DBT val;
    memset(&val, 0, sizeof(DBT));

    u_int32_t flags = DB_SET;
    while(qdc->c_get(qdc, &key, &val, flags) == 0)
    {
        flags = DB_NEXT;

        if(strcmp(key.data, nick) != 0)
            break;
        
        queue_directory_t *qd = val.data;
        if(strcmp(nick, qd->nick) == 0 &&
           (qd->flags & QUEUE_DIRECTORY_RESOLVED) == 0)
        {
            qdc->c_close(qdc);
            return qd;
        }
    }

    qdc->c_close(qdc);
    return NULL;
}

#ifdef TEST

#include "unit_test.h"

int main(void)
{
    global_working_directory = "/tmp";
    sp_log_set_level("debug");
    unlink("/tmp/queue.db");

    fail_unless(queue_init() == 0);

    db_seq_t s_cur = queue_get_target_sequence();
    db_seq_t s_next1 = queue_get_target_sequence();
    db_seq_t s_next2 = queue_get_target_sequence();
    db_seq_t s_next3 = queue_get_target_sequence();

    g_debug("sequence: %lli %lli %lli %lli", s_cur, s_next1, s_next2, s_next3);

    fail_unless(s_next1 == s_cur + 1);
    fail_unless(s_next2 == s_cur + 2);
    fail_unless(s_next3 == s_cur + 3);

    queue_target_t qt;
    xstrlcpy(qt.filename, "/target/filename.ext", QUEUE_TARGET_MAXPATH);
    qt.size = 4711ULL;
    xstrlcpy(qt.tth, "ABCDEFGHIJKLMNOPQRTSUVWXYZ0123456789012", 40);
    qt.flags = 0;
    qt.priority = 2;
    fail_unless(queue_add_target(&qt) == 0);

    queue_target_t *pqt;
    fail_unless(queue_lookup_target("/target/unknown.ext") == NULL);
    pqt = queue_lookup_target("/target/filename.ext");
    fail_unless(pqt);
    fail_unless(strcmp(pqt->tth, "ABCDEFGHIJKLMNOPQRTSUVWXYZ0123456789012") == 0);
    fail_unless(pqt->priority == 2);

    qt.priority = 3;
    fail_unless(queue_update_target(&qt) == 0);

    pqt = queue_lookup_target("/target/filename.ext");
    fail_unless(pqt);
    fail_unless(pqt->priority == 3);

    /* Can't add another target with the same TTH. The same filename is OK,
     * will be made unique by appending a -version. */
    fail_unless(queue_add_target(&qt) == -1);
    pqt = queue_lookup_target("/target/filename.ext-1");
    fail_unless(pqt == NULL);

    /* Change the TTH and try again. */
    qt.tth[0] = 'X';
    fail_unless(queue_add_target(&qt) == 0);
    /* queue_add_target should modify the filename if not unique */
    fail_unless(strcmp(qt.filename, "/target/filename.ext-1") == 0);
    pqt = queue_lookup_target("/target/filename.ext-1");
    fail_unless(pqt);

    pqt = queue_lookup_target_by_tth("ABCDEFGHIJKLMNOPQRTSUVWXYZ0123456789012");
    fail_unless(pqt);

    pqt = queue_lookup_target_by_tth("XBCDEFGHIJKLMNOPQRTSUVWXYZ0123456789012");
    fail_unless(pqt);

    pqt = queue_lookup_target_by_tth("YBCDEFGHIJKLMNOPQRTSUVWXYZ0123456789012");
    fail_unless(pqt == NULL);

    /* add a source */
    queue_source_t qs;
    xstrlcpy(qs.target_filename, "/target/filename.ext", QUEUE_TARGET_MAXPATH);
    xstrlcpy(qs.source_filename, "\\source\\filename.ext", QUEUE_TARGET_MAXPATH);
    fail_unless(queue_add_source("nicke_nyfiken", &qs) == 0);

    pqt = queue_lookup_target_by_nick("nicke_nyfiken");
    fail_unless(pqt);
    fail_unless(strcmp(pqt->filename, "/target/filename.ext") == 0);

    /* add another source */
    queue_source_t qs2;
    xstrlcpy(qs2.target_filename, "/target/filename.ext", QUEUE_TARGET_MAXPATH);
    xstrlcpy(qs2.source_filename, "\\blah\\filename.ext", QUEUE_TARGET_MAXPATH);
    fail_unless(queue_add_source("nils", &qs2) == 0);

    pqt = queue_lookup_target_by_nick("nils");
    fail_unless(pqt);
    fail_unless(strcmp(pqt->filename, "/target/filename.ext") == 0);

    fail_unless(queue_remove_sources("/target/filename.ext") == 0);
    fail_unless(queue_lookup_target_by_nick("nils") == NULL);
    fail_unless(queue_lookup_target_by_nick("nicke_nyfiken") == NULL);

    queue_filelist_t qf;
    fail_unless(queue_db_add_filelist("bananen", &qf) == 0);

    fail_unless(queue_remove_target("/target/filename.ext-1") == 0);
    fail_unless(queue_lookup_target("/target/filename.ext-1") == NULL);

    fail_unless(queue_lookup_filelist("bananen") != NULL);

    /* Add a target without a TTH */
    memset(&qt, 0, sizeof(qt));
    xstrlcpy(qt.filename, "/target/file_w/o_TTH", QUEUE_TARGET_MAXPATH);
    qt.size = 4242ULL;
    qt.flags = 0;
    qt.priority = 2;
    fail_unless(queue_add_target(&qt) == 0);

    fail_unless(queue_close() == 0);

    return 0;
}

#endif

