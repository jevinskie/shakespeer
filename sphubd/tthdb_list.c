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

#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "globals.h"
#include "tthdb.h"
#include "log.h"
#include "xstr.h"
#include "dbenv.h"

extern DB *tth_db;

int main(int argc, char **argv)
{
    if(argc > 1)
        global_working_directory = argv[1];
    else
        global_working_directory = "/tmp";

    sp_log_set_level("debug");

    tthdb_open();

    if(tth_db == NULL)
        return 2;

    g_debug("creating cursor");

    /* create a cursor */
    DBC *cursor;
    tth_db->cursor(tth_db, NULL, &cursor, 0);

    int rc = -1;
    while(cursor)
    {
        DBT key;
        DBT val;

        memset(&key, 0, sizeof(DBT));
        memset(&val, 0, sizeof(DBT));

        rc = cursor->c_get(cursor, &key, &val, DB_NEXT);

        if(rc != 0)
            break;

        struct tthdb_data *d = val.data;
        printf("got inode %llu: ", d->inode);

        char *tth = tthdb_get_tth_by_inode(d->inode);

        if(tth)
        {
            printf("%s", tth);
            assert(strncmp(tth, key.data, strlen(tth)) == 0);
        }
        else
        {
            printf("NO TTH FOUND!");
        }
        printf(" size: %llu mtime: %lu\n", d->size, (unsigned long)d->mtime);

    }

    if(rc == -1)
    {
        printf("Failed: %s\n", strerror(errno));
    }

    if(cursor)
        cursor->c_close(cursor);

    tthdb_close();
    close_default_db_environment();

    return 0;
}

