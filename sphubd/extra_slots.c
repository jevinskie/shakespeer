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
#include <db.h>
#include <fcntl.h>

#include "globals.h"
#include "log.h"
#include "hub.h"
#include "ui.h"

#include "extra_slots.h"
#include "dbenv.h"

#define SLOTS_DB_FILENAME "slots.db"

static DB *slots_db = NULL;

int extra_slots_init(void)
{
    g_debug("opening database %s", SLOTS_DB_FILENAME);

    const char *db_list[] = {"slots", NULL};
    if(verify_db(SLOTS_DB_FILENAME, db_list) != 0)
    {
        g_warning("Corrupt slots database!");
        backup_db(SLOTS_DB_FILENAME);
    }
    else
        g_message("Slots database verified OK");

    /* open slots database */
    if(open_database(&slots_db, SLOTS_DB_FILENAME, "slots",
                DB_HASH, 0) != 0)
    {
        return -1;
    }

    return 0;
}

int extra_slots_close(void)
{
    return close_db(&slots_db, "slots");
}

static int extra_slots_lookup(const char *nick)
{
    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)nick;
    key.size = strlen(nick) + 1;

    DBT val;
    memset(&val, 0, sizeof(DBT));

    int rc = slots_db->get(slots_db, NULL, &key, &val, 0);
    if(rc == 0)
    {
        return *(int *)val.data;
    }
    else if(rc == DB_NOTFOUND)
    {
        return -1;
    }
    else
    {
        g_warning("failed: %s", db_strerror(rc));
        return -2;
    }
}

int extra_slots_grant(const char *nick, int delta)
{
    return_val_if_fail(slots_db, -1);
    return_val_if_fail(nick, -1);
    return_val_if_fail(delta != 0, 0);

    int current_extra_slots = extra_slots_lookup(nick);

    int extra_slots = 0;
    if(current_extra_slots > 0)
        extra_slots = current_extra_slots;

    extra_slots += delta;

    return_val_if_fail(extra_slots >= 0, -1);

    DBT val;
    memset(&val, 0, sizeof(DBT));
    val.data = &extra_slots;
    val.size = sizeof(unsigned);

    DBT key;
    memset(&key, 0, sizeof(DBT));
    key.data = (void *)nick;
    key.size = strlen(nick) + 1; /* store terminating nul */

    int rc;
    if(extra_slots == 0)
    {
        if(current_extra_slots >= 0)
            rc = slots_db->del(slots_db, NULL, &key, 0);
        else
            rc = 0;
    }
    else
        rc = slots_db->put(slots_db, NULL, &key, &val, 0);

    if(rc != 0)
    {
        g_warning("failed to add slots: %s", db_strerror(rc));
    }

#ifndef TEST
    hub_t *hub = hub_find_by_nick(nick);
    if(hub)
    {
        user_t *user = hub_lookup_user(hub, nick);
        if(user)
        {
            user->extra_slots = extra_slots;
            ui_send_user_update(NULL, hub->address, nick, user->description,
                    user->tag, user->speed, user->email, user->shared_size,
                    user->is_operator, user->extra_slots);
        }
    }
#endif

    return rc;
}

unsigned extra_slots_get_for_user(const char *nick)
{
    int s = extra_slots_lookup(nick);
    if(s < 0)
        s = 0;
    return s;
}


#ifdef TEST

#include <stdio.h>
#include "unit_test.h"

int main(void)
{
    sp_log_set_level("debug");
    global_working_directory = "/tmp/sp-slots-test.d";
    system("/bin/rm -rf /tmp/sp-slots-test.d");
    system("mkdir /tmp/sp-slots-test.d");

    fail_unless(extra_slots_init() == 0);

    fail_unless(extra_slots_get_for_user("foo") == 0);
    fail_unless(extra_slots_grant("foo", 2) == 0);
    fail_unless(extra_slots_grant("foo2", 3) == 0);
    fail_unless(extra_slots_grant("foo3", 3) == 0);
    fail_unless(extra_slots_grant("foo4", 3) == 0);
    fail_unless(extra_slots_grant("foo5", 1) == 0);
    fail_unless(extra_slots_grant("foo6", 5) == 0);
    fail_unless(extra_slots_grant("blabla", 6) == 0);
    fail_unless(extra_slots_get_for_user("foo") == 2);
    fail_unless(extra_slots_grant("foo", -1) == 0);
    fail_unless(extra_slots_get_for_user("foo") == 1);
    extra_slots_close();

    fail_unless(extra_slots_init() == 0);
    fail_unless(extra_slots_get_for_user("foo") == 1);
    fail_unless(extra_slots_get_for_user("foo2") == 3);
    fail_unless(extra_slots_get_for_user("foo3") == 3);
    fail_unless(extra_slots_get_for_user("foo4") == 3);
    fail_unless(extra_slots_get_for_user("foo5") == 1);
    fail_unless(extra_slots_get_for_user("foo6") == 5);
    fail_unless(extra_slots_get_for_user("blabla") == 6);

    /* setting extra slots to zero will remove the nick from the database */
    fail_unless(extra_slots_grant("blabla", -6) == 0);
    fail_unless(extra_slots_lookup("blabla") == -1);

    /* can't have negative amount of extra slots */
    fail_unless(extra_slots_grant("gazonk", 1) == 0);
    fail_unless(extra_slots_grant("gazonk", -2) == -1);
    fail_unless(extra_slots_get_for_user("gazonk") == 1);

    g_message("the following should fail");
    fail_unless(extra_slots_grant("bar", -1) == -1);

    extra_slots_close();
    system("/bin/rm -rf /tmp/sp-slots-test.d");

    return 0;
}

#endif

