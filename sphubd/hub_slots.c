/*
 * Copyright 2005-2006 Martin Hedenfalk <martin@bzero.se>
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

#include "hub.h"
#include "log.h"
#include "extra_slots.h"

static slot_t global_slots = { .total = 3, .used = 0 };
slot_t default_slots = { .total = 3, .used = 0 };
static bool use_global_slots = false;

static slot_t *hub_slots(hub_t *hub)
{
    if(use_global_slots)
	return &global_slots;
    return &hub->slots;
}

void hub_free_upload_slot(hub_t *hub, const char *nick, slot_state_t slot_state)
{
    slot_t *slots = hub_slots(hub);

    switch(slot_state)
    {
        case SLOT_EXTRA:
            g_info("removing extra upload slot for nick %s", nick);
            extra_slots_grant(nick, -1);
            break;
        case SLOT_NORMAL:
	    slots->used--;

	    if(slots->used < 0)
	    {
		g_warning("INTERNAL ERROR: slots->used < 0");
		slots->used = 0;
	    }

	    g_info("freeing one upload slot for nick %s: %d used, %d free",
		    nick, slots->used, slots->total - slots->used);
	    hub_set_need_myinfo_update(true);
            break;
        case SLOT_FREE:
        case SLOT_NONE:
        default:
            break;
    }
}

/* Returns 0 if a normal slot is available and was allocated. Returns 1 if a
 * free slot was granted (either for a filelist, small file or an extra granted
 * slot). Returns -1 if no slot was available.
 */
slot_state_t hub_request_upload_slot(hub_t *hub, const char *nick,
        const char *filename, uint64_t size)
{
    if(filename == NULL || is_filelist(filename) || size < 64*1024)
    {
        g_info("allowing free upload slot for file %s", filename);
        return SLOT_FREE;
    }

    if(extra_slots_get_for_user(nick) > 0)
    {
        g_info("allowing extra upload slot for nick %s", nick);
        return SLOT_EXTRA;
    }

    slot_t *slots = hub_slots(hub);
    if(slots->used >= slots->total)
    {
	g_info("no free slots left");
	return SLOT_NONE;
    }

    slots->used++;
    g_info("allocating one upload slot for file %s: %d used, %d free",
	    filename, slots->used, slots->total - slots->used);
    hub_set_need_myinfo_update(true);

    return SLOT_NORMAL;
}

static void hub_set_slots(hub_t *hub, int slots)
{
    return_if_fail(hub);
    return_if_fail(slots > 0);

    g_debug("setting slots = %d for hub [%s]", slots, hub->address);
    hub->slots.total = slots;
    hub_set_need_myinfo_update(true);
}

static void hub_set_slots_GFunc(hub_t *hub, void *user_data)
{
    return_if_fail(hub);
    int slots = *(int *)user_data;

    hub_set_slots(hub, slots);
}

void hub_all_set_slots(int slots)
{
    return_if_fail(slots > 0);

    hub_foreach(hub_set_slots_GFunc, (void *)&slots);

    /* set default value for newly created hubs */
    default_slots.total = slots;
    use_global_slots = false;
}

void hub_set_slots_global(int slots)
{
    g_info("setting slots = 2 globally", slots);
    global_slots.total = slots;
    use_global_slots = true;
    hub_set_need_myinfo_update(true);
}

/* Returns number of free slots currently available. */
int hub_slots_free(hub_t *hub)
{
    slot_t *slots = hub_slots(hub);
    int d = slots->total - slots->used;
    /* we might have total - used < 0 if we've lowered the total slots */
    if(d < 0)
	d = 0;
    return d;
}

/* Returns total number of slots available. */
int hub_slots_total(hub_t *hub)
{
    if(use_global_slots)
	return global_slots.total;
    return hub->slots.total;
}

/* initialize a slot_t for a new hub */
void hub_slots_init(slot_t *slots)
{
    if(!use_global_slots)
    {
	slots->total = default_slots.total;
	slots->used = 0;
    }
}

#ifdef TEST

#include "unit_test.h"
#include "globals.h"

void check_slots(hub_t *ahub)
{
    /* we haven't used any slots yet */
    printf("hub_slots_free = %d\n", hub_slots_free(ahub));
    fail_unless(hub_slots_free(ahub) == 2);

    /* request a normal slot */
    slot_state_t ss;
    ss = hub_request_upload_slot(ahub, "nicke", "filename", 123456);
    fail_unless(ss == SLOT_NORMAL);

    /* make sure we've used one, free it and verify we're back */
    fail_unless(hub_slots_free(ahub) == 1);
    hub_free_upload_slot(ahub, "nicke", ss);
    fail_unless(hub_slots_free(ahub) == 2);

    /* use all available slots */
    ss = hub_request_upload_slot(ahub, "nicke", "filename", 123456);
    fail_unless(ss == SLOT_NORMAL);
    ss = hub_request_upload_slot(ahub, "nicke2", "filename2", 234567);
    fail_unless(ss == SLOT_NORMAL);
    fail_unless(hub_slots_free(ahub) == 0);
    ss = hub_request_upload_slot(ahub, "nicke3", "filename3", 345678);
    fail_unless(ss == SLOT_NONE);
    ss = hub_request_upload_slot(ahub, "nicke3", "filename3", 345678);
    fail_unless(ss == SLOT_NONE);

    /* back to all free slots */
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    hub_free_upload_slot(ahub, "nicke", SLOT_NONE);
    fail_unless(hub_slots_free(ahub) == 1);
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free(ahub) == 2);

    /* make sure we handle a possible inconsistency */
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free(ahub) == 2);

    /* check that we can get a free slot for small files */
    ss = hub_request_upload_slot(ahub, "nicke", "filename", 63*1024);
    fail_unless(ss == SLOT_FREE);
    fail_unless(hub_slots_free(ahub) == 2);

    /* check that we can get a free slot for filelists */
    ss = hub_request_upload_slot(ahub, "nicke", "files.xml.bz2", 67*1024);
    fail_unless(ss == SLOT_FREE);
    fail_unless(hub_slots_free(ahub) == 2);
    ss = hub_request_upload_slot(ahub, "nicke", NULL, 67*1024);
    fail_unless(ss == SLOT_FREE);
    fail_unless(hub_slots_free(ahub) == 2);

    /* check integration with extra-slots module */
    fail_unless(extra_slots_grant("nicke", 1) == 0);
    fail_unless(hub_slots_free(ahub) == 2);
    ss = hub_request_upload_slot(ahub, "nicke", "filename", 23423423);
    fail_unless(ss == SLOT_EXTRA);
    /* we should still have 2 slots available for everyone else */
    fail_unless(hub_slots_free(ahub) == 2);
    ss = hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    fail_unless(ss == SLOT_EXTRA);
    fail_unless(hub_slots_free(ahub) == 2);
    hub_free_upload_slot(ahub, "nicke", SLOT_EXTRA);
    fail_unless(hub_slots_free(ahub) == 2);
    /* now the extra slots should be gone */
    ss = hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    fail_unless(ss == SLOT_NORMAL);
    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free(ahub) == 2);

    /* increase total number of slots */
    if(use_global_slots)
	hub_set_slots_global(4);
    else
	hub_all_set_slots(4);

    /* use all 4 slots, then lower the available slots */
    hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    hub_request_upload_slot(ahub, "nicke", "filename2", 5432323);
    fail_unless(hub_slots_free(ahub) == 0);
    slot_t *slots = hub_slots(ahub);
    fail_unless(slots->used == 4);

    /* ...and lower the number of available slots */
    if(use_global_slots)
	hub_set_slots_global(2);
    else
	hub_all_set_slots(2);
    fail_unless(hub_slots_free(ahub) == 0);
    fail_unless(slots->used == 4);

    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free(ahub) == 0);
    fail_unless(slots->used == 3);

    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free(ahub) == 0);
    fail_unless(slots->used == 2);

    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free(ahub) == 1);
    fail_unless(slots->used == 1);

    hub_free_upload_slot(ahub, "nicke", SLOT_NORMAL);
    fail_unless(hub_slots_free(ahub) == 2);
    fail_unless(slots->used == 0);
}

int main(void)
{
    sp_log_set_level("debug");

    global_working_directory = "/tmp/sp-slots-test.d";
    system("/bin/rm -rf /tmp/sp-slots-test.d");
    system("mkdir /tmp/sp-slots-test.d");

    fail_unless(extra_slots_init() == 0);

    hub_list_init();
    hub_t *ahub = hub_new();
    fail_unless(ahub);
    ahub->address = strdup("ahub");
    hub_list_add(ahub);
    fail_unless(ahub->slots.total == 3);
    fail_unless(ahub->slots.used == 0);

    /* set 2 slots per hub */
    hub_all_set_slots(2);
    fail_unless(ahub->slots.total == 2);
    fail_unless(ahub->slots.used == 0);
    fail_unless(use_global_slots == false);

    check_slots(ahub);

    /* set 2 slots globally */
    hub_set_slots_global(2);
    fail_unless(use_global_slots == true);

    check_slots(ahub);

    /* set 5 slots per hub */
    hub_all_set_slots(5);

    /* add a new hub and make sure we get the new default slots value */
    hub_t *bhub = hub_new();
    fail_unless(bhub);
    bhub->address = strdup("bhub");
    fail_unless(bhub->slots.total == 5);
    fail_unless(bhub->slots.used == 0);

    hub_list_add(bhub);

    /* decrease per-hub slot settings and check that total slots decreases */
    hub_all_set_slots(4);
    fail_unless(ahub->slots.total == 4);
    fail_unless(ahub->slots.used == 0);
    fail_unless(bhub->slots.total == 4);
    fail_unless(bhub->slots.used == 0);

    extra_slots_close();
    system("/bin/rm -rf /tmp/sp-slots-test.d");

    return 0;
}

#endif

