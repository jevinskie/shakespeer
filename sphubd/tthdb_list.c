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

int main(int argc, char **argv)
{
    if(argc > 1)
        global_working_directory = argv[1];
    else
        global_working_directory = "/tmp";

    sp_log_set_level("debug");

    tth_store_init();

    uint64_t leafdata_size = 0;
    unsigned ntths = 0;

    struct tth_entry *te;
    RB_FOREACH(te, tth_entries_head,
	&((struct tth_store *)global_tth_store)->entries)
    {
	printf("%s:", te->tth);

        printf(" inode=%016llX", te->active_inode);

	struct tth_inode *ti = tth_store_lookup_inode(global_tth_store,
	    te->active_inode);


        printf(" mtime=%08lX", (unsigned long)ti->mtime);

	unsigned prev_leafdata_len = te->leafdata_len;
	int rc = tth_store_load_leafdata(global_tth_store, te);
	if(rc == 0)
	{
		if(te->leafdata_len != prev_leafdata_len)
			printf(" LEAFDATA CHANGED");
		printf(" leafdata size=%u\n", te->leafdata_len);
	}

	leafdata_size += te->leafdata_len;
	ntths++;
    }

    printf("\n%u tths, average leafdata size: %llu\n",
	ntths, leafdata_size / ntths);

    tth_store_close();

    return 0;
}

