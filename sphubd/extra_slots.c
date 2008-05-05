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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "globals.h"
#include "log.h"
#include "hub.h"
#include "ui.h"

#include "extra_slots.h"
#include "notifications.h"

#define SLOTS_DB_FILENAME "slots2.db"

struct extra_slot
{
	LIST_ENTRY(extra_slot) link;

	char *nick;
	char *hub_address;
	int extra_slots;
};

static LIST_HEAD(, extra_slot) xs_head = LIST_HEAD_INITIALIZER(extra_slot);

static FILE *xs_fp = NULL;
static bool xs_loading = false;

static void xs_free(struct extra_slot *xs)
{
	if(xs)
	{
		LIST_REMOVE(xs, link);
		free(xs->nick);
		free(xs);
	}
}

struct extra_slot *xs_find(const char *nick)
{
	struct extra_slot *xs;
	LIST_FOREACH(xs, &xs_head, link)
	{
		if(strcmp(nick, xs->nick) == 0)
			return xs;
	}

	return NULL;
}

void xs_set(const char *nick, int extra_slots)
{
	return_if_fail(nick);
	return_if_fail(extra_slots >= 0);

	DEBUG("setting %i slots for [%s]", extra_slots, nick);

	struct extra_slot *xs = xs_find(nick);

	if(extra_slots == 0)
	{
		xs_free(xs);
	}
	else
	{
		if(xs == NULL)
		{
			xs = calloc(1, sizeof(struct extra_slot));
			xs->nick = strdup(nick);
			LIST_INSERT_HEAD(&xs_head, xs, link);
		}

		xs->extra_slots = extra_slots;
	}

	if(!xs_loading)
	{
		char *esc_nick = malloc(strlen(nick) * 2 + 1);
		char *ep = esc_nick;
		const char *np = nick;
		for(; *np; np++)
		{
			if(*np == ':' || *np == '\\')
				*ep++ = '\\';
			*ep++ = *np;
		}
		*ep = 0;
		fprintf(xs_fp, "%s:%i\n", esc_nick, extra_slots);
		free(esc_nick);
	}
}

static void extra_slots_parse(void)
{
	return_if_fail(xs_fp);

	rewind(xs_fp);

	unsigned line_number = 0;
	char *buf, *lbuf = NULL;
	size_t len;
	while((buf = fgetln(xs_fp, &len)) != NULL)
	{
		++line_number;

		if(buf[len - 1] == '\n')
			buf[len - 1] = 0;
		else
		{
			/* EOF without EOL, copy and add the NUL */
			lbuf = malloc(len + 1);
			assert(lbuf);
			memcpy(lbuf, buf, len);
			lbuf[len] = 0;
			buf = lbuf;
		}

		char *nick = strdup(buf);
		char *np = nick;

		char *p;
		for(p = buf; *p; p++)
		{
			if(*p == '\\')
			{
				if(++p == 0)
					break;
			}
			else if(*p == ':')
				break;
			*np++ = *p;
		}
		*np = 0;

		DEBUG("p = [%s]", p);

		if(*p != ':')
			goto failed;
		*p = 0; /* nul-terminate nick */

		size_t nick_len = p - buf;

		buf += nick_len + 1;

		char *endptr;
		int extra_slots = strtol(buf, &endptr, 10);
		if(extra_slots < 0 || endptr == NULL || *endptr != 0)
			goto failed;

		xs_set(nick, extra_slots);

		free(nick);
		continue;
failed:
		free(nick);
		WARNING("failed to parse line %u\n", line_number);

	}
	free(lbuf);

}

int extra_slots_init(void)
{
	DEBUG("opening database %s", SLOTS_DB_FILENAME);

	char *fname;
	asprintf(&fname, "%s/%s", global_working_directory, SLOTS_DB_FILENAME);
	xs_fp = fopen(fname, "a+");
	free(fname);

	if(xs_fp)
	{
		xs_loading = true;
		extra_slots_parse();
		xs_loading = false;
	}

	return 0;
}

int extra_slots_close(void)
{
	xs_loading = true; /* disable logging */

	struct extra_slot *xs;
	while((xs = LIST_FIRST(&xs_head)) != NULL)
		xs_free(xs);

	fclose(xs_fp);
	xs_fp = NULL;
	return 0;
}

static int extra_slots_lookup(const char *nick)
{
	struct extra_slot *xs = xs_find(nick);
	return xs ? xs->extra_slots : -1;
}

int extra_slots_grant(const char *nick, int delta)
{
	return_val_if_fail(xs_fp, -1);
	return_val_if_fail(nick, -1);
	return_val_if_fail(delta != 0, 0);

	struct extra_slot *xs = xs_find(nick);

	int extra_slots = 0;
	if(xs && xs->extra_slots > 0)
		extra_slots = xs->extra_slots;

	extra_slots += delta;
	return_val_if_fail(extra_slots >= 0, -1);

	xs_set(nick, extra_slots);
	nc_send_extra_slot_granted_notification(nc_default(), nick, extra_slots);

	return 0;
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
    fail_unless(extra_slots_grant("bla:bla", 6) == 0);
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
    fail_unless(extra_slots_get_for_user("bla:bla") == 6);

    /* setting extra slots to zero will remove the nick from the database */
    fail_unless(extra_slots_grant("bla:bla", -6) == 0);
    fail_unless(extra_slots_lookup("bla:bla") == -1);

    /* can't have negative amount of extra slots */
    fail_unless(extra_slots_grant("gazonk", 1) == 0);
    fail_unless(extra_slots_grant("gazonk", -2) == -1);
    fail_unless(extra_slots_get_for_user("gazonk") == 1);

    INFO("the following should fail");
    fail_unless(extra_slots_grant("bar", -1) == -1);

    extra_slots_close();
    system("/bin/rm -rf /tmp/sp-slots-test.d");

    return 0;
}

#endif

