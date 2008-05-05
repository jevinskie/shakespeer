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

#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "globals.h"
#include "xstr.h"
#include "dbenv.h"
#include "tthdb.h"

#define TTH_DB_FILENAME "tth.db"

DB *tth_db = NULL;
static DB *tth_inode_db = NULL;

#if DB_VERSION_MAJOR < 4
# error "Wrong version of Berkeley DB"
#endif

int tthdb_close(void)
{
    int rc = 0;

    rc += close_db(&tth_db, "tth");
    rc += close_db(&tth_inode_db, "tth_inode");

    return rc;
}

int tthdb_open(void)
{
    g_debug("opening database %s", TTH_DB_FILENAME);

    /* open tth database */
    if(open_database(&tth_db, TTH_DB_FILENAME, "tth", DB_HASH, 0) != 0)
    {
        return -1;
    }

    /* open tth_inode database */
    if(open_database(&tth_inode_db, TTH_DB_FILENAME, "tth_inode",
                DB_HASH, 0) != 0)
    {
        return -1;
    }

    return 0;
}

int tthdb_remove(const char *tth)
{
    return_val_if_fail(tth_db && tth_inode_db, -1);
    return_val_if_fail(tth, -1);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)tth;
    key.size = strlen(tth) + 1;

    int rc = tth_db->del(tth_db, NULL, &key, 0);
    if(rc != 0)
    {
        g_warning("failed to delete TTH [%s]: %s", tth, db_strerror(rc));
        return -1;
    }

    return 0;
}

int tthdb_remove_inode(uint64_t inode)
{
    return_val_if_fail(tth_db && tth_inode_db, -1);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = &inode;
    key.size = sizeof(uint64_t);

    int rc = tth_inode_db->del(tth_inode_db, NULL, &key, 0);
    if(rc != 0)
    {
        g_warning("failed to delete inode %llu: %s",
                inode, db_strerror(rc));
        return -1;
    }

    return 0;
}

static int tthdb_put(const char *tth, struct tthdb_data *tthdata, int flags)
{
    return_val_if_fail(tth_db && tth_inode_db, -1);
    return_val_if_fail(tth, -1);
    return_val_if_fail(tthdata, -1);
    return_val_if_fail(tthdata->leafdata_len > 0, -1);
    return_val_if_fail(tthdata->inode > 0ULL, -1);
    return_val_if_fail(tthdata->size > 0ULL, -1);

    DBT key, val;
    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    key.data = (void *)tth;
    key.size = strlen(tth) + 1; /* store terminating nul */

    val.data = tthdata;
    val.size = sizeof(struct tthdb_data) + tthdata->leafdata_len;

    DB_TXN *txn;
    return_val_if_fail(db_transaction(&txn) == 0, -1);

    int rc = tth_db->put(tth_db, txn, &key, &val, flags);

    if(rc == 0 || rc == DB_KEYEXIST)
    {
        DBT key, val;
        memset(&key, 0, sizeof(DBT));
        memset(&val, 0, sizeof(DBT));

        key.data = &tthdata->inode;
        key.size = sizeof(uint64_t);

        tth_inode_t ti;
        ti.size = tthdata->size;
        ti.mtime = tthdata->mtime;
        strlcpy(ti.tth, tth, sizeof(ti.tth));

        val.data = &ti;
        val.size = sizeof(tth_inode_t);

        int rc2 = tth_inode_db->put(tth_inode_db, txn, &key, &val, 0);
        if(rc2 != 0)
        {
            g_warning("Failed to add inode: %s", db_strerror(rc2));
        }
        else if(rc == DB_KEYEXIST)
            rc2 = DB_KEYEXIST;

        rc = rc2;
    }

    if(rc == 0 || rc == DB_KEYEXIST)
    {
        txn->commit(txn, 0);
    }
    else
    {
        g_warning("aborting transaction");
        txn->abort(txn);
    }

    return rc;
}

int tthdb_update(const char *tth, struct tthdb_data *tthdata)
{
    /* g_debug("updating tth [%s] with inode %llu", tth, tthdata->inode); */

    int rc = tthdb_put(tth, tthdata, 0);

    if(rc != 0)
    {
        g_warning("failed to update tth: %s", db_strerror(rc));
        return -1;
    }

    return 0;
}

int tthdb_add(const char *tth, struct tthdb_data *tthdata)
{
    /* g_debug("adding tth [%s] with inode %llu", tth, tthdata->inode); */

    int rc = tthdb_put(tth, tthdata, DB_NOOVERWRITE);

    if(rc != 0 && rc != DB_KEYEXIST)
    {
        g_warning("failed to add tth: %s", db_strerror(rc));
        return -1;
    }

    return rc == DB_KEYEXIST ? 1 : 0;
}

struct tthdb_data *tthdb_lookup(const char *tth)
{
    return_val_if_fail(tth_db && tth_inode_db, NULL);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)tth;
    key.size = strlen(tth) + 1;

    DBT val;
    memset(&val, 0, sizeof(DBT));

    int rc = tth_db->get(tth_db, NULL, &key, &val, 0);
    if(rc == 0)
    {
        return val.data;
    }
    else if(rc != DB_NOTFOUND)
    {
        g_warning("failed to get tth data: %s", db_strerror(rc));
    }

    return NULL;
}

tth_inode_t *tthdb_lookup_inode(uint64_t inode)
{
    return_val_if_fail(tth_db && tth_inode_db, NULL);

    DBT key, val;
    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    key.data = &inode;
    key.size = sizeof(uint64_t);

    int rc = tth_inode_db->get(tth_inode_db, NULL, &key, &val, 0);
    if(rc == 0)
    {
        return val.data;
    }
    else if(rc != DB_NOTFOUND)
    {
        g_warning("failed to get tth inode data: %s", db_strerror(rc));
    }

    return NULL;
}

struct tthdb_data *tthdb_lookup_by_inode(uint64_t inode)
{
    tth_inode_t *ti = tthdb_lookup_inode(inode);
    if(ti)
    {
        return tthdb_lookup(ti->tth);
    }

    return NULL;
}

char *tthdb_get_tth_by_inode(uint64_t inode)
{
    tth_inode_t *ti = tthdb_lookup_inode(inode);
    if(ti)
        return ti->tth;

    return NULL;
}

#ifdef TEST

#include "unit_test.h"

int test_add(uint64_t inode,
        uint64_t size, time_t mtime,
        char *tth,
        unsigned leafdata_len, void *leafdata)
{
    struct tthdb_data *td = calloc(1,
            sizeof(struct tthdb_data) + leafdata_len);

    td->inode = inode;
    td->size = size;
    td->mtime = mtime;
    td->leafdata_len = leafdata_len;
    memcpy(td->leafdata, leafdata, leafdata_len);

    int rc = tthdb_add(tth, td);
    free(td);

    return rc;
}

struct
{
    uint64_t inode;
    uint64_t size;
    time_t mtime;
    char *tth;
    unsigned leafdata_len;
    char *leafdata;
} test_data[] =
{
    {25770216759ULL, 23ULL, 1147701096,
        "X7ZSCHG6JHDTCITHAL6COZYVA2OEQF65GIJCEZA",
        24, "123456789012345678901234"},
    {25770216120ULL, 123456789ULL, 1147701097,
        "52RTKFZCHGND7DNQJAL5F24U3BSVT5MGI332D3I",
        48, "123456789012345678901234567890123456789012345678"},
    {25770216121ULL, 12345ULL, 1147701098,
        "52RTKFZCHGND7DNQJAL5F24U3BSVT5MGI332D3J",
        48, "123456789012345678901234567890123456789012345679"},
    {12334516121ULL, 23445ULL, 1147704564,
        "ADFGSDFGSDFG7DNQJAL5F24U3BSVT5MGI332D3J",
        48, "ADSADSFASDF2345678901234567890123456789012345679"},
    {0, 0, 0, NULL, 0, NULL}
};

int main(void)
{
    global_working_directory = "/tmp/sp-tthdb-test.d";
    sp_log_set_level("debug");
    system("/bin/rm -rf /tmp/sp-tthdb-test.d");
    system("mkdir /tmp/sp-tthdb-test.d");

    fail_unless(tthdb_open() == 0);

    int i;
    for(i = 0; test_data[i].tth; i++)
    {
        int rc = test_add(test_data[i].inode,
                test_data[i].size, test_data[i].mtime,
                test_data[i].tth,
                test_data[i].leafdata_len, test_data[i].leafdata);
        fail_unless(rc == 0);
    }

    fail_unless(tth_db);
    fail_unless(tth_inode_db);
    fail_unless(tthdb_close() == 0);

    fail_unless(tthdb_open() == 0);

    for(i = 0; test_data[i].tth; i++)
    {
        g_debug("inode %lli, tth [%s]",
                test_data[i].inode, test_data[i].tth);

        char *tth = tthdb_get_tth_by_inode(test_data[i].inode);
        fail_unless(tth);
        fail_unless(strcmp(tth, test_data[i].tth) == 0);

        struct tthdb_data *d = tthdb_lookup(test_data[i].tth);
        fail_unless(d);

        struct tthdb_data *d2 = tthdb_lookup_by_inode(test_data[i].inode);
        fail_unless(d2);

        fail_unless(d->leafdata_len == d2->leafdata_len);
        fail_unless(memcmp(d, d2,
                    sizeof(struct tthdb_data) + d->leafdata_len) == 0);

        fail_unless(d->inode == test_data[i].inode);
        fail_unless(d->mtime == test_data[i].mtime);
        fail_unless(d->size == test_data[i].size);
        fail_unless(d->leafdata_len == test_data[i].leafdata_len);
    }

    /* Add a different TTH to a pre-existing inode. */
    int rc = test_add(test_data[0].inode,
            test_data[0].size, test_data[0].mtime,
            "BBBBBBBBBBBBBDNQJAL5F24U3BSVT5MGI332D3J",
            48, "BBBBBBBBB012345678901234567890123456789012345679");
    fail_unless(rc == 0);

    /* Make sure the new TTH has replaced the old one. */
    const char *tth = tthdb_get_tth_by_inode(test_data[0].inode);
    fail_unless(tth);
    fail_unless(strcmp(tth,
                "BBBBBBBBBBBBBDNQJAL5F24U3BSVT5MGI332D3J") == 0);
    /* However, the old TTH is still available. */
    struct tthdb_data *td = tthdb_lookup(test_data[0].tth);
    fail_unless(td);
    fail_unless(td->inode == test_data[0].inode);

    /* Test removing entries. */
    fail_unless(tthdb_lookup(test_data[1].tth) != NULL);
    fail_unless(tthdb_remove(test_data[1].tth) == 0);
    fail_unless(tthdb_lookup(test_data[1].tth) == NULL);

    fail_unless(tthdb_lookup_inode(test_data[1].inode) != NULL);
    fail_unless(tthdb_remove_inode(test_data[1].inode) == 0);
    fail_unless(tthdb_lookup_inode(test_data[1].inode) == NULL);

    /* Add a duplicate inode */
    time_t dup_mtime = test_data[2].mtime + 17;
    rc = test_add(1008806325121417935ULL,
            test_data[2].size, dup_mtime,
            test_data[2].tth,
            test_data[2].leafdata_len, test_data[2].leafdata);
    fail_unless(rc == 1); /* TTH already exists */

    /* We should be able to lookup the duplicate inode. */
    tth_inode_t *ti = tthdb_lookup_inode(1008806325121417935ULL);
    fail_unless(ti);
    fail_unless(ti->size == test_data[2].size);
    fail_unless(ti->mtime == dup_mtime);

    fail_unless(tthdb_close() == 0);
    system("/bin/rm -rf /tmp/sp-tthdb-test.d");

    return 0;
}

#endif

