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

#ifndef _tthdb_h_
#define _tthdb_h_

/* The filesize and modification time are saved here so we can check if the
 * TTH is uptodate. */
struct tthdb_data
{
    uint64_t inode;
    uint64_t size;   /* filesize */ 
    time_t mtime;              /* modification time */
    unsigned leafdata_len;
    char leafdata[0];
};

typedef struct tth_inode tth_inode_t;
struct tth_inode
{
    uint64_t size;
    time_t mtime;
    char tth[41];
};

int tthdb_open(void);
int tthdb_close(void);

int tthdb_add(const char *tth, struct tthdb_data *tthdata);
int tthdb_update(const char *tth, struct tthdb_data *tthdata);
struct tthdb_data *tthdb_lookup(const char *tth);
int tthdb_remove(const char *tth);

struct tthdb_data *tthdb_lookup_by_inode(uint64_t inode);
char *tthdb_get_tth_by_inode(uint64_t inode);

int tthdb_remove_inode(uint64_t inode);
tth_inode_t *tthdb_lookup_inode(uint64_t inode);

#endif

