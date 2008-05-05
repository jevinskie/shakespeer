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
#include <db.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "globals.h"
#include "log.h"

static DB_ENV *default_db_env = NULL;

void close_default_db_environment(void)
{
    if(default_db_env)
    {
        int rc = default_db_env->close(default_db_env, 0);
        if(rc == -1)
        {
            g_warning("Failed to close default database environment: %s",
                    db_strerror(rc));
        }
        default_db_env = NULL;
    }
}

DB_ENV *get_default_db_environment(void)
{
    if(default_db_env == NULL)
    {
        g_message("Opening default database environment, home = [%s]",
                global_working_directory);
        int rc = db_env_create(&default_db_env, 0);
        if(rc != 0)
        {
            g_warning("db_env_create: %s", db_strerror(rc));
            return NULL;
        }

        return_val_if_fail(default_db_env, NULL);
        rc = default_db_env->open(default_db_env,
                    global_working_directory,
                    DB_CREATE | DB_INIT_TXN | DB_INIT_LOG |
                    DB_INIT_MPOOL | DB_RECOVER, 0);
        if(rc != 0)
        {
            g_warning("db_env->open: %s", db_strerror(rc));
            close_default_db_environment();
            return NULL;
        }
        default_db_env->set_flags(default_db_env,
                DB_AUTO_COMMIT | DB_TXN_NOSYNC | DB_LOG_AUTOREMOVE, 1);
    }

    return default_db_env;
}

static int verify_db_ordercheck(const char *dbfile, const char *dbname)
{
    DB *db;

    g_debug("checking database %s", dbname);

    db_create(&db, get_default_db_environment(), 0);
    return_val_if_fail(db, -1);
    int rc = db->verify(db, dbfile, dbname, NULL, DB_ORDERCHKONLY);
    if(rc != 0 && rc != DB_NOTFOUND)
    {
        g_warning("%s[%s]: %s", dbfile, dbname, db_strerror(rc));
        return -1;
    }

    return 0;
}

int verify_db(const char *dbfile, const char *db_list[])
{
    return_val_if_fail(dbfile, -1);
    return_val_if_fail(db_list, -1);

    g_debug("checking databases in file %s", dbfile);

    int fd = open(".", O_RDONLY);
    chdir(global_working_directory);

    DB *db;
    db_create(&db, get_default_db_environment(), 0);
    return_val_if_fail(db, -1);
    int rc = db->verify(db, dbfile, NULL, NULL, DB_NOORDERCHK);

    if(rc == ENOENT)
    {
        fchdir(fd);
        return 0;
    }

    if(rc != 0)
    {
        g_warning("%s: %s", dbfile, db_strerror(rc));
        fchdir(fd);
        return -1;
    }

    int i;
    for(i = 0; db_list[i]; i++)
    {
        if(verify_db_ordercheck(dbfile, db_list[i]) != 0)
        {
            fchdir(fd);
            return -1;
        }
    }

    g_debug("Database verified OK");
    fchdir(fd);

    return 0;
}

void backup_db(const char *dbfile)
{
    int fd = open(".", O_RDONLY);
    chdir(global_working_directory);
    char *backup_file;
    asprintf(&backup_file, "%s.corrupted", dbfile);
    unlink(backup_file);
    rename(dbfile, backup_file);
    free(backup_file);
    fchdir(fd);
    close(fd);
}

int open_database(DB **db, const char *dbfile, const char *dbname,
        int type, int flags)
{
    return_val_if_fail(db_create(db, get_default_db_environment(), 0) == 0, -1);
    return_val_if_fail(*db, -1);

    if(flags)
        (*db)->set_flags(*db, flags);

    int rc = (*db)->open(*db, NULL, dbfile, dbname, type, DB_CREATE, 0644);
    if(rc != 0)
    {
        g_warning("Failed to open %s database in %s: %s", dbname,
                dbfile, db_strerror(rc));
        (*db)->close(*db, 0);
        *db = NULL;
        return -1;
    }

    return 0;
}

int close_db(DB **db, const char *dbname)
{
    int rc = 0;

    if(db && *db)
    {
        g_message("closing %s database", dbname);
        rc = (*db)->close(*db, 0);
        if(rc == -1)
        {
            g_warning("Failed to close %s database: %s",
                    dbname, db_strerror(rc));
        }
        *db = NULL;
    }

    return rc == 0 ? 0 : -1;
}

