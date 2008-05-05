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

int db_transaction(DB_TXN **txn)
{
    return_val_if_fail(default_db_env, -1);
    return default_db_env->txn_begin(default_db_env, NULL, txn, 0);
}

void close_default_db_environment(void)
{
    if(default_db_env)
    {
        g_debug("closing default database environment");
        int rc = default_db_env->close(default_db_env, 0);
        if(rc == -1)
        {
            g_warning("Failed to close default database environment: %s",
                    db_strerror(rc));
        }
        default_db_env = NULL;
    }
}

static void db_err(const DB_ENV *dbenv, const char *errpfx,
        const char *msg)
{
    g_warning("DB: %s", msg);
}

static void db_msg(const DB_ENV *dbenv, const char *msg)
{
    g_message("DB: %s", msg);
}

int create_default_db_environment(void)
{
    return_val_if_fail(default_db_env == NULL, -1);

    g_message("creating default database environment, home = [%s]",
            global_working_directory);
    int rc = db_env_create(&default_db_env, 0);
    if(rc != 0)
    {
        g_warning("db_env_create: %s", db_strerror(rc));
        return -1;
    }

    default_db_env->set_errcall(default_db_env, db_err);
    default_db_env->set_msgcall(default_db_env, db_msg);

    return 0;
}

int open_default_db_environment(u_int32_t flags)
{
    return_val_if_fail(create_default_db_environment() == 0, -1);
    return_val_if_fail(default_db_env, -1);

    g_debug("opening database environment");
    int rc = default_db_env->open(default_db_env,
            global_working_directory, flags, 0);
    if(rc != 0)
    {
        g_warning("db_env->open: %s", db_strerror(rc));
        close_default_db_environment();
    }

    return rc;
}

static DB_ENV *get_default_db_environment(void)
{
    if(default_db_env == NULL)
    {
        u_int32_t flags = DB_CREATE | DB_INIT_TXN | DB_INIT_LOG |
            DB_INIT_MPOOL | DB_REGISTER | DB_RECOVER;

        if(open_default_db_environment(flags) == DB_RUNRECOVERY)
        {
	    flags &= ~DB_RECOVER;
	    flags |= DB_RECOVER_FATAL;
	    g_warning("running fatal recovery");
	    if(open_default_db_environment(flags) != 0)
	    {
		g_warning("giving up opening database environment");
		exit(2);
	    }
        }

        default_db_env->set_flags(default_db_env,
                DB_TXN_WRITE_NOSYNC | DB_LOG_AUTOREMOVE, 1);
    }

    return default_db_env;
}

int open_database(DB **db, const char *dbfile, const char *dbname,
        int type, int flags)
{
    return_val_if_fail(db_create(db,
                get_default_db_environment(), 0) == 0, -1);
    return_val_if_fail(*db, -1);

    if(flags)
        (*db)->set_flags(*db, flags);

    g_info("opening [%s] in [%s]", dbname, dbfile);
    int rc = (*db)->open(*db, NULL, dbfile, dbname, type, DB_CREATE | DB_AUTO_COMMIT, 0644);
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

