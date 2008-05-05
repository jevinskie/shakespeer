/*
 * Copyright 2005 Martin Hedenfalk <martin@bzero.se>
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

int hub_default_slots = 3;
int hub_global_slots = -1; /* -1 = not enabled, use per-hub slots settings */

void hub_free_upload_slot(hub_t *hub, const char *nick, slot_state_t slot_state)
{
    switch(slot_state)
    {
        case SLOT_EXTRA:
            g_info("removing extra upload slot for nick %s", nick);
            extra_slots_grant(nick, -1);
            break;
        case SLOT_NORMAL:
            {
                int slots_left = -1;
                if(hub_global_slots == -1)
                    slots_left = ++hub->open_slots;
                else
                    slots_left = ++hub_global_slots;
                g_info("freeing one upload slot for nick %s, %d slots left",
                        nick, slots_left);
                hub_set_need_myinfo_update(1);
            }
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
        const char *filename, unsigned long long int size)
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

    if(hub->open_slots)
    {
        int slots_left = -1;
        if(hub_global_slots == -1)
            slots_left = --hub->open_slots;
        else
            slots_left = --hub_global_slots;

        g_info("allocating one upload slot for file %s, %d slots left",
                filename, slots_left);
        hub_set_need_myinfo_update(1);

        return SLOT_NORMAL;
    }

    return SLOT_NONE;
}

void hub_set_slots(hub_t *hub, int slots)
{
    if(slots >= 0)
    {
        hub->slots = slots;
        hub_set_need_myinfo_update(1);
    }
    else
    {
        g_warning("attempt to set negative or zero number of slots (%d)", slots);
    }
}

static void hub_set_slots_GFunc(hub_t *hub, void *user_data)
{
    return_if_fail(hub);
    int slots = *(int *)user_data;

    hub_set_slots(hub, slots);
}

void hub_all_set_slots(int slots)
{
    hub_foreach(hub_set_slots_GFunc, (void *)&slots);

    if(slots >= 0)
    {
        /* set default value for newly created hubs */
        hub_default_slots = slots;
    }
}

void hub_set_slots_global(int slots)
{
    hub_global_slots = slots;
    hub_set_need_myinfo_update(1);
}

static void hub_count_slots_GFunc(hub_t *hub, void *user_data)
{
    int *tot_slots = user_data;

    *tot_slots += hub->slots;
}

int hub_count_slots(void)
{
    int tot_slots = 0;
    if(hub_global_slots == -1)
    {
        hub_foreach(hub_count_slots_GFunc, &tot_slots);
    }
    else
    {
        tot_slots = hub_global_slots;
    }

    return tot_slots;
}

